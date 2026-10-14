// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// DPI-C trace writer. Used by the core tracers when the RTL is built with
// SNITCH_TRACE_GZ (see hw/ip/snitch/include/snitch/trace_writer.svh).
//
// Backends, selected by the `mode` string passed to tw_open and overridable
// at run time with +trace_mode=<mode> or the TW_MODE environment variable
// (TW_MODE wins; use it where a run wrapper cannot forward a plusarg):
//
//   "null"       discard; measures the $sformat cost alone
//   "plain"      stdio fwrite into a 1 MiB buffer (the pre-DPI behaviour)
//   "gz<N>"      zlib gzwrite at level N, deflating on the caller's thread
//   "thr:gz<N>"  producer/consumer: tw_write only memcpys into a block and a
//                worker thread deflates it. The default and the only backend
//                that is cheaper per call than plain $fwrite.
//   "thr:plain"  same handoff without compression
//
// Do not use inline "gz<N>": zlib then deflates a whole buffer on the
// simulator thread every few hundred lines, stalling it for ~1 ms.
//
// Tuning: TW_BLKSZ (bytes per block) and TW_NBLK (blocks in flight) size the
// threaded backend's ring; the default 256 KiB x 2 costs 512 KiB per traced
// hart. They are not interchangeable. TW_BLKSZ is an efficiency floor: at
// 256 KiB the worker does one gzwrite per ~430 lines, and shrinking it to
// 32 KiB costs a rotation and a zlib call often enough to push mean per-write
// from 9 ns to 1334 ns. TW_NBLK is the burst budget: two is the minimum that
// lets the producer fill one block while the worker drains the other, and on
// real traces it measures no worse than four, which only pays off when a burst
// arrives faster than zlib drains. Raise it if a workload traces in bursts.
// TW_GZBUF sizes zlib's output buffer (default 64 KiB) and is the knob that
// governs how much trace is lost if the simulation is killed - the ring is not.
// TW_LAT=1 records every tw_write's duration and prints percentiles at tw_close.
//
// The worker thread assumes the simulation has a spare core. Under a
// single-core allocation it contends with the simulator and the margin over
// inline zlib narrows.
//
// The number of concurrent tracers is not capped; the slot table grows on
// demand. What does scale with hart count is one worker thread and one ring
// per tracer - 512 KiB each by default, so 256 harts hold 128 MB. Trim that
// with TW_NBLK rather than by shrinking TW_BLKSZ.
//
// Compiled as C but pulled in by Verilator's g++, hence the extern "C" guard.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <zlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TW_SLOT0  16        /* initial tracer slots; the table grows as needed */
#define TW_BUFSZ  (1 << 20)
#define TW_MAXBLK 8

typedef enum { TW_NULL, TW_PLAIN, TW_GZ, TW_THR } tw_kind_t;

/* ---------- latency instrumentation (TW_LAT=1) ---------- */

static double tw_tsc_hz  = 0.0;   /* calibrated once */
static double tw_tsc_ovh = 0.0;   /* back-to-back read cost, in ticks */

static double tw_wall(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

#if defined(__x86_64__) || defined(__i386__)
static inline unsigned long long tw_tick(void) {
  unsigned lo, hi;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi) :: "memory");
  return ((unsigned long long)hi << 32) | lo;
}
#else
/* No cycle counter: fall back to the clock, already in ns. */
static inline unsigned long long tw_tick(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long long)ts.tv_sec * 1000000000ULL + (unsigned long long)ts.tv_nsec;
}
#endif

static void tw_calibrate(void) {
  double t0, t1;
  unsigned long long c0, c1, best = ~0ULL;
  int i;
  if (tw_tsc_hz > 0.0) return;
  t0 = tw_wall(); c0 = tw_tick();
  while (tw_wall() - t0 < 0.05) { }
  c1 = tw_tick(); t1 = tw_wall();
  tw_tsc_hz = (double)(c1 - c0) / (t1 - t0);
  for (i = 0; i < 2000; i++) {          /* empty-pair floor */
    unsigned long long a = tw_tick(), b = tw_tick();
    if (b - a < best) best = b - a;
  }
  tw_tsc_ovh = (double)best;
}

typedef struct {
  unsigned *v;        /* per-call tick counts */
  size_t    n, cap;
} tw_lat_t;

static int tw_lat_on = -1;

static int tw_cmp_u(const void *a, const void *b) {
  unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;
  return (x > y) - (x < y);
}

static double tw_ns(double ticks) {
  double c = ticks - tw_tsc_ovh;
  if (c < 0) c = 0;
  return c * 1e9 / tw_tsc_hz;
}

static void tw_lat_report(const char *mode, tw_lat_t *L) {
  static const double pct[] = {50, 90, 99, 99.9, 99.99};
  double sum = 0;
  size_t i;
  unsigned k;
  if (!L->v || !L->n) return;
  for (i = 0; i < L->n; i++) sum += L->v[i];
  qsort(L->v, L->n, sizeof(unsigned), tw_cmp_u);
  printf("[tw] mode=%s calls=%zu mean_ns=%.0f", mode, L->n, tw_ns(sum / (double)L->n));
  for (k = 0; k < sizeof pct / sizeof *pct; k++) {
    size_t j = (size_t)((pct[k] / 100.0) * (double)L->n);
    if (j >= L->n) j = L->n - 1;
    printf(" p%g_ns=%.0f", pct[k], tw_ns((double)L->v[j]));
  }
  printf(" max_ns=%.0f\n", tw_ns((double)L->v[L->n - 1]));
  fflush(stdout);
}

/* ---------- threaded backend ---------- */

/* zlib's own output buffer, which dominates how much is lost if the simulator
   is killed: it holds compressed bytes, so at ~24x each buffered byte stands
   for ~24 bytes of trace. 64 KiB keeps that under a megabyte at no measured
   cost in latency or compression, since the worker thread absorbs the extra
   write() calls off the simulator's critical path. */
static unsigned tw_gzbuf(void) {
  const char *e = getenv("TW_GZBUF");
  int v = e ? atoi(e) : 0;
  if (v < 4096) v = 64 * 1024;
  return (unsigned)v;
}

typedef struct {
  char  *blk[TW_MAXBLK];
  size_t fill[TW_MAXBLK];
  int    full[TW_MAXBLK], nfull, fhead, ftail;   /* queue of full blocks */
  int    free_[TW_MAXBLK], nfree, rhead, rtail;  /* queue of free blocks */
  int    cur; size_t curfill;
  size_t blksz; int nblk;
  pthread_t th;
  pthread_mutex_t m;
  pthread_cond_t cv_full, cv_free;
  int stop;
  gzFile gz; FILE *fp;
} tw_thr_t;

typedef struct {
  tw_kind_t kind;
  FILE     *fp;
  gzFile    gz;
  char     *buf;
  tw_thr_t *thr;
  tw_lat_t  lat;
  char      mode[64];
  int       in_use;
} tw_slot_t;

/* Grown on demand so the number of traced harts is bounded only by memory.
   No pointer into this table outlives a single call - every tw_slot_t * is
   re-derived from the handle - so reallocating it here is safe. The worker
   thread only ever holds its own tw_thr_t, which is allocated separately. */
static tw_slot_t *tw_slots;
static int        tw_nslots;

static void *tw_worker(void *arg) {
  tw_thr_t *T = (tw_thr_t *)arg;
  for (;;) {
    int idx;
    pthread_mutex_lock(&T->m);
    while (!T->nfull && !T->stop) pthread_cond_wait(&T->cv_full, &T->m);
    if (!T->nfull && T->stop) { pthread_mutex_unlock(&T->m); break; }
    idx = T->full[T->fhead]; T->fhead = (T->fhead + 1) % T->nblk; T->nfull--;
    pthread_mutex_unlock(&T->m);

    if (T->gz) gzwrite(T->gz, T->blk[idx], (unsigned)T->fill[idx]);
    else       fwrite(T->blk[idx], 1, T->fill[idx], T->fp);

    pthread_mutex_lock(&T->m);
    T->free_[T->rtail] = idx; T->rtail = (T->rtail + 1) % T->nblk; T->nfree++;
    pthread_cond_signal(&T->cv_free);
    pthread_mutex_unlock(&T->m);
  }
  return NULL;
}

/* Hand the current block to the worker and take a fresh one. Blocks only when
   every block is still in flight, i.e. when the trace rate outruns zlib. */
static void tw_thr_rotate(tw_thr_t *T) {
  pthread_mutex_lock(&T->m);
  T->fill[T->cur] = T->curfill;
  T->full[T->ftail] = T->cur; T->ftail = (T->ftail + 1) % T->nblk; T->nfull++;
  pthread_cond_signal(&T->cv_full);
  while (!T->nfree) pthread_cond_wait(&T->cv_free, &T->m);
  T->cur = T->free_[T->rhead]; T->rhead = (T->rhead + 1) % T->nblk; T->nfree--;
  pthread_mutex_unlock(&T->m);
  T->curfill = 0;
}

static tw_thr_t *tw_thr_open(const char *path, int level) {
  const char *e;
  int i;
  tw_thr_t *T = (tw_thr_t *)calloc(1, sizeof *T);
  if (!T) return NULL;
  T->blksz = (e = getenv("TW_BLKSZ")) ? (size_t)atol(e) : (256u << 10);
  T->nblk  = (e = getenv("TW_NBLK"))  ? atoi(e) : 2;
  if (T->blksz < (4u << 10)) T->blksz = 4u << 10;
  if (T->nblk < 2) T->nblk = 2;
  if (T->nblk > TW_MAXBLK) T->nblk = TW_MAXBLK;

  if (level > 0) {
    char m[16]; snprintf(m, sizeof m, "wb%d", level);
    T->gz = gzopen(path, m);
    if (!T->gz) { free(T); return NULL; }
    gzbuffer(T->gz, tw_gzbuf());
  } else {
    T->fp = fopen(path, "wb");
    if (!T->fp) { free(T); return NULL; }
    setvbuf(T->fp, NULL, _IOFBF, TW_BUFSZ);
  }
  for (i = 0; i < T->nblk; i++) {
    T->blk[i] = (char *)malloc(T->blksz);
    if (!T->blk[i]) { /* shrink the ring rather than fail outright */
      T->nblk = (i < 2) ? 0 : i;
      break;
    }
  }
  if (T->nblk < 2) {
    if (T->gz) gzclose(T->gz); else fclose(T->fp);
    for (i = 0; i < TW_MAXBLK; i++) free(T->blk[i]);
    free(T);
    return NULL;
  }
  T->cur = 0; T->curfill = 0;
  for (i = 1; i < T->nblk; i++) { T->free_[T->rtail++] = i; T->nfree++; }
  T->rtail %= T->nblk;
  pthread_mutex_init(&T->m, NULL);
  pthread_cond_init(&T->cv_full, NULL);
  pthread_cond_init(&T->cv_free, NULL);
  if (pthread_create(&T->th, NULL, tw_worker, T) != 0) {
    if (T->gz) gzclose(T->gz); else fclose(T->fp);
    for (i = 0; i < T->nblk; i++) free(T->blk[i]);
    free(T);
    return NULL;
  }
  return T;
}

static void tw_thr_close(tw_thr_t *T) {
  int i;
  if (T->curfill) tw_thr_rotate(T);
  pthread_mutex_lock(&T->m);
  T->stop = 1;
  pthread_cond_signal(&T->cv_full);
  pthread_mutex_unlock(&T->m);
  pthread_join(T->th, NULL);
  if (T->gz) gzclose(T->gz); else fclose(T->fp);
  for (i = 0; i < T->nblk; i++) free(T->blk[i]);
  pthread_mutex_destroy(&T->m);
  pthread_cond_destroy(&T->cv_full);
  pthread_cond_destroy(&T->cv_free);
  free(T);
}

/* ---------- DPI surface ---------- */

void tw_close(int h);

static int tw_alloc(void) {
  int i, n;
  tw_slot_t *grown;
  for (i = 0; i < tw_nslots; i++)
    if (!tw_slots[i].in_use) {
      memset(&tw_slots[i], 0, sizeof(tw_slot_t));
      tw_slots[i].in_use = 1;
      return i;
    }
  n = tw_nslots ? tw_nslots * 2 : TW_SLOT0;
  grown = (tw_slot_t *)realloc(tw_slots, (size_t)n * sizeof(tw_slot_t));
  if (!grown) return -1;
  tw_slots = grown;
  memset(&tw_slots[tw_nslots], 0, (size_t)(n - tw_nslots) * sizeof(tw_slot_t));
  i = tw_nslots;
  tw_nslots = n;
  tw_slots[i].in_use = 1;
  return i;
}

/* File suffix matching a mode, so the name on disk always describes the bytes
   in it. Only the gz-producing backends get ".gz". */
const char *tw_ext(const char *mode) {
  const char *env_mode = getenv("TW_MODE");
  if (env_mode && *env_mode) mode = env_mode;   /* same override as tw_open */
  if (!strncmp(mode, "thr:gz", 6)) return "dasm.gz";
  if (mode[0] == 'g' && mode[1] == 'z') return "dasm.gz";
  return "dasm";
}

/* Verilator does not run SystemVerilog `final` blocks, so tw_close is never
   reached there and a gzip stream would be left unterminated - the plain
   backend survives only because the C runtime flushes stdio at exit. Closing
   every live tracer from atexit covers that. tw_close clears in_use, so a
   simulator that *does* run `final` (Questa, VCS) simply finds nothing left. */
static void tw_atexit(void) {
  int i;
  for (i = 0; i < tw_nslots; i++)
    if (tw_slots[i].in_use) tw_close(i);
}

int tw_open(const char *path, const char *mode) {
  static int tw_atexit_armed = 0;
  int h = tw_alloc();
  tw_slot_t *s;
  const char *env_mode = getenv("TW_MODE");
  /* Some run wrappers take a single positional argument and cannot forward a
     +trace_mode plusarg, so TW_MODE offers the same choice through the
     environment and wins when both are given. */
  if (env_mode && *env_mode) mode = env_mode;
  if (h < 0) { fprintf(stderr, "[tw] cannot allocate a tracer slot for %s\n", path); return -1; }
  s = &tw_slots[h];
  snprintf(s->mode, sizeof s->mode, "%s", mode);
  if (!tw_atexit_armed) { atexit(tw_atexit); tw_atexit_armed = 1; }

  if (tw_lat_on < 0) { const char *e = getenv("TW_LAT"); tw_lat_on = e && *e == '1'; }
  if (tw_lat_on) {
    tw_calibrate();
    s->lat.cap = 1u << 20;
    s->lat.v = (unsigned *)malloc(s->lat.cap * sizeof(unsigned));
    if (!s->lat.v) s->lat.cap = 0;
  }

  if (!strcmp(mode, "null")) {
    s->kind = TW_NULL;
  } else if (!strcmp(mode, "plain")) {
    s->kind = TW_PLAIN;
    s->fp = fopen(path, "wb");
    if (!s->fp) { perror("[tw] fopen"); s->in_use = 0; return -1; }
    s->buf = (char *)malloc(TW_BUFSZ);
    if (s->buf) setvbuf(s->fp, s->buf, _IOFBF, TW_BUFSZ);
  } else if (!strncmp(mode, "thr:", 4)) {
    int lvl = 0;
    if (!strncmp(mode + 4, "gz", 2)) { lvl = atoi(mode + 6); if (lvl < 1 || lvl > 9) lvl = 6; }
    s->kind = TW_THR;
    s->thr = tw_thr_open(path, lvl);
    if (!s->thr) { fprintf(stderr, "[tw] cannot open %s\n", path); s->in_use = 0; return -1; }
  } else if (mode[0] == 'g' && mode[1] == 'z') {
    char m[16];
    int lvl = atoi(mode + 2);
    if (lvl < 1 || lvl > 9) lvl = 6;
    s->kind = TW_GZ;
    snprintf(m, sizeof m, "wb%d", lvl);
    s->gz = gzopen(path, m);
    if (!s->gz) { fprintf(stderr, "[tw] gzopen %s failed\n", path); s->in_use = 0; return -1; }
    gzbuffer(s->gz, tw_gzbuf());
  } else {
    fprintf(stderr, "[tw] unknown trace_mode '%s'\n", mode);
    s->in_use = 0;
    return -1;
  }
  return h;
}

void tw_write(int h, const char *str) {
  tw_slot_t *s;
  size_t n;
  unsigned long long c0 = 0;
  if (h < 0 || h >= tw_nslots) return;
  s = &tw_slots[h];
  if (!s->in_use) return;
  n = strlen(str);
  if (tw_lat_on) c0 = tw_tick();

  switch (s->kind) {
    case TW_NULL:  break;
    case TW_PLAIN: fwrite(str, 1, n, s->fp); break;
    case TW_GZ:    gzwrite(s->gz, str, (unsigned)n); break;
    case TW_THR: {
      tw_thr_t *T = s->thr;
      size_t off = 0;
      /* A line longer than a block is split across blocks; gzip does not care
         about record boundaries and the reader sees the same byte stream. */
      while (off < n) {
        size_t room = T->blksz - T->curfill;
        size_t take = (n - off < room) ? (n - off) : room;
        if (!room) { tw_thr_rotate(T); continue; }
        memcpy(T->blk[T->cur] + T->curfill, str + off, take);
        T->curfill += take;
        off += take;
      }
      break;
    }
  }

  if (tw_lat_on && s->lat.v) {
    unsigned long long d = tw_tick() - c0;
    if (s->lat.n == s->lat.cap) {
      unsigned *nv = (unsigned *)realloc(s->lat.v, s->lat.cap * 2 * sizeof(unsigned));
      if (nv) { s->lat.v = nv; s->lat.cap *= 2; }
      else return;
    }
    s->lat.v[s->lat.n++] = (d > 0xffffffffULL) ? 0xffffffffu : (unsigned)d;
  }
}

void tw_close(int h) {
  tw_slot_t *s;
  if (h < 0 || h >= tw_nslots) return;
  s = &tw_slots[h];
  if (!s->in_use) return;
  switch (s->kind) {
    case TW_NULL:  break;
    case TW_PLAIN: fclose(s->fp); break;
    case TW_GZ:    gzclose(s->gz); break;
    case TW_THR:   tw_thr_close(s->thr); break;
  }
  if (tw_lat_on) tw_lat_report(s->mode, &s->lat);
  free(s->lat.v);
  free(s->buf);
  s->in_use = 0;
}

#ifdef __cplusplus
}
#endif
