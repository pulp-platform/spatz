
/scratch2/amirkia/spatz-github/spatz/hw/system/spatz_cluster/sw/build/LLM/test-LLMTests-train_gpt2-FP16:	file format elf32-littleriscv

Sections:
Idx Name               Size     VMA      Type
  0                    00000000 00000000 
  1 .text              000031e8 80000000 TEXT
  2 .rodata            0000033c 800031e8 DATA
  3 .eh_frame          0000002c 80003524 DATA
  4 .htif              00000048 80003580 DATA
  5 .got               0000000c 800035c8 DATA
  6 .tdata             00000000 800035d4 DATA
  7 .tbss              0000000c 800035d4 BSS
  8 .tbssend           00000000 800035e0 DATA
  9 .sdata             000000b0 800035e0 DATA
 10 .data              0000a000 80003690 DATA
 11 .sbss              00000004 8000d690 BSS
 12 .bss               00000000 8000d694 BSS
 13 .dram              00000000 8000d694 DATA
 14 .debug_loclists    00005004 00000000 DEBUG
 15 .debug_abbrev      000011fe 00000000 DEBUG
 16 .debug_info        00006633 00000000 DEBUG
 17 .debug_rnglists    00000821 00000000 DEBUG
 18 .debug_str_offsets 00000ab8 00000000 DEBUG
 19 .debug_str         00001d8e 00000000 DEBUG
 20 .debug_addr        0000040c 00000000 DEBUG
 21 .comment           0000009c 00000000 
 22 .riscv.attributes  000001e3 00000000 
 23 .debug_frame       000009e8 00000000 DEBUG
 24 .debug_line        0000905c 00000000 DEBUG
 25 .debug_line_str    00000847 00000000 DEBUG
 26 .Pulp_Chip.Info    0000030c 00000000 
 27 .debug_aranges     00000078 00000000 DEBUG
 28 .debug_loc         00000bf5 00000000 DEBUG
 29 .debug_ranges      000000f0 00000000 DEBUG
 30 .symtab            0000ac20 00000000 
 31 .shstrtab          00000152 00000000 
 32 .strtab            000011f1 00000000 

Disassembly of section .text:

80000000 <snrt.crt0.init_fp_registers>:
;     csrr    t0, misa
80000000: f3 22 10 30  	csrr	t0, misa
;     andi    t0, t0, (1 << 3) | (1 << 5) # D/F - single/double precision float extension
80000004: 93 f2 82 02  	andi	t0, t0, 40
;     beqz    t0, 3f
80000008: 63 82 02 08  	beqz	t0, 0x8000008c <snrt.crt0.init_global_pointer>
;     fcvt.d.w f0, zero
8000000c: 53 00 00 d2  	fcvt.d.w	ft0, zero
;     fcvt.d.w f1, zero
80000010: d3 00 00 d2  	fcvt.d.w	ft1, zero
;     fcvt.d.w f2, zero
80000014: 53 01 00 d2  	fcvt.d.w	ft2, zero
;     fcvt.d.w f3, zero
80000018: d3 01 00 d2  	fcvt.d.w	ft3, zero
;     fcvt.d.w f4, zero
8000001c: 53 02 00 d2  	fcvt.d.w	ft4, zero
;     fcvt.d.w f5, zero
80000020: d3 02 00 d2  	fcvt.d.w	ft5, zero
;     fcvt.d.w f6, zero
80000024: 53 03 00 d2  	fcvt.d.w	ft6, zero
;     fcvt.d.w f7, zero
80000028: d3 03 00 d2  	fcvt.d.w	ft7, zero
;     fcvt.d.w f8, zero
8000002c: 53 04 00 d2  	fcvt.d.w	fs0, zero
;     fcvt.d.w f9, zero
80000030: d3 04 00 d2  	fcvt.d.w	fs1, zero
;     fcvt.d.w f10, zero
80000034: 53 05 00 d2  	fcvt.d.w	fa0, zero
;     fcvt.d.w f11, zero
80000038: d3 05 00 d2  	fcvt.d.w	fa1, zero
;     fcvt.d.w f12, zero
8000003c: 53 06 00 d2  	fcvt.d.w	fa2, zero
;     fcvt.d.w f13, zero
80000040: d3 06 00 d2  	fcvt.d.w	fa3, zero
;     fcvt.d.w f14, zero
80000044: 53 07 00 d2  	fcvt.d.w	fa4, zero
;     fcvt.d.w f15, zero
80000048: d3 07 00 d2  	fcvt.d.w	fa5, zero
;     fcvt.d.w f16, zero
8000004c: 53 08 00 d2  	fcvt.d.w	fa6, zero
;     fcvt.d.w f17, zero
80000050: d3 08 00 d2  	fcvt.d.w	fa7, zero
;     fcvt.d.w f18, zero
80000054: 53 09 00 d2  	fcvt.d.w	fs2, zero
;     fcvt.d.w f19, zero
80000058: d3 09 00 d2  	fcvt.d.w	fs3, zero
;     fcvt.d.w f20, zero
8000005c: 53 0a 00 d2  	fcvt.d.w	fs4, zero
;     fcvt.d.w f21, zero
80000060: d3 0a 00 d2  	fcvt.d.w	fs5, zero
;     fcvt.d.w f22, zero
80000064: 53 0b 00 d2  	fcvt.d.w	fs6, zero
;     fcvt.d.w f23, zero
80000068: d3 0b 00 d2  	fcvt.d.w	fs7, zero
;     fcvt.d.w f24, zero
8000006c: 53 0c 00 d2  	fcvt.d.w	fs8, zero
;     fcvt.d.w f25, zero
80000070: d3 0c 00 d2  	fcvt.d.w	fs9, zero
;     fcvt.d.w f26, zero
80000074: 53 0d 00 d2  	fcvt.d.w	fs10, zero
;     fcvt.d.w f27, zero
80000078: d3 0d 00 d2  	fcvt.d.w	fs11, zero
;     fcvt.d.w f28, zero
8000007c: 53 0e 00 d2  	fcvt.d.w	ft8, zero
;     fcvt.d.w f29, zero
80000080: d3 0e 00 d2  	fcvt.d.w	ft9, zero
;     fcvt.d.w f30, zero
80000084: 53 0f 00 d2  	fcvt.d.w	ft10, zero
;     fcvt.d.w f31, zero
80000088: d3 0f 00 d2  	fcvt.d.w	ft11, zero

8000008c <snrt.crt0.init_global_pointer>:
; 1:  auipc   gp, %pcrel_hi(__global_pointer$)
8000008c: 97 41 00 00  	auipc	gp, 4
;     addi    gp, gp, %pcrel_lo(1b)
80000090: 93 81 41 d4  	addi	gp, gp, -700

80000094 <snrt.crt0.init_core_info>:
;     mv a4, a1
80000094: 13 87 05 00  	mv	a4, a1
;     csrr a0, mhartid
80000098: 73 25 40 f1  	csrr	a0, mhartid
;     li   t0, 0
8000009c: 93 02 00 00  	li	t0, 0
;     sub  a0, a0, t0
800000a0: 33 05 55 40  	sub	a0, a0, t0
;     li   a1, 1
800000a4: 93 05 10 00  	li	a1, 1
;     div  t0, a0, a1
800000a8: b3 42 b5 02  	div	t0, a0, a1
;     remu a0, a0, a1
800000ac: 33 75 b5 02  	remu	a0, a0, a1
;     li   a2, 1048576
800000b0: 37 06 10 00  	lui	a2, 256
;     li   t1, 0
800000b4: 13 03 00 00  	li	t1, 0
;     mul  t0, t1, t0
800000b8: b3 02 53 02  	mul	t0, t1, t0
;     add  a2, a2, t0
800000bc: 33 06 56 00  	add	a2, a2, t0
;     li   t0, 131072
800000c0: b7 02 02 00  	lui	t0, 32
;     add  a3, a2, t0
800000c4: b3 06 56 00  	add	a3, a2, t0

800000c8 <.Lpcrel_hi0>:
;     lw        t0, _snrt_team_size
800000c8: 97 32 00 00  	auipc	t0, 3
800000cc: 83 a2 02 27  	lw	t0, 624(t0)
;     sub       a3, a3, t0
800000d0: b3 86 56 40  	sub	a3, a3, t0
;     mv        a5, a3
800000d4: 93 87 06 00  	mv	a5, a3

800000d8 <snrt.crt0.init_stack>:
;     lw        t2, snrt_stack_size  # load stack size (overridable by binary)
800000d8: 97 33 00 00  	auipc	t2, 3
800000dc: 83 a3 43 26  	lw	t2, 612(t2)
;     sll       t0, a0, t2  # this hart
800000e0: b3 12 75 00  	sll	t0, a0, t2
;     sll       t1, a1, t2  # all harts
800000e4: 33 93 75 00  	sll	t1, a1, t2
;     sub       sp, a3, t0
800000e8: 33 81 56 40  	sub	sp, a3, t0
;     sub       a3, a3, t1
800000ec: b3 86 66 40  	sub	a3, a3, t1
;     slli      t0, a0, 3  # this hart
800000f0: 93 12 35 00  	slli	t0, a0, 3
;     slli      t1, a1, 3  # all harts
800000f4: 13 93 35 00  	slli	t1, a1, 3
;     sub       sp, sp, t0
800000f8: 33 01 51 40  	sub	sp, sp, t0
;     sub       a3, a3, t1
800000fc: b3 86 66 40  	sub	a3, a3, t1

80000100 <snrt.crt0.init_tls>:
;     la        t0, __tdata_end
80000100: 97 32 00 00  	auipc	t0, 3
80000104: 93 82 42 4d  	addi	t0, t0, 1236

80000108 <.Lpcrel_hi3>:
;     la        t1, __tdata_start
80000108: 17 33 00 00  	auipc	t1, 3
8000010c: 13 03 c3 4c  	addi	t1, t1, 1228
;     sub       t0, t0, t1
80000110: b3 82 62 40  	sub	t0, t0, t1
;     sub       sp, sp, t0
80000114: 33 01 51 40  	sub	sp, sp, t0

80000118 <.Lpcrel_hi4>:
;     la        t0, __tbss_end
80000118: 97 32 00 00  	auipc	t0, 3
8000011c: 93 82 82 4c  	addi	t0, t0, 1224

80000120 <.Lpcrel_hi5>:
;     la        t1, __tbss_start
80000120: 17 33 00 00  	auipc	t1, 3
80000124: 13 03 43 4b  	addi	t1, t1, 1204
;     sub       t0, t0, t1
80000128: b3 82 62 40  	sub	t0, t0, t1
;     sub       sp, sp, t0
8000012c: 33 01 51 40  	sub	sp, sp, t0
;     mv        tp, sp
80000130: 13 02 01 00  	mv	tp, sp
;     addi      sp, sp, -4
80000134: 13 01 c1 ff  	addi	sp, sp, -4
;     andi      sp, sp, ~0x7
80000138: 13 71 81 ff  	andi	sp, sp, -8

8000013c <.Lpcrel_hi6>:
;     la        t0, __tdata_start
8000013c: 97 32 00 00  	auipc	t0, 3
80000140: 93 82 82 49  	addi	t0, t0, 1176

80000144 <.Lpcrel_hi7>:
;     la        t1, __tdata_end
80000144: 17 33 00 00  	auipc	t1, 3
80000148: 13 03 03 49  	addi	t1, t1, 1168

8000014c <.Lpcrel_hi8>:
;     la        t2, __tbss_start
8000014c: 97 33 00 00  	auipc	t2, 3
80000150: 93 83 83 48  	addi	t2, t2, 1160

80000154 <.Lpcrel_hi9>:
;     la        t3, __tbss_end
80000154: 17 3e 00 00  	auipc	t3, 3
80000158: 13 0e ce 48  	addi	t3, t3, 1164
;     mv        t4, tp
8000015c: 93 0e 02 00  	mv	t4, tp
;     bge       t0, t1, 2f
80000160: 63 dc 62 00  	bge	t0, t1, 0x80000178 <.Lpcrel_hi10>
; 1:  lw        t5, 0(t0)
80000164: 03 af 02 00  	lw	t5, 0(t0)
;     sw        t5, 0(t4)
80000168: 23 a0 ee 01  	sw	t5, 0(t4)
;     addi      t0, t0, 4
8000016c: 93 82 42 00  	addi	t0, t0, 4
;     addi      t4, t4, 4
80000170: 93 8e 4e 00  	addi	t4, t4, 4
;     blt       t0, t1, 1b
80000174: e3 c8 62 fe  	blt	t0, t1, 0x80000164 <.Lpcrel_hi9+0x10>

80000178 <.Lpcrel_hi10>:
;     la        t0, __tbss_start
80000178: 97 32 00 00  	auipc	t0, 3
8000017c: 93 82 c2 45  	addi	t0, t0, 1116

80000180 <.Lpcrel_hi11>:
;     la        t1, __tbss_end
80000180: 17 33 00 00  	auipc	t1, 3
80000184: 13 03 03 46  	addi	t1, t1, 1120
;     bge       t0, t1, 2f
80000188: 63 da 62 00  	bge	t0, t1, 0x8000019c <snrt.crt0.init_interrupt>
; 1:  sw        zero, 0(t4)
8000018c: 23 a0 0e 00  	sw	zero, 0(t4)
;     addi      t0, t0, 4
80000190: 93 82 42 00  	addi	t0, t0, 4
;     addi      t4, t4, 4
80000194: 93 8e 4e 00  	addi	t4, t4, 4
;     blt       t0, t2, 1b
80000198: e3 ca 72 fe  	blt	t0, t2, 0x8000018c <.Lpcrel_hi11+0xc>

8000019c <snrt.crt0.init_interrupt>:
;     la t0, __snrt_crt0_interrupt_handler
8000019c: 97 02 00 00  	auipc	t0, 0
800001a0: 93 82 42 07  	addi	t0, t0, 116
;     csrw   mtvec, t0
800001a4: 73 90 52 30  	csrw	mtvec, t0

800001a8 <snrt.crt0.init_team>:
;     addi      sp, sp, -20
800001a8: 13 01 c1 fe  	addi	sp, sp, -20
;     sw        a0, 0(sp)
800001ac: 23 20 a1 00  	sw	a0, 0(sp)
;     sw        a1, 4(sp)
800001b0: 23 22 b1 00  	sw	a1, 4(sp)
;     sw        a2, 8(sp)
800001b4: 23 24 c1 00  	sw	a2, 8(sp)
;     sw        a3, 12(sp)
800001b8: 23 26 d1 00  	sw	a3, 12(sp)
;     sw        a4, 16(sp)
800001bc: 23 28 e1 00  	sw	a4, 16(sp)
;     call      _snrt_init_team
800001c0: 97 30 00 00  	auipc	ra, 3
800001c4: e7 80 40 d8  	jalr	-636(ra)
;     lw        a0, 0(sp)
800001c8: 03 25 01 00  	lw	a0, 0(sp)
;     lw        a1, 4(sp)
800001cc: 83 25 41 00  	lw	a1, 4(sp)
;     lw        a2, 8(sp)
800001d0: 03 26 81 00  	lw	a2, 8(sp)
;     lw        a3, 12(sp)
800001d4: 83 26 c1 00  	lw	a3, 12(sp)
;     lw        a4, 16(sp)
800001d8: 03 27 01 01  	lw	a4, 16(sp)
;     addi      sp, sp, 20
800001dc: 13 01 41 01  	addi	sp, sp, 20

800001e0 <snrt.crt0.pre_barrier>:
;     call      _snrt_cluster_barrier
800001e0: 97 00 00 00  	auipc	ra, 0
800001e4: e7 80 c0 1e  	jalr	492(ra)

800001e8 <snrt.crt0.main>:
;     call      main  # main(int core_id, int core_num, void *spm_start, void *spm_end)
800001e8: 97 00 00 00  	auipc	ra, 0
800001ec: e7 80 80 76  	jalr	1896(ra)
;     mv        s0, a0 # store return value in s0
800001f0: 13 04 05 00  	mv	s0, a0

800001f4 <snrt.crt0.post_barrier>:
;     call      _snrt_cluster_barrier
800001f4: 97 00 00 00  	auipc	ra, 0
800001f8: e7 80 80 1d  	jalr	472(ra)

800001fc <snrt.crt0.end>:
;     mv        a0, s0 # recover return value of main function in s0
800001fc: 13 05 04 00  	mv	a0, s0
;     call      _snrt_exit
80000200: 97 00 00 00  	auipc	ra, 0
80000204: e7 80 80 1f  	jalr	504(ra)
;     wfi
80000208: 73 00 50 10  	wfi	
;     j       1b
8000020c: 6f f0 df ff  	j	0x80000208 <snrt.crt0.end+0xc>

80000210 <__snrt_crt0_interrupt_handler>:
;     addi    sp, sp, -80
80000210: 13 01 01 fb  	addi	sp, sp, -80
;     sw  ra, 64(sp)
80000214: 23 20 11 04  	sw	ra, 64(sp)
;     sw  t0, 60(sp)
80000218: 23 2e 51 02  	sw	t0, 60(sp)
;     sw  t1, 56(sp)
8000021c: 23 2c 61 02  	sw	t1, 56(sp)
;     sw  t2, 52(sp)
80000220: 23 2a 71 02  	sw	t2, 52(sp)
;     sw  s0, 48(sp)
80000224: 23 28 81 02  	sw	s0, 48(sp)
;     sw  a0, 44(sp)
80000228: 23 26 a1 02  	sw	a0, 44(sp)
;     sw  a1, 40(sp)
8000022c: 23 24 b1 02  	sw	a1, 40(sp)
;     sw  a2, 36(sp)
80000230: 23 22 c1 02  	sw	a2, 36(sp)
;     sw  a3, 32(sp)
80000234: 23 20 d1 02  	sw	a3, 32(sp)
;     sw  a4, 28(sp)
80000238: 23 2e e1 00  	sw	a4, 28(sp)
;     sw  a5, 24(sp)
8000023c: 23 2c f1 00  	sw	a5, 24(sp)
;     sw  a6, 20(sp)
80000240: 23 2a 01 01  	sw	a6, 20(sp)
;     sw  a7, 16(sp)
80000244: 23 28 11 01  	sw	a7, 16(sp)
;     sw  t3, 12(sp)
80000248: 23 26 c1 01  	sw	t3, 12(sp)
;     sw  t4,  8(sp)
8000024c: 23 24 d1 01  	sw	t4, 8(sp)
;     sw  t5,  4(sp)
80000250: 23 22 e1 01  	sw	t5, 4(sp)
;     sw  t6,  0(sp)
80000254: 23 20 f1 01  	sw	t6, 0(sp)
;     csrr    t0, misa
80000258: f3 22 10 30  	csrr	t0, misa
;     andi    t0, t0, (1 << 3) # 3: D - single precision float extension
8000025c: 93 f2 82 00  	andi	t0, t0, 8
;     beqz    t0, 1f
80000260: 63 84 02 08  	beqz	t0, 0x800002e8 <__snrt_crt0_interrupt_handler+0xd8>
;     addi    sp, sp, -256
80000264: 13 01 01 f0  	addi	sp, sp, -256
;     fsd     f0, 248(sp)
80000268: 27 3c 01 0e  	fsd	ft0, 248(sp)
;     fsd     f1, 240(sp)
8000026c: 27 38 11 0e  	fsd	ft1, 240(sp)
;     fsd     f2, 232(sp)
80000270: 27 34 21 0e  	fsd	ft2, 232(sp)
;     fsd     f3, 224(sp)
80000274: 27 30 31 0e  	fsd	ft3, 224(sp)
;     fsd     f4, 216(sp)
80000278: 27 3c 41 0c  	fsd	ft4, 216(sp)
;     fsd     f5, 208(sp)
8000027c: 27 38 51 0c  	fsd	ft5, 208(sp)
;     fsd     f6, 200(sp)
80000280: 27 34 61 0c  	fsd	ft6, 200(sp)
;     fsd     f7, 192(sp)
80000284: 27 30 71 0c  	fsd	ft7, 192(sp)
;     fsd     f8, 184(sp)
80000288: 27 3c 81 0a  	fsd	fs0, 184(sp)
;     fsd     f9, 176(sp)
8000028c: 27 38 91 0a  	fsd	fs1, 176(sp)
;     fsd     f10, 168(sp)
80000290: 27 34 a1 0a  	fsd	fa0, 168(sp)
;     fsd     f11, 160(sp)
80000294: 27 30 b1 0a  	fsd	fa1, 160(sp)
;     fsd     f12, 152(sp)
80000298: 27 3c c1 08  	fsd	fa2, 152(sp)
;     fsd     f13, 144(sp)
8000029c: 27 38 d1 08  	fsd	fa3, 144(sp)
;     fsd     f14, 136(sp)
800002a0: 27 34 e1 08  	fsd	fa4, 136(sp)
;     fsd     f15, 128(sp)
800002a4: 27 30 f1 08  	fsd	fa5, 128(sp)
;     fsd     f16, 120(sp)
800002a8: 27 3c 01 07  	fsd	fa6, 120(sp)
;     fsd     f17, 112(sp)
800002ac: 27 38 11 07  	fsd	fa7, 112(sp)
;     fsd     f18, 104(sp)
800002b0: 27 34 21 07  	fsd	fs2, 104(sp)
;     fsd     f19, 96(sp)
800002b4: 27 30 31 07  	fsd	fs3, 96(sp)
;     fsd     f20, 88(sp)
800002b8: 27 3c 41 05  	fsd	fs4, 88(sp)
;     fsd     f21, 80(sp)
800002bc: 27 38 51 05  	fsd	fs5, 80(sp)
;     fsd     f22, 72(sp)
800002c0: 27 34 61 05  	fsd	fs6, 72(sp)
;     fsd     f23, 64(sp)
800002c4: 27 30 71 05  	fsd	fs7, 64(sp)
;     fsd     f24, 56(sp)
800002c8: 27 3c 81 03  	fsd	fs8, 56(sp)
;     fsd     f25, 48(sp)
800002cc: 27 38 91 03  	fsd	fs9, 48(sp)
;     fsd     f26, 40(sp)
800002d0: 27 34 a1 03  	fsd	fs10, 40(sp)
;     fsd     f27, 32(sp)
800002d4: 27 30 b1 03  	fsd	fs11, 32(sp)
;     fsd     f28, 24(sp)
800002d8: 27 3c c1 01  	fsd	ft8, 24(sp)
;     fsd     f29, 16(sp)
800002dc: 27 38 d1 01  	fsd	ft9, 16(sp)
;     fsd     f30, 8(sp)
800002e0: 27 34 e1 01  	fsd	ft10, 8(sp)
;     fsd     f31, 0(sp)
800002e4: 27 30 f1 01  	fsd	ft11, 0(sp)
;     call    __snrt_isr
800002e8: 97 30 00 00  	auipc	ra, 3
800002ec: e7 80 80 dd  	jalr	-552(ra)
;     csrr    t0, misa
800002f0: f3 22 10 30  	csrr	t0, misa
;     andi    t0, t0, (1 << 3) # 3: D - single precision float extension
800002f4: 93 f2 82 00  	andi	t0, t0, 8
;     beqz    t0, 1f
800002f8: 63 84 02 08  	beqz	t0, 0x80000380 <__snrt_crt0_interrupt_handler+0x170>
;     fld     f0, 248(sp)
800002fc: 07 30 81 0f  	fld	ft0, 248(sp)
;     fld     f1, 240(sp)
80000300: 87 30 01 0f  	fld	ft1, 240(sp)
;     fld     f2, 232(sp)
80000304: 07 31 81 0e  	fld	ft2, 232(sp)
;     fld     f3, 224(sp)
80000308: 87 31 01 0e  	fld	ft3, 224(sp)
;     fld     f4, 216(sp)
8000030c: 07 32 81 0d  	fld	ft4, 216(sp)
;     fld     f5, 208(sp)
80000310: 87 32 01 0d  	fld	ft5, 208(sp)
;     fld     f6, 200(sp)
80000314: 07 33 81 0c  	fld	ft6, 200(sp)
;     fld     f7, 192(sp)
80000318: 87 33 01 0c  	fld	ft7, 192(sp)
;     fld     f8, 184(sp)
8000031c: 07 34 81 0b  	fld	fs0, 184(sp)
;     fld     f9, 176(sp)
80000320: 87 34 01 0b  	fld	fs1, 176(sp)
;     fld     f10, 168(sp)
80000324: 07 35 81 0a  	fld	fa0, 168(sp)
;     fld     f11, 160(sp)
80000328: 87 35 01 0a  	fld	fa1, 160(sp)
;     fld     f12, 152(sp)
8000032c: 07 36 81 09  	fld	fa2, 152(sp)
;     fld     f13, 144(sp)
80000330: 87 36 01 09  	fld	fa3, 144(sp)
;     fld     f14, 136(sp)
80000334: 07 37 81 08  	fld	fa4, 136(sp)
;     fld     f15, 128(sp)
80000338: 87 37 01 08  	fld	fa5, 128(sp)
;     fld     f16, 120(sp)
8000033c: 07 38 81 07  	fld	fa6, 120(sp)
;     fld     f17, 112(sp)
80000340: 87 38 01 07  	fld	fa7, 112(sp)
;     fld     f18, 104(sp)
80000344: 07 39 81 06  	fld	fs2, 104(sp)
;     fld     f19, 96(sp)
80000348: 87 39 01 06  	fld	fs3, 96(sp)
;     fld     f20, 88(sp)
8000034c: 07 3a 81 05  	fld	fs4, 88(sp)
;     fld     f21, 80(sp)
80000350: 87 3a 01 05  	fld	fs5, 80(sp)
;     fld     f22, 72(sp)
80000354: 07 3b 81 04  	fld	fs6, 72(sp)
;     fld     f23, 64(sp)
80000358: 87 3b 01 04  	fld	fs7, 64(sp)
;     fld     f24, 56(sp)
8000035c: 07 3c 81 03  	fld	fs8, 56(sp)
;     fld     f25, 48(sp)
80000360: 87 3c 01 03  	fld	fs9, 48(sp)
;     fld     f26, 40(sp)
80000364: 07 3d 81 02  	fld	fs10, 40(sp)
;     fld     f27, 32(sp)
80000368: 87 3d 01 02  	fld	fs11, 32(sp)
;     fld     f28, 24(sp)
8000036c: 07 3e 81 01  	fld	ft8, 24(sp)
;     fld     f29, 16(sp)
80000370: 87 3e 01 01  	fld	ft9, 16(sp)
;     fld     f30, 8(sp)
80000374: 07 3f 81 00  	fld	ft10, 8(sp)
;     fld     f31, 0(sp)
80000378: 87 3f 01 00  	fld	ft11, 0(sp)
;     addi    sp, sp, 256
8000037c: 13 01 01 10  	addi	sp, sp, 256
;     lw  t6,  0(sp)
80000380: 83 2f 01 00  	lw	t6, 0(sp)
;     lw  t5,  4(sp)
80000384: 03 2f 41 00  	lw	t5, 4(sp)
;     lw  t4,  8(sp)
80000388: 83 2e 81 00  	lw	t4, 8(sp)
;     lw  t3, 12(sp)
8000038c: 03 2e c1 00  	lw	t3, 12(sp)
;     lw  a7, 16(sp)
80000390: 83 28 01 01  	lw	a7, 16(sp)
;     lw  a6, 20(sp)
80000394: 03 28 41 01  	lw	a6, 20(sp)
;     lw  a5, 24(sp)
80000398: 83 27 81 01  	lw	a5, 24(sp)
;     lw  a4, 28(sp)
8000039c: 03 27 c1 01  	lw	a4, 28(sp)
;     lw  a3, 32(sp)
800003a0: 83 26 01 02  	lw	a3, 32(sp)
;     lw  a2, 36(sp)
800003a4: 03 26 41 02  	lw	a2, 36(sp)
;     lw  a1, 40(sp)
800003a8: 83 25 81 02  	lw	a1, 40(sp)
;     lw  a0, 44(sp)
800003ac: 03 25 c1 02  	lw	a0, 44(sp)
;     lw  s0, 48(sp)
800003b0: 03 24 01 03  	lw	s0, 48(sp)
;     lw  t2, 52(sp)
800003b4: 83 23 41 03  	lw	t2, 52(sp)
;     lw  t1, 56(sp)
800003b8: 03 23 81 03  	lw	t1, 56(sp)
;     lw  t0, 60(sp)
800003bc: 83 22 c1 03  	lw	t0, 60(sp)
;     lw  ra, 64(sp)
800003c0: 83 20 01 04  	lw	ra, 64(sp)
;     addi    sp, sp, 80
800003c4: 13 01 01 05  	addi	sp, sp, 80
;     mret
800003c8: 73 00 20 30  	mret	

800003cc <_snrt_cluster_barrier>:
;     addi      sp, sp, -4
800003cc: 13 01 c1 ff  	addi	sp, sp, -4
;     sw        ra, 0(sp)
800003d0: 23 20 11 00  	sw	ra, 0(sp)
;     call      _snrt_barrier_reg_ptr
800003d4: 97 30 00 00  	auipc	ra, 3
800003d8: e7 80 00 ab  	jalr	-1360(ra)
;     lw        a0, 0(a0)
800003dc: 03 25 05 00  	lw	a0, 0(a0)
;     mv        zero, a0
800003e0: 13 00 05 00  	mv	zero, a0
;     lw        ra, 0(sp)
800003e4: 83 20 01 00  	lw	ra, 0(sp)
;     addi      sp, sp, 4
800003e8: 13 01 41 00  	addi	sp, sp, 4
;     ret
800003ec: 67 80 00 00  	ret

800003f0 <_snrt_get_base_hartid>:
;     lw        a0, 16(a0)  # load cluster's hartid offset
800003f0: 03 25 05 01  	lw	a0, 16(a0)
;     ret
800003f4: 67 80 00 00  	ret

800003f8 <_snrt_exit>:
;     addi      sp, sp, -8
800003f8: 13 01 81 ff  	addi	sp, sp, -8
;     sw        a0, 0(sp)
800003fc: 23 20 a1 00  	sw	a0, 0(sp)
;     sw        ra, 4(sp)
80000400: 23 22 11 00  	sw	ra, 4(sp)
;     call      snrt_global_core_idx
80000404: 97 30 00 00  	auipc	ra, 3
80000408: e7 80 80 a3  	jalr	-1480(ra)
;     lw        t0, 0(sp)
8000040c: 83 22 01 00  	lw	t0, 0(sp)
;     lw        ra, 4(sp)
80000410: 83 20 41 00  	lw	ra, 4(sp)
;     addi      sp, sp, 8
80000414: 13 01 81 00  	addi	sp, sp, 8
;     bnez      a0, 1f
80000418: 63 1c 05 00  	bnez	a0, 0x80000430 <.Lpcrel_hi0+0xc>
;     slli      t0, t0, 1
8000041c: 93 92 12 00  	slli	t0, t0, 1
;     ori       t0, t0, 1
80000420: 93 e2 12 00  	ori	t0, t0, 1

80000424 <.Lpcrel_hi0>:
;     la        t1, tohost
80000424: 17 33 00 00  	auipc	t1, 3
80000428: 13 03 c3 15  	addi	t1, t1, 348
;     sw        t0, 0(t1)
8000042c: 23 20 53 00  	sw	t0, 0(t1)
; 1:  ret
80000430: 67 80 00 00  	ret

80000434 <__udivdi3>:
80000434: 93 87 05 00  	mv	a5, a1

80000438 <.LBB16>:
80000438: 13 08 06 00  	mv	a6, a2

8000043c <.LVL1>:
8000043c: 93 88 06 00  	mv	a7, a3

80000440 <.LVL2>:
80000440: 13 03 05 00  	mv	t1, a0

80000444 <.LVL3>:
80000444: 63 96 06 28  	bnez	a3, 0x800006d0 <.LVL100>
80000448: 37 37 00 80  	lui	a4, 524291
8000044c: 13 07 87 23  	addi	a4, a4, 568

80000450 <.L0 >:
80000450: 63 f6 c5 0e  	bgeu	a1, a2, 0x8000053c <.LVL38>

80000454 <.LVL4>:
80000454: b7 06 01 00  	lui	a3, 16

80000458 <.LVL5>:
80000458: 63 78 d6 0c  	bgeu	a2, a3, 0x80000528 <.LVL36>
8000045c: 93 06 f0 0f  	li	a3, 255
80000460: b3 b6 c6 00  	sltu	a3, a3, a2
80000464: 93 96 36 00  	slli	a3, a3, 3

80000468 <.LVL6>:
80000468: b3 58 d6 00  	srl	a7, a2, a3
8000046c: 33 07 17 01  	add	a4, a4, a7
80000470: 03 47 07 00  	lbu	a4, 0(a4)
80000474: b3 06 d7 00  	add	a3, a4, a3

80000478 <.LVL7>:
80000478: 13 07 00 02  	li	a4, 32
8000047c: 33 07 d7 40  	sub	a4, a4, a3

80000480 <.LVL8>:
80000480: 63 0c 07 00  	beqz	a4, 0x80000498 <.LVL12>

80000484 <.LVL9>:
80000484: b3 97 e7 00  	sll	a5, a5, a4
80000488: b3 56 d5 00  	srl	a3, a0, a3

8000048c <.L0 >:
8000048c: 33 18 e6 00  	sll	a6, a2, a4

80000490 <.LVL10>:
80000490: b3 e5 f6 00  	or	a1, a3, a5

80000494 <.LVL11>:
80000494: 33 13 e5 00  	sll	t1, a0, a4

80000498 <.LVL12>:
80000498: 13 55 08 01  	srli	a0, a6, 16

8000049c <.LVL13>:
8000049c: b3 d7 a5 02  	divu	a5, a1, a0
800004a0: 13 16 08 01  	slli	a2, a6, 16

800004a4 <.LVL14>:
800004a4: 13 56 06 01  	srli	a2, a2, 16

800004a8 <.LVL15>:
800004a8: 93 56 03 01  	srli	a3, t1, 16
800004ac: 33 f7 a5 02  	remu	a4, a1, a0

800004b0 <.LVL16>:
800004b0: b3 05 f6 02  	mul	a1, a2, a5

800004b4 <.LVL17>:
800004b4: 13 17 07 01  	slli	a4, a4, 16

800004b8 <.LVL18>:
800004b8: b3 66 d7 00  	or	a3, a4, a3

800004bc <.LVL19>:
800004bc: 13 87 07 00  	mv	a4, a5
800004c0: 63 fe b6 00  	bgeu	a3, a1, 0x800004dc <.L7>
800004c4: b3 86 06 01  	add	a3, a3, a6

800004c8 <.LVL20>:
800004c8: 13 87 f7 ff  	addi	a4, a5, -1

800004cc <.LVL21>:
800004cc: 63 e8 06 01  	bltu	a3, a6, 0x800004dc <.L7>
800004d0: 63 f6 b6 00  	bgeu	a3, a1, 0x800004dc <.L7>
800004d4: 13 87 e7 ff  	addi	a4, a5, -2
800004d8: b3 86 06 01  	add	a3, a3, a6

800004dc <.L7>:
800004dc: b3 86 b6 40  	sub	a3, a3, a1
800004e0: b3 f7 a6 02  	remu	a5, a3, a0

800004e4 <.LVL25>:
800004e4: 13 13 03 01  	slli	t1, t1, 16

800004e8 <.LVL26>:
800004e8: 13 53 03 01  	srli	t1, t1, 16
800004ec: b3 d6 a6 02  	divu	a3, a3, a0

800004f0 <.LVL27>:
800004f0: 93 97 07 01  	slli	a5, a5, 16

800004f4 <.LVL28>:
800004f4: 33 e3 67 00  	or	t1, a5, t1
800004f8: b3 05 d6 02  	mul	a1, a2, a3

800004fc <.LVL29>:
800004fc: 13 85 06 00  	mv	a0, a3

80000500 <.LVL30>:
80000500: 63 7c b3 00  	bgeu	t1, a1, 0x80000518 <.L8>
80000504: 33 03 68 00  	add	t1, a6, t1

80000508 <.LVL31>:
80000508: 13 85 f6 ff  	addi	a0, a3, -1

8000050c <.LVL32>:
8000050c: 63 66 03 01  	bltu	t1, a6, 0x80000518 <.L8>
80000510: 63 74 b3 00  	bgeu	t1, a1, 0x80000518 <.L8>
80000514: 13 85 e6 ff  	addi	a0, a3, -2

80000518 <.L8>:
80000518: 13 17 07 01  	slli	a4, a4, 16

8000051c <.LVL34>:
8000051c: 33 67 a7 00  	or	a4, a4, a0

80000520 <.LVL35>:
80000520: 93 05 00 00  	li	a1, 0
80000524: 6f 00 40 0e  	j	0x80000608 <.LVL65>

80000528 <.LVL36>:
80000528: b7 08 00 01  	lui	a7, 4096

8000052c <.LVL37>:
8000052c: 93 06 00 01  	li	a3, 16
80000530: e3 6c 16 f3  	bltu	a2, a7, 0x80000468 <.LVL6>
80000534: 93 06 80 01  	li	a3, 24
80000538: 6f f0 1f f3  	j	0x80000468 <.LVL6>

8000053c <.LVL38>:
8000053c: 63 16 06 00  	bnez	a2, 0x80000548 <.LVL40>

80000540 <.L0 >:
80000540: 93 06 10 00  	li	a3, 1

80000544 <.LVL39>:
80000544: 33 d8 c6 02  	divu	a6, a3, a2

80000548 <.LVL40>:
80000548: b7 06 01 00  	lui	a3, 16
8000054c: 63 72 d8 0c  	bgeu	a6, a3, 0x80000610 <.LVL66>
80000550: 93 06 f0 0f  	li	a3, 255
80000554: 63 f4 06 01  	bgeu	a3, a6, 0x8000055c <.LVL41>
80000558: 93 08 80 00  	li	a7, 8

8000055c <.LVL41>:
8000055c: b3 56 18 01  	srl	a3, a6, a7
80000560: 33 07 d7 00  	add	a4, a4, a3
80000564: 83 46 07 00  	lbu	a3, 0(a4)
80000568: 13 07 00 02  	li	a4, 32
8000056c: b3 86 16 01  	add	a3, a3, a7
80000570: 33 07 d7 40  	sub	a4, a4, a3

80000574 <.LVL42>:
80000574: 63 18 07 0a  	bnez	a4, 0x80000624 <.LVL68>

80000578 <.L0 >:
80000578: b3 87 07 41  	sub	a5, a5, a6

8000057c <.LVL43>:
8000057c: 93 05 10 00  	li	a1, 1

80000580 <.LVL44>:
80000580: 13 56 08 01  	srli	a2, a6, 16

80000584 <.LVL45>:
80000584: 93 18 08 01  	slli	a7, a6, 16
80000588: 93 d8 08 01  	srli	a7, a7, 16

8000058c <.LVL46>:
8000058c: 93 56 03 01  	srli	a3, t1, 16
80000590: 33 f7 c7 02  	remu	a4, a5, a2

80000594 <.LVL47>:
80000594: b3 d7 c7 02  	divu	a5, a5, a2

80000598 <.LVL48>:
80000598: 13 17 07 01  	slli	a4, a4, 16

8000059c <.LVL49>:
8000059c: b3 66 d7 00  	or	a3, a4, a3
800005a0: 33 85 f8 02  	mul	a0, a7, a5

800005a4 <.LVL50>:
800005a4: 13 87 07 00  	mv	a4, a5
800005a8: 63 fe a6 00  	bgeu	a3, a0, 0x800005c4 <.L17>
800005ac: b3 86 06 01  	add	a3, a3, a6

800005b0 <.LVL51>:
800005b0: 13 87 f7 ff  	addi	a4, a5, -1

800005b4 <.LVL52>:
800005b4: 63 e8 06 01  	bltu	a3, a6, 0x800005c4 <.L17>
800005b8: 63 f6 a6 00  	bgeu	a3, a0, 0x800005c4 <.L17>
800005bc: 13 87 e7 ff  	addi	a4, a5, -2
800005c0: b3 86 06 01  	add	a3, a3, a6

800005c4 <.L17>:
800005c4: b3 86 a6 40  	sub	a3, a3, a0
800005c8: b3 f7 c6 02  	remu	a5, a3, a2

800005cc <.LVL56>:
800005cc: 13 13 03 01  	slli	t1, t1, 16

800005d0 <.LVL57>:
800005d0: 13 53 03 01  	srli	t1, t1, 16
800005d4: b3 d6 c6 02  	divu	a3, a3, a2

800005d8 <.LVL58>:
800005d8: 93 97 07 01  	slli	a5, a5, 16

800005dc <.LVL59>:
800005dc: 33 e3 67 00  	or	t1, a5, t1
800005e0: b3 88 d8 02  	mul	a7, a7, a3

800005e4 <.LVL60>:
800005e4: 13 85 06 00  	mv	a0, a3
800005e8: 63 7c 13 01  	bgeu	t1, a7, 0x80000600 <.L18>
800005ec: 33 03 68 00  	add	t1, a6, t1

800005f0 <.LVL61>:
800005f0: 13 85 f6 ff  	addi	a0, a3, -1

800005f4 <.LVL62>:
800005f4: 63 66 03 01  	bltu	t1, a6, 0x80000600 <.L18>
800005f8: 63 74 13 01  	bgeu	t1, a7, 0x80000600 <.L18>
800005fc: 13 85 e6 ff  	addi	a0, a3, -2

80000600 <.L18>:
80000600: 13 17 07 01  	slli	a4, a4, 16

80000604 <.LVL64>:
80000604: 33 67 a7 00  	or	a4, a4, a0

80000608 <.LVL65>:
80000608: 13 05 07 00  	mv	a0, a4
8000060c: 67 80 00 00  	ret

80000610 <.LVL66>:
80000610: b7 06 00 01  	lui	a3, 4096
80000614: 93 08 00 01  	li	a7, 16

80000618 <.LVL67>:
80000618: e3 62 d8 f4  	bltu	a6, a3, 0x8000055c <.LVL41>
8000061c: 93 08 80 01  	li	a7, 24
80000620: 6f f0 df f3  	j	0x8000055c <.LVL41>

80000624 <.LVL68>:
80000624: 33 18 e8 00  	sll	a6, a6, a4

80000628 <.LVL69>:
80000628: b3 d5 d7 00  	srl	a1, a5, a3

8000062c <.LVL70>:
8000062c: 33 13 e5 00  	sll	t1, a0, a4

80000630 <.LVL71>:
80000630: b3 56 d5 00  	srl	a3, a0, a3

80000634 <.LVL72>:
80000634: 13 55 08 01  	srli	a0, a6, 16

80000638 <.LVL73>:
80000638: b3 97 e7 00  	sll	a5, a5, a4

8000063c <.LVL74>:
8000063c: 33 f7 a5 02  	remu	a4, a1, a0

80000640 <.LVL75>:
80000640: b3 e8 f6 00  	or	a7, a3, a5

80000644 <.LVL76>:
80000644: 93 17 08 01  	slli	a5, a6, 16
80000648: 93 d7 07 01  	srli	a5, a5, 16

8000064c <.LVL77>:
8000064c: 13 d6 08 01  	srli	a2, a7, 16

80000650 <.LVL78>:
80000650: b3 d5 a5 02  	divu	a1, a1, a0

80000654 <.LVL79>:
80000654: 13 17 07 01  	slli	a4, a4, 16

80000658 <.LVL80>:
80000658: 33 67 c7 00  	or	a4, a4, a2
8000065c: b3 86 b7 02  	mul	a3, a5, a1

80000660 <.LVL81>:
80000660: 13 86 05 00  	mv	a2, a1
80000664: 63 7e d7 00  	bgeu	a4, a3, 0x80000680 <.L15>
80000668: 33 07 07 01  	add	a4, a4, a6

8000066c <.LVL82>:
8000066c: 13 86 f5 ff  	addi	a2, a1, -1

80000670 <.LVL83>:
80000670: 63 68 07 01  	bltu	a4, a6, 0x80000680 <.L15>
80000674: 63 76 d7 00  	bgeu	a4, a3, 0x80000680 <.L15>
80000678: 13 86 e5 ff  	addi	a2, a1, -2
8000067c: 33 07 07 01  	add	a4, a4, a6

80000680 <.L15>:
80000680: b3 06 d7 40  	sub	a3, a4, a3

80000684 <.LVL86>:
80000684: 33 f7 a6 02  	remu	a4, a3, a0

80000688 <.LVL87>:
80000688: 93 98 08 01  	slli	a7, a7, 16

8000068c <.LVL88>:
8000068c: 93 d8 08 01  	srli	a7, a7, 16
80000690: b3 d6 a6 02  	divu	a3, a3, a0

80000694 <.LVL89>:
80000694: 13 17 07 01  	slli	a4, a4, 16

80000698 <.LVL90>:
80000698: b3 85 d7 02  	mul	a1, a5, a3

8000069c <.LVL91>:
8000069c: b3 67 17 01  	or	a5, a4, a7

800006a0 <.LVL92>:
800006a0: 13 87 06 00  	mv	a4, a3
800006a4: 63 fe b7 00  	bgeu	a5, a1, 0x800006c0 <.L16>
800006a8: b3 87 07 01  	add	a5, a5, a6

800006ac <.LVL93>:
800006ac: 13 87 f6 ff  	addi	a4, a3, -1

800006b0 <.LVL94>:
800006b0: 63 e8 07 01  	bltu	a5, a6, 0x800006c0 <.L16>
800006b4: 63 f6 b7 00  	bgeu	a5, a1, 0x800006c0 <.L16>
800006b8: 13 87 e6 ff  	addi	a4, a3, -2
800006bc: b3 87 07 01  	add	a5, a5, a6

800006c0 <.L16>:
800006c0: b3 87 b7 40  	sub	a5, a5, a1
800006c4: 93 15 06 01  	slli	a1, a2, 16

800006c8 <.LVL98>:
800006c8: b3 e5 e5 00  	or	a1, a1, a4

800006cc <.LVL99>:
800006cc: 6f f0 5f eb  	j	0x80000580 <.LVL44>

800006d0 <.LVL100>:
800006d0: 63 e6 d5 18  	bltu	a1, a3, 0x8000085c <.LVL161>

800006d4 <.LVL101>:
800006d4: 37 07 01 00  	lui	a4, 16
800006d8: 63 f4 e6 04  	bgeu	a3, a4, 0x80000720 <.LVL106>
800006dc: 13 08 f0 0f  	li	a6, 255
800006e0: 33 37 d8 00  	sltu	a4, a6, a3
800006e4: 13 17 37 00  	slli	a4, a4, 3

800006e8 <.LVL102>:
800006e8: 37 38 00 80  	lui	a6, 524291
800006ec: 13 08 88 23  	addi	a6, a6, 568
800006f0: b3 d5 e6 00  	srl	a1, a3, a4
800006f4: b3 85 05 01  	add	a1, a1, a6
800006f8: 03 c8 05 00  	lbu	a6, 0(a1)
800006fc: 93 05 00 02  	li	a1, 32
80000700: 33 08 e8 00  	add	a6, a6, a4
80000704: b3 85 05 41  	sub	a1, a1, a6

80000708 <.LVL103>:
80000708: 63 96 05 02  	bnez	a1, 0x80000734 <.LVL108>

8000070c <.L0 >:
8000070c: 13 07 10 00  	li	a4, 1

80000710 <.LVL104>:
80000710: e3 ec f6 ee  	bltu	a3, a5, 0x80000608 <.LVL65>
80000714: 33 35 c5 00  	sltu	a0, a0, a2

80000718 <.LVL105>:
80000718: 13 47 15 00  	xori	a4, a0, 1
8000071c: 6f f0 df ee  	j	0x80000608 <.LVL65>

80000720 <.LVL106>:
80000720: b7 05 00 01  	lui	a1, 4096

80000724 <.LVL107>:
80000724: 13 07 00 01  	li	a4, 16
80000728: e3 e0 b6 fc  	bltu	a3, a1, 0x800006e8 <.LVL102>
8000072c: 13 07 80 01  	li	a4, 24
80000730: 6f f0 9f fb  	j	0x800006e8 <.LVL102>

80000734 <.LVL108>:
80000734: 33 57 06 01  	srl	a4, a2, a6

80000738 <.LVL109>:
80000738: b3 96 b6 00  	sll	a3, a3, a1

8000073c <.LVL110>:
8000073c: b3 66 d7 00  	or	a3, a4, a3

80000740 <.LVL111>:
80000740: 93 de 06 01  	srli	t4, a3, 16

80000744 <.LBE21>:
80000744: 33 d7 07 01  	srl	a4, a5, a6

80000748 <.LBB22>:
80000748: b3 78 d7 03  	remu	a7, a4, t4

8000074c <.LVL112>:
8000074c: b3 97 b7 00  	sll	a5, a5, a1

80000750 <.LVL113>:
80000750: 33 58 05 01  	srl	a6, a0, a6

80000754 <.LVL114>:
80000754: 33 63 f8 00  	or	t1, a6, a5

80000758 <.LVL115>:
80000758: 93 97 06 01  	slli	a5, a3, 16
8000075c: 93 d7 07 01  	srli	a5, a5, 16
80000760: 13 58 03 01  	srli	a6, t1, 16

80000764 <.LBE23>:
80000764: 33 16 b6 00  	sll	a2, a2, a1

80000768 <.LVL116>:
80000768: 33 57 d7 03  	divu	a4, a4, t4

8000076c <.LVL117>:
8000076c: 93 98 08 01  	slli	a7, a7, 16

80000770 <.LVL118>:
80000770: 33 e8 08 01  	or	a6, a7, a6
80000774: 33 8f e7 02  	mul	t5, a5, a4

80000778 <.LVL119>:
80000778: 13 0e 07 00  	mv	t3, a4
8000077c: 63 7e e8 01  	bgeu	a6, t5, 0x80000798 <.L22>
80000780: 33 08 d8 00  	add	a6, a6, a3

80000784 <.LVL120>:
80000784: 13 0e f7 ff  	addi	t3, a4, -1

80000788 <.LVL121>:
80000788: 63 68 d8 00  	bltu	a6, a3, 0x80000798 <.L22>
8000078c: 63 76 e8 01  	bgeu	a6, t5, 0x80000798 <.L22>
80000790: 13 0e e7 ff  	addi	t3, a4, -2
80000794: 33 08 d8 00  	add	a6, a6, a3

80000798 <.L22>:
80000798: 33 08 e8 41  	sub	a6, a6, t5
8000079c: b3 78 d8 03  	remu	a7, a6, t4

800007a0 <.LVL125>:
800007a0: 33 58 d8 03  	divu	a6, a6, t4

800007a4 <.LVL126>:
800007a4: 93 98 08 01  	slli	a7, a7, 16

800007a8 <.LVL127>:
800007a8: b3 8e 07 03  	mul	t4, a5, a6

800007ac <.LVL128>:
800007ac: 93 17 03 01  	slli	a5, t1, 16

800007b0 <.LVL129>:
800007b0: 93 d7 07 01  	srli	a5, a5, 16
800007b4: b3 e7 f8 00  	or	a5, a7, a5

800007b8 <.LVL130>:
800007b8: 13 07 08 00  	mv	a4, a6
800007bc: 63 fe d7 01  	bgeu	a5, t4, 0x800007d8 <.L23>
800007c0: b3 87 d7 00  	add	a5, a5, a3

800007c4 <.LVL131>:
800007c4: 13 07 f8 ff  	addi	a4, a6, -1

800007c8 <.LVL132>:
800007c8: 63 e8 d7 00  	bltu	a5, a3, 0x800007d8 <.L23>
800007cc: 63 f6 d7 01  	bgeu	a5, t4, 0x800007d8 <.L23>
800007d0: 13 07 e8 ff  	addi	a4, a6, -2
800007d4: b3 87 d7 00  	add	a5, a5, a3

800007d8 <.L23>:
800007d8: 13 1e 0e 01  	slli	t3, t3, 16

800007dc <.LVL135>:
800007dc: b3 87 d7 41  	sub	a5, a5, t4

800007e0 <.LBE24>:
800007e0: b7 0e 01 00  	lui	t4, 16

800007e4 <.LVL137>:
800007e4: 33 67 ee 00  	or	a4, t3, a4

800007e8 <.LVL138>:
800007e8: 13 88 fe ff  	addi	a6, t4, -1

800007ec <.LVL139>:
800007ec: 33 73 07 01  	and	t1, a4, a6

800007f0 <.LVL140>:
800007f0: 93 58 07 01  	srli	a7, a4, 16

800007f4 <.LVL141>:
800007f4: 33 78 06 01  	and	a6, a2, a6

800007f8 <.LVL142>:
800007f8: 13 56 06 01  	srli	a2, a2, 16

800007fc <.LVL143>:
800007fc: 33 0e 03 03  	mul	t3, t1, a6

80000800 <.LVL144>:
80000800: 33 88 08 03  	mul	a6, a7, a6

80000804 <.LVL145>:
80000804: 93 56 0e 01  	srli	a3, t3, 16

80000808 <.LVL146>:
80000808: 33 03 c3 02  	mul	t1, t1, a2

8000080c <.LVL147>:
8000080c: 33 03 03 01  	add	t1, t1, a6

80000810 <.LVL148>:
80000810: b3 86 66 00  	add	a3, a3, t1
80000814: b3 88 c8 02  	mul	a7, a7, a2

80000818 <.LVL149>:
80000818: 63 f4 06 01  	bgeu	a3, a6, 0x80000820 <.L24>
8000081c: b3 88 d8 01  	add	a7, a7, t4

80000820 <.L24>:
80000820: 13 d6 06 01  	srli	a2, a3, 16

80000824 <.LVL151>:
80000824: b3 08 16 01  	add	a7, a2, a7

80000828 <.LVL152>:
80000828: 63 e6 17 03  	bltu	a5, a7, 0x80000854 <.LVL159>
8000082c: e3 9a 17 cf  	bne	a5, a7, 0x80000520 <.LVL35>

80000830 <.LBB28>:
80000830: b7 07 01 00  	lui	a5, 16

80000834 <.LVL153>:
80000834: 93 87 f7 ff  	addi	a5, a5, -1

80000838 <.LVL154>:
80000838: b3 f6 f6 00  	and	a3, a3, a5

8000083c <.LVL155>:
8000083c: 93 96 06 01  	slli	a3, a3, 16
80000840: 33 7e fe 00  	and	t3, t3, a5

80000844 <.LVL156>:
80000844: 33 15 b5 00  	sll	a0, a0, a1

80000848 <.LVL157>:
80000848: b3 86 c6 01  	add	a3, a3, t3

8000084c <.LBE29>:
8000084c: 93 05 00 00  	li	a1, 0

80000850 <.LVL158>:
80000850: e3 7c d5 da  	bgeu	a0, a3, 0x80000608 <.LVL65>

80000854 <.LVL159>:
80000854: 13 07 f7 ff  	addi	a4, a4, -1

80000858 <.LVL160>:
80000858: 6f f0 9f cc  	j	0x80000520 <.LVL35>

8000085c <.LVL161>:
8000085c: 93 05 00 00  	li	a1, 0

80000860 <.LVL162>:
80000860: 13 07 00 00  	li	a4, 0
80000864: 6f f0 5f da  	j	0x80000608 <.LVL65>

80000868 <memset>:
80000868: 13 08 f0 00  	li	a6, 15
8000086c: 13 07 05 00  	mv	a4, a0
80000870: 63 7e c8 02  	bgeu	a6, a2, 0x800008ac <.Ltiny>
80000874: 93 77 f7 00  	andi	a5, a4, 15
80000878: 63 90 07 0a  	bnez	a5, 0x80000918 <.Lmisaligned>

8000087c <.Laligned>:
8000087c: 63 92 05 08  	bnez	a1, 0x80000900 <.Lwordify>

80000880 <.Lwordified>:
80000880: 93 76 06 ff  	andi	a3, a2, -16
80000884: 13 76 f6 00  	andi	a2, a2, 15
80000888: b3 86 e6 00  	add	a3, a3, a4

8000088c <.L11>:
8000088c: 23 20 b7 00  	sw	a1, 0(a4)
80000890: 23 22 b7 00  	sw	a1, 4(a4)
80000894: 23 24 b7 00  	sw	a1, 8(a4)
80000898: 23 26 b7 00  	sw	a1, 12(a4)
8000089c: 13 07 07 01  	addi	a4, a4, 16
800008a0: e3 66 d7 fe  	bltu	a4, a3, 0x8000088c <.L11>
800008a4: 63 14 06 00  	bnez	a2, 0x800008ac <.Ltiny>
800008a8: 67 80 00 00  	ret

800008ac <.Ltiny>:
800008ac: b3 06 c8 40  	sub	a3, a6, a2
800008b0: 93 96 26 00  	slli	a3, a3, 2

800008b4 <.L12>:
800008b4: 97 02 00 00  	auipc	t0, 0
800008b8: b3 86 56 00  	add	a3, a3, t0

800008bc <.Ltable_misaligned>:
800008bc: 67 80 c6 00  	jr	12(a3)

800008c0 <.Ltable>:
800008c0: 23 07 b7 00  	sb	a1, 14(a4)
800008c4: a3 06 b7 00  	sb	a1, 13(a4)
800008c8: 23 06 b7 00  	sb	a1, 12(a4)
800008cc: a3 05 b7 00  	sb	a1, 11(a4)
800008d0: 23 05 b7 00  	sb	a1, 10(a4)
800008d4: a3 04 b7 00  	sb	a1, 9(a4)
800008d8: 23 04 b7 00  	sb	a1, 8(a4)
800008dc: a3 03 b7 00  	sb	a1, 7(a4)
800008e0: 23 03 b7 00  	sb	a1, 6(a4)
800008e4: a3 02 b7 00  	sb	a1, 5(a4)
800008e8: 23 02 b7 00  	sb	a1, 4(a4)
800008ec: a3 01 b7 00  	sb	a1, 3(a4)
800008f0: 23 01 b7 00  	sb	a1, 2(a4)
800008f4: a3 00 b7 00  	sb	a1, 1(a4)
800008f8: 23 00 b7 00  	sb	a1, 0(a4)
800008fc: 67 80 00 00  	ret

80000900 <.Lwordify>:
80000900: 93 f5 f5 0f  	andi	a1, a1, 255
80000904: 93 96 85 00  	slli	a3, a1, 8
80000908: b3 e5 d5 00  	or	a1, a1, a3
8000090c: 93 96 05 01  	slli	a3, a1, 16
80000910: b3 e5 d5 00  	or	a1, a1, a3
80000914: 6f f0 df f6  	j	0x80000880 <.Lwordified>

80000918 <.Lmisaligned>:
80000918: 93 96 27 00  	slli	a3, a5, 2

8000091c <.L13>:
8000091c: 97 02 00 00  	auipc	t0, 0
80000920: b3 86 56 00  	add	a3, a3, t0
80000924: 93 82 00 00  	mv	t0, ra
80000928: e7 80 06 fa  	jalr	-96(a3)
8000092c: 93 80 02 00  	mv	ra, t0
80000930: 93 87 07 ff  	addi	a5, a5, -16
80000934: 33 07 f7 40  	sub	a4, a4, a5
80000938: 33 06 f6 00  	add	a2, a2, a5
8000093c: e3 78 c8 f6  	bgeu	a6, a2, 0x800008ac <.Ltiny>
80000940: 6f f0 df f3  	j	0x8000087c <.Laligned>
		...

80000950 <main>:
; int main() {
80000950: 13 01 01 fe  	addi	sp, sp, -32
;     volatile int errors = 0;
80000954: 23 2e 11 00  	sw	ra, 28(sp)
80000958: 23 2c 81 00  	sw	s0, 24(sp)
8000095c: 23 2a 91 00  	sw	s1, 20(sp)
80000960: 23 28 21 01  	sw	s2, 16(sp)
80000964: 23 26 31 01  	sw	s3, 12(sp)
80000968: 23 24 41 01  	sw	s4, 8(sp)
8000096c: 23 22 51 01  	sw	s5, 4(sp)
80000970: 23 20 01 00  	sw	zero, 0(sp)
;     const unsigned int num_cores = snrt_cluster_core_num();
80000974: 97 20 00 00  	auipc	ra, 2
80000978: e7 80 80 4f  	jalr	1272(ra)
8000097c: 13 0a 05 00  	mv	s4, a0
;     const unsigned int cid       = snrt_cluster_core_idx();
80000980: 97 20 00 00  	auipc	ra, 2
80000984: e7 80 c0 4d  	jalr	1244(ra)
80000988: 13 04 05 00  	mv	s0, a0
;     if (cid == 0) {
8000098c: 63 0e 05 00  	beqz	a0, 0x800009a8 <main+0x58>
;     snrt_cluster_hw_barrier();
80000990: 97 00 00 00  	auipc	ra, 0
80000994: e7 80 40 66  	jalr	1636(ra)
; size_t benchmark_get_cycle() { return read_csr(mcycle); }
80000998: 73 25 00 b0  	csrr	a0, mcycle
;     snrt_cluster_hw_barrier();
8000099c: 97 00 00 00  	auipc	ra, 0
800009a0: e7 80 80 65  	jalr	1624(ra)
800009a4: 6f 00 c0 09  	j	0x80000a40 <main+0xf0>
;         snrt_dma_wait_all();
800009a8: 97 00 00 00  	auipc	ra, 0
800009ac: e7 80 40 67  	jalr	1652(ra)
;     snrt_cluster_hw_barrier();
800009b0: 97 00 00 00  	auipc	ra, 0
800009b4: e7 80 40 64  	jalr	1604(ra)
; size_t benchmark_get_cycle() { return read_csr(mcycle); }
800009b8: 73 25 00 b0  	csrr	a0, mcycle
;         inp     = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
800009bc: 37 85 00 00  	lui	a0, 8
800009c0: 97 20 00 00  	auipc	ra, 2
800009c4: e7 80 c0 4d  	jalr	1244(ra)
800009c8: 93 04 05 00  	mv	s1, a0
;         weight  = (__fp16*)snrt_l1alloc(1 * OC* C * sizeof(__fp16));
800009cc: 37 85 00 00  	lui	a0, 8
800009d0: 97 20 00 00  	auipc	ra, 2
800009d4: e7 80 c0 4c  	jalr	1228(ra)
800009d8: 13 09 05 00  	mv	s2, a0
;         out     = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
800009dc: 37 85 00 00  	lui	a0, 8
800009e0: 97 20 00 00  	auipc	ra, 2
800009e4: e7 80 c0 4b  	jalr	1212(ra)
800009e8: 93 09 05 00  	mv	s3, a0
;     snrt_cluster_hw_barrier();
800009ec: 97 00 00 00  	auipc	ra, 0
800009f0: e7 80 80 60  	jalr	1544(ra)
;         snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(__fp16));
800009f4: 37 55 00 80  	lui	a0, 524293
800009f8: 93 0a 05 69  	addi	s5, a0, 1680
800009fc: 37 86 00 00  	lui	a2, 8
80000a00: 13 85 04 00  	mv	a0, s1
80000a04: 93 85 0a 00  	mv	a1, s5
80000a08: 97 00 00 00  	auipc	ra, 0
80000a0c: e7 80 40 5f  	jalr	1524(ra)
;         snrt_dma_start_1d(weight , data1_dram, 1 * OC* C * sizeof(__fp16));
80000a10: 37 86 00 00  	lui	a2, 8
80000a14: 13 05 09 00  	mv	a0, s2
80000a18: 93 85 0a 00  	mv	a1, s5
80000a1c: 97 00 00 00  	auipc	ra, 0
80000a20: e7 80 00 5e  	jalr	1504(ra)
;         snrt_dma_start_1d(out    , data1_dram, B * T * C * sizeof(__fp16));
80000a24: 37 86 00 00  	lui	a2, 8
80000a28: 13 85 09 00  	mv	a0, s3
80000a2c: 93 85 0a 00  	mv	a1, s5
80000a30: 97 00 00 00  	auipc	ra, 0
80000a34: e7 80 c0 5c  	jalr	1484(ra)
;         snrt_dma_wait_all();
80000a38: 97 00 00 00  	auipc	ra, 0
80000a3c: e7 80 40 5e  	jalr	1508(ra)
80000a40: 13 05 00 08  	li	a0, 128
;     m_start = (gemm_l.M / num_cores) * cid;
80000a44: b3 55 45 03  	divu	a1, a0, s4
80000a48: 33 85 85 02  	mul	a0, a1, s0
;     m_end = (gemm_l.M / num_cores) * (cid + 1);
80000a4c: 13 06 14 00  	addi	a2, s0, 1
80000a50: b3 85 c5 02  	mul	a1, a1, a2
; size_t benchmark_get_cycle() { return read_csr(mcycle); }
80000a54: 73 2a 00 b0  	csrr	s4, mcycle
;     if (cid == 0)
80000a58: 63 0c 04 24  	beqz	s0, 0x80000cb0 <.LBB30_33>
;     for (unsigned int m = m_start; m < m_end; m += 8) {
80000a5c: 63 7a b5 4c  	bgeu	a0, a1, 0x80000f30 <.LBB30_33+0x280>
80000a60: 13 06 00 00  	li	a2, 0
;   while (p < p_end) {
80000a64: 93 06 09 20  	addi	a3, s2, 512
80000a68: 13 07 00 08  	li	a4, 128
80000a6c: 6f 00 c0 00  	j	0x80000a78 <main+0x128>
;     p += gvl;
80000a70: 33 86 c7 00  	add	a2, a5, a2
;   while (p < p_end) {
80000a74: 63 7e e6 4c  	bgeu	a2, a4, 0x80000f50 <.LBB30_33+0x2a0>
;                  : [vl] "r"(p_end - p));
80000a78: b3 07 c7 40  	sub	a5, a4, a2
;     asm volatile("vsetvli %[gvl], %[vl], e16, m2, ta, ma"
80000a7c: d7 f7 97 0c  	vsetvli	a5, a5, e16, m2, ta, ma
;     const __fp16 *b_ = b + p;
80000a80: 93 12 16 00  	slli	t0, a2, 1
80000a84: 33 08 59 00  	add	a6, s2, t0
;     __fp16 *c_ = c + p;
80000a88: b3 88 59 00  	add	a7, s3, t0
;     for (unsigned int m = m_start; m < m_end; m += 8) {
80000a8c: b3 82 56 00  	add	t0, a3, t0
80000a90: 13 03 05 00  	mv	t1, a0
80000a94: 6f 00 00 07  	j	0x80000b04 <main+0x1b4>
80000a98: 13 00 00 00  	nop
80000a9c: 13 00 00 00  	nop
;       asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
80000aa0: 57 d0 43 b3  	vfmacc.vf	v0, ft7, v20
;       asm volatile("vse16.v v0, (%0);" ::"r"(c__));
80000aa4: 27 d0 03 02  	vse16.v	v0, (t2)
;       c__ += P;
80000aa8: 13 8e 03 10  	addi	t3, t2, 256
;       asm volatile("vfmacc.vf v2, %0, v20" ::"f"(t1));
80000aac: 57 51 43 b3  	vfmacc.vf	v2, ft6, v20
;       asm volatile("vse16.v v2, (%0);" ::"r"(c__));
80000ab0: 27 51 0e 02  	vse16.v	v2, (t3)
;       c__ += P;
80000ab4: 13 8e 03 20  	addi	t3, t2, 512
;       asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t2));
80000ab8: 57 d2 42 b3  	vfmacc.vf	v4, ft5, v20
;       asm volatile("vse16.v v4, (%0);" ::"r"(c__));
80000abc: 27 52 0e 02  	vse16.v	v4, (t3)
;       c__ += P;
80000ac0: 13 8e 03 30  	addi	t3, t2, 768
;       asm volatile("vfmacc.vf v6, %0, v20" ::"f"(t3));
80000ac4: 57 53 42 b3  	vfmacc.vf	v6, ft4, v20
;       asm volatile("vse16.v v6, (%0);" ::"r"(c__));
80000ac8: 27 53 0e 02  	vse16.v	v6, (t3)
;       c__ += P;
80000acc: 13 8e 03 40  	addi	t3, t2, 1024
;       asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t4));
80000ad0: 57 d4 41 b3  	vfmacc.vf	v8, ft3, v20
;       asm volatile("vse16.v v8, (%0);" ::"r"(c__));
80000ad4: 27 54 0e 02  	vse16.v	v8, (t3)
;       c__ += P;
80000ad8: 13 8e 03 50  	addi	t3, t2, 1280
;       asm volatile("vfmacc.vf v10, %0, v20" ::"f"(t5));
80000adc: 57 55 41 b3  	vfmacc.vf	v10, ft2, v20
;       asm volatile("vse16.v v10, (%0);" ::"r"(c__));
80000ae0: 27 55 0e 02  	vse16.v	v10, (t3)
;       c__ += P;
80000ae4: 13 8e 03 60  	addi	t3, t2, 1536
;       asm volatile("vfmacc.vf v12, %0, v20" ::"f"(t6));
80000ae8: 57 d6 40 b3  	vfmacc.vf	v12, ft1, v20
;       asm volatile("vse16.v v12, (%0);" ::"r"(c__));
80000aec: 27 56 0e 02  	vse16.v	v12, (t3)
;       c__ += P;
80000af0: 93 83 03 70  	addi	t2, t2, 1792
;       asm volatile("vfmacc.vf v14, %0, v20" ::"f"(t7));
80000af4: 57 57 40 b3  	vfmacc.vf	v14, ft0, v20
;       asm volatile("vse16.v v14, (%0);" ::"r"(c__));
80000af8: 27 d7 03 02  	vse16.v	v14, (t2)
;     for (unsigned int m = m_start; m < m_end; m += 8) {
80000afc: 13 03 83 00  	addi	t1, t1, 8
80000b00: e3 78 b3 f6  	bgeu	t1, a1, 0x80000a70 <main+0x120>
80000b04: 13 0f 00 00  	li	t5, 0
;       const __fp16 *a_ = a + m * N;
80000b08: 93 13 83 00  	slli	t2, t1, 8
80000b0c: 33 8e 74 00  	add	t3, s1, t2
;       asm volatile("vle16.v v18, (%0);" ::"r"(b_));
80000b10: 07 59 08 02  	vle16.v	v18, (a6)
;       __fp16 *c__ = c_ + m * P;
80000b14: b3 83 78 00  	add	t2, a7, t2
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
80000b18: 87 13 0e 00  	flh	ft7, 0(t3)
;       a__ += N;
80000b1c: 93 0e 0e 10  	addi	t4, t3, 256
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
80000b20: 07 93 0e 00  	flh	ft6, 0(t4)
;       a__ += N;
80000b24: 93 0e 0e 20  	addi	t4, t3, 512
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
80000b28: 87 92 0e 00  	flh	ft5, 0(t4)
;       a__ += N;
80000b2c: 93 0e 0e 30  	addi	t4, t3, 768
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
80000b30: 07 92 0e 00  	flh	ft4, 0(t4)
;       a__ += N;
80000b34: 93 0e 0e 40  	addi	t4, t3, 1024
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
80000b38: 87 91 0e 00  	flh	ft3, 0(t4)
;       a__ += N;
80000b3c: 93 0e 0e 50  	addi	t4, t3, 1280
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
80000b40: 07 91 0e 00  	flh	ft2, 0(t4)
;       a__ += N;
80000b44: 93 0e 0e 60  	addi	t4, t3, 1536
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
80000b48: 87 90 0e 00  	flh	ft1, 0(t4)
;       a__ += N;
80000b4c: 93 0e 0e 70  	addi	t4, t3, 1792
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));
80000b50: 07 90 0e 00  	flh	ft0, 0(t4)
80000b54: 93 8e 02 00  	mv	t4, t0
80000b58: 6f 00 80 0d  	j	0x80000c30 <main+0x2e0>
80000b5c: 13 00 00 00  	nop
;           asm volatile("vfmul.vf v0, v18, %0" ::"f"(t0));
80000b60: 57 d0 23 93  	vfmul.vf	v0, v18, ft7
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
80000b64: 87 93 0f 00  	flh	ft7, 0(t6)
;           a__ += N;
80000b68: 13 8f 0f 10  	addi	t5, t6, 256
;           asm volatile("vfmul.vf v2, v18, %0" ::"f"(t1));
80000b6c: 57 51 23 93  	vfmul.vf	v2, v18, ft6
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
80000b70: 07 13 0f 00  	flh	ft6, 0(t5)
;           a__ += N;
80000b74: 13 8f 0f 20  	addi	t5, t6, 512
;           asm volatile("vfmul.vf v4, v18, %0" ::"f"(t2));
80000b78: 57 d2 22 93  	vfmul.vf	v4, v18, ft5
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
80000b7c: 87 12 0f 00  	flh	ft5, 0(t5)
;           a__ += N;
80000b80: 13 8f 0f 30  	addi	t5, t6, 768
;           asm volatile("vfmul.vf v6, v18, %0" ::"f"(t3));
80000b84: 57 53 22 93  	vfmul.vf	v6, v18, ft4
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
80000b88: 07 12 0f 00  	flh	ft4, 0(t5)
;           a__ += N;
80000b8c: 13 8f 0f 40  	addi	t5, t6, 1024
;           asm volatile("vfmul.vf v8, v18, %0" ::"f"(t4));
80000b90: 57 d4 21 93  	vfmul.vf	v8, v18, ft3
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
80000b94: 87 11 0f 00  	flh	ft3, 0(t5)
;           a__ += N;
80000b98: 13 8f 0f 50  	addi	t5, t6, 1280
;           asm volatile("vfmul.vf v10, v18, %0" ::"f"(t5));
80000b9c: 57 55 21 93  	vfmul.vf	v10, v18, ft2
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
80000ba0: 07 11 0f 00  	flh	ft2, 0(t5)
;           a__ += N;
80000ba4: 13 8f 0f 60  	addi	t5, t6, 1536
;           asm volatile("vfmul.vf v12, v18, %0" ::"f"(t6));
80000ba8: 57 d6 20 93  	vfmul.vf	v12, v18, ft1
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
80000bac: 87 10 0f 00  	flh	ft1, 0(t5)
;           a__ += N;
80000bb0: 13 8f 0f 70  	addi	t5, t6, 1792
;           asm volatile("vfmul.vf v14, v18, %0" ::"f"(t7));
80000bb4: 57 57 20 93  	vfmul.vf	v14, v18, ft0
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));
80000bb8: 07 10 0f 00  	flh	ft0, 0(t5)
80000bbc: 13 0f 20 00  	li	t5, 2
;         a__ = a_ + ++n;
80000bc0: 93 1f 1f 00  	slli	t6, t5, 1
80000bc4: b3 0f fe 01  	add	t6, t3, t6
;         asm volatile("vle16.v v18, (%0);" ::"r"(b__));
80000bc8: 07 d9 0e 02  	vle16.v	v18, (t4)
;         asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
80000bcc: 57 d0 43 b3  	vfmacc.vf	v0, ft7, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
80000bd0: 87 93 0f 00  	flh	ft7, 0(t6)
;         a__ += N;
80000bd4: 93 8a 0f 10  	addi	s5, t6, 256
;         asm volatile("vfmacc.vf v2, %0, v20" ::"f"(t1));
80000bd8: 57 51 43 b3  	vfmacc.vf	v2, ft6, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
80000bdc: 07 93 0a 00  	flh	ft6, 0(s5)
;         a__ += N;
80000be0: 93 8a 0f 20  	addi	s5, t6, 512
;         asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t2));
80000be4: 57 d2 42 b3  	vfmacc.vf	v4, ft5, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
80000be8: 87 92 0a 00  	flh	ft5, 0(s5)
;         a__ += N;
80000bec: 93 8a 0f 30  	addi	s5, t6, 768
;         asm volatile("vfmacc.vf v6, %0, v20" ::"f"(t3));
80000bf0: 57 53 42 b3  	vfmacc.vf	v6, ft4, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
80000bf4: 07 92 0a 00  	flh	ft4, 0(s5)
;         a__ += N;
80000bf8: 93 8a 0f 40  	addi	s5, t6, 1024
;         asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t4));
80000bfc: 57 d4 41 b3  	vfmacc.vf	v8, ft3, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
80000c00: 87 91 0a 00  	flh	ft3, 0(s5)
;         a__ += N;
80000c04: 93 8a 0f 50  	addi	s5, t6, 1280
;         asm volatile("vfmacc.vf v10, %0, v20" ::"f"(t5));
80000c08: 57 55 41 b3  	vfmacc.vf	v10, ft2, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
80000c0c: 07 91 0a 00  	flh	ft2, 0(s5)
;         a__ += N;
80000c10: 93 8a 0f 60  	addi	s5, t6, 1536
;         asm volatile("vfmacc.vf v12, %0, v20" ::"f"(t6));
80000c14: 57 d6 40 b3  	vfmacc.vf	v12, ft1, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
80000c18: 87 90 0a 00  	flh	ft1, 0(s5)
;         a__ += N;
80000c1c: 93 8f 0f 70  	addi	t6, t6, 1792
;         asm volatile("vfmacc.vf v14, %0, v20" ::"f"(t7));
80000c20: 57 57 40 b3  	vfmacc.vf	v14, ft0, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));
80000c24: 07 90 0f 00  	flh	ft0, 0(t6)
;       while (n < N) {
80000c28: 93 8e 0e 20  	addi	t4, t4, 512
80000c2c: e3 7a ef e6  	bgeu	t5, a4, 0x80000aa0 <main+0x150>
;         a__ = a_ + ++n;
80000c30: 93 8a 0e f0  	addi	s5, t4, -256
80000c34: 93 1f 1f 00  	slli	t6, t5, 1
80000c38: 93 ef 2f 00  	ori	t6, t6, 2
80000c3c: b3 0f fe 01  	add	t6, t3, t6
;         asm volatile("vle16.v v20, (%0);" ::"r"(b__));
80000c40: 07 da 0a 02  	vle16.v	v20, (s5)
;         if (n == 1) {
80000c44: e3 0e 0f f0  	beqz	t5, 0x80000b60 <main+0x210>
;           asm volatile("vfmacc.vf v0, %0, v18" ::"f"(t0));
80000c48: 57 d0 23 b3  	vfmacc.vf	v0, ft7, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
80000c4c: 87 93 0f 00  	flh	ft7, 0(t6)
;           a__ += N;
80000c50: 93 8a 0f 10  	addi	s5, t6, 256
;           asm volatile("vfmacc.vf v2, %0, v18" ::"f"(t1));
80000c54: 57 51 23 b3  	vfmacc.vf	v2, ft6, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
80000c58: 07 93 0a 00  	flh	ft6, 0(s5)
;           a__ += N;
80000c5c: 93 8a 0f 20  	addi	s5, t6, 512
;           asm volatile("vfmacc.vf v4, %0, v18" ::"f"(t2));
80000c60: 57 d2 22 b3  	vfmacc.vf	v4, ft5, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
80000c64: 87 92 0a 00  	flh	ft5, 0(s5)
;           a__ += N;
80000c68: 93 8a 0f 30  	addi	s5, t6, 768
;           asm volatile("vfmacc.vf v6, %0, v18" ::"f"(t3));
80000c6c: 57 53 22 b3  	vfmacc.vf	v6, ft4, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
80000c70: 07 92 0a 00  	flh	ft4, 0(s5)
;           a__ += N;
80000c74: 93 8a 0f 40  	addi	s5, t6, 1024
;           asm volatile("vfmacc.vf v8, %0, v18" ::"f"(t4));
80000c78: 57 d4 21 b3  	vfmacc.vf	v8, ft3, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
80000c7c: 87 91 0a 00  	flh	ft3, 0(s5)
;           a__ += N;
80000c80: 93 8a 0f 50  	addi	s5, t6, 1280
;           asm volatile("vfmacc.vf v10, %0, v18" ::"f"(t5));
80000c84: 57 55 21 b3  	vfmacc.vf	v10, ft2, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
80000c88: 07 91 0a 00  	flh	ft2, 0(s5)
;           a__ += N;
80000c8c: 93 8a 0f 60  	addi	s5, t6, 1536
;           asm volatile("vfmacc.vf v12, %0, v18" ::"f"(t6));
80000c90: 57 d6 20 b3  	vfmacc.vf	v12, ft1, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
80000c94: 87 90 0a 00  	flh	ft1, 0(s5)
;           a__ += N;
80000c98: 93 8f 0f 70  	addi	t6, t6, 1792
;           asm volatile("vfmacc.vf v14, %0, v18" ::"f"(t7));
80000c9c: 57 57 20 b3  	vfmacc.vf	v14, ft0, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));
80000ca0: 07 90 0f 00  	flh	ft0, 0(t6)
;         a__ = a_ + ++n;
80000ca4: 13 0f 2f 00  	addi	t5, t5, 2
80000ca8: e3 1c ef f0  	bne	t5, a4, 0x80000bc0 <main+0x270>
80000cac: 6f f0 5f df  	j	0x80000aa0 <main+0x150>

80000cb0 <.LBB30_33>:
;       (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
80000cb0: 17 36 00 00  	auipc	a2, 3
80000cb4: 03 26 c6 91  	lw	a2, -1764(a2)
80000cb8: 93 06 10 00  	li	a3, 1
80000cbc: 33 06 46 00  	add	a2, a2, tp
80000cc0: 03 26 06 00  	lw	a2, 0(a2)
80000cc4: 03 26 06 00  	lw	a2, 0(a2)
80000cc8: 03 26 86 03  	lw	a2, 56(a2)
;   *bench = 1;
80000ccc: 23 28 d6 04  	sw	a3, 80(a2)
;     for (unsigned int m = m_start; m < m_end; m += 8) {
80000cd0: 63 7c b5 28  	bgeu	a0, a1, 0x80000f68 <.LBB30_33+0x2b8>
80000cd4: 13 06 00 00  	li	a2, 0
;   while (p < p_end) {
80000cd8: 93 06 09 20  	addi	a3, s2, 512
80000cdc: 13 07 00 08  	li	a4, 128
80000ce0: 6f 00 80 01  	j	0x80000cf8 <.LBB30_33+0x48>
80000ce4: 13 00 00 00  	nop
80000ce8: 13 00 00 00  	nop
80000cec: 13 00 00 00  	nop
;     p += gvl;
80000cf0: 33 86 c7 00  	add	a2, a5, a2
;   while (p < p_end) {
80000cf4: 63 76 e6 28  	bgeu	a2, a4, 0x80000f80 <.LBB30_33+0x2d0>
;                  : [vl] "r"(p_end - p));
80000cf8: b3 07 c7 40  	sub	a5, a4, a2
;     asm volatile("vsetvli %[gvl], %[vl], e16, m2, ta, ma"
80000cfc: d7 f7 97 0c  	vsetvli	a5, a5, e16, m2, ta, ma
;     const __fp16 *b_ = b + p;
80000d00: 93 12 16 00  	slli	t0, a2, 1
80000d04: 33 08 59 00  	add	a6, s2, t0
;     __fp16 *c_ = c + p;
80000d08: b3 88 59 00  	add	a7, s3, t0
;     for (unsigned int m = m_start; m < m_end; m += 8) {
80000d0c: b3 82 56 00  	add	t0, a3, t0
80000d10: 13 03 05 00  	mv	t1, a0
80000d14: 6f 00 00 07  	j	0x80000d84 <.LBB30_33+0xd4>
80000d18: 13 00 00 00  	nop
80000d1c: 13 00 00 00  	nop
;       asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
80000d20: 57 d0 43 b3  	vfmacc.vf	v0, ft7, v20
;       asm volatile("vse16.v v0, (%0);" ::"r"(c__));
80000d24: 27 d0 03 02  	vse16.v	v0, (t2)
;       c__ += P;
80000d28: 13 8e 03 10  	addi	t3, t2, 256
;       asm volatile("vfmacc.vf v2, %0, v20" ::"f"(t1));
80000d2c: 57 51 43 b3  	vfmacc.vf	v2, ft6, v20
;       asm volatile("vse16.v v2, (%0);" ::"r"(c__));
80000d30: 27 51 0e 02  	vse16.v	v2, (t3)
;       c__ += P;
80000d34: 13 8e 03 20  	addi	t3, t2, 512
;       asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t2));
80000d38: 57 d2 42 b3  	vfmacc.vf	v4, ft5, v20
;       asm volatile("vse16.v v4, (%0);" ::"r"(c__));
80000d3c: 27 52 0e 02  	vse16.v	v4, (t3)
;       c__ += P;
80000d40: 13 8e 03 30  	addi	t3, t2, 768
;       asm volatile("vfmacc.vf v6, %0, v20" ::"f"(t3));
80000d44: 57 53 42 b3  	vfmacc.vf	v6, ft4, v20
;       asm volatile("vse16.v v6, (%0);" ::"r"(c__));
80000d48: 27 53 0e 02  	vse16.v	v6, (t3)
;       c__ += P;
80000d4c: 13 8e 03 40  	addi	t3, t2, 1024
;       asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t4));
80000d50: 57 d4 41 b3  	vfmacc.vf	v8, ft3, v20
;       asm volatile("vse16.v v8, (%0);" ::"r"(c__));
80000d54: 27 54 0e 02  	vse16.v	v8, (t3)
;       c__ += P;
80000d58: 13 8e 03 50  	addi	t3, t2, 1280
;       asm volatile("vfmacc.vf v10, %0, v20" ::"f"(t5));
80000d5c: 57 55 41 b3  	vfmacc.vf	v10, ft2, v20
;       asm volatile("vse16.v v10, (%0);" ::"r"(c__));
80000d60: 27 55 0e 02  	vse16.v	v10, (t3)
;       c__ += P;
80000d64: 13 8e 03 60  	addi	t3, t2, 1536
;       asm volatile("vfmacc.vf v12, %0, v20" ::"f"(t6));
80000d68: 57 d6 40 b3  	vfmacc.vf	v12, ft1, v20
;       asm volatile("vse16.v v12, (%0);" ::"r"(c__));
80000d6c: 27 56 0e 02  	vse16.v	v12, (t3)
;       c__ += P;
80000d70: 93 83 03 70  	addi	t2, t2, 1792
;       asm volatile("vfmacc.vf v14, %0, v20" ::"f"(t7));
80000d74: 57 57 40 b3  	vfmacc.vf	v14, ft0, v20
;       asm volatile("vse16.v v14, (%0);" ::"r"(c__));
80000d78: 27 d7 03 02  	vse16.v	v14, (t2)
;     for (unsigned int m = m_start; m < m_end; m += 8) {
80000d7c: 13 03 83 00  	addi	t1, t1, 8
80000d80: e3 78 b3 f6  	bgeu	t1, a1, 0x80000cf0 <.LBB30_33+0x40>
80000d84: 13 0f 00 00  	li	t5, 0
;       const __fp16 *a_ = a + m * N;
80000d88: 93 13 83 00  	slli	t2, t1, 8
80000d8c: 33 8e 74 00  	add	t3, s1, t2
;       asm volatile("vle16.v v18, (%0);" ::"r"(b_));
80000d90: 07 59 08 02  	vle16.v	v18, (a6)
;       __fp16 *c__ = c_ + m * P;
80000d94: b3 83 78 00  	add	t2, a7, t2
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
80000d98: 87 13 0e 00  	flh	ft7, 0(t3)
;       a__ += N;
80000d9c: 93 0e 0e 10  	addi	t4, t3, 256
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
80000da0: 07 93 0e 00  	flh	ft6, 0(t4)
;       a__ += N;
80000da4: 93 0e 0e 20  	addi	t4, t3, 512
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
80000da8: 87 92 0e 00  	flh	ft5, 0(t4)
;       a__ += N;
80000dac: 93 0e 0e 30  	addi	t4, t3, 768
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
80000db0: 07 92 0e 00  	flh	ft4, 0(t4)
;       a__ += N;
80000db4: 93 0e 0e 40  	addi	t4, t3, 1024
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
80000db8: 87 91 0e 00  	flh	ft3, 0(t4)
;       a__ += N;
80000dbc: 93 0e 0e 50  	addi	t4, t3, 1280
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
80000dc0: 07 91 0e 00  	flh	ft2, 0(t4)
;       a__ += N;
80000dc4: 93 0e 0e 60  	addi	t4, t3, 1536
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
80000dc8: 87 90 0e 00  	flh	ft1, 0(t4)
;       a__ += N;
80000dcc: 93 0e 0e 70  	addi	t4, t3, 1792
;       asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));
80000dd0: 07 90 0e 00  	flh	ft0, 0(t4)
80000dd4: 93 8e 02 00  	mv	t4, t0
80000dd8: 6f 00 80 0d  	j	0x80000eb0 <.LBB30_33+0x200>
80000ddc: 13 00 00 00  	nop
;           asm volatile("vfmul.vf v0, v18, %0" ::"f"(t0));
80000de0: 57 d0 23 93  	vfmul.vf	v0, v18, ft7
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
80000de4: 87 93 0f 00  	flh	ft7, 0(t6)
;           a__ += N;
80000de8: 13 8f 0f 10  	addi	t5, t6, 256
;           asm volatile("vfmul.vf v2, v18, %0" ::"f"(t1));
80000dec: 57 51 23 93  	vfmul.vf	v2, v18, ft6
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
80000df0: 07 13 0f 00  	flh	ft6, 0(t5)
;           a__ += N;
80000df4: 13 8f 0f 20  	addi	t5, t6, 512
;           asm volatile("vfmul.vf v4, v18, %0" ::"f"(t2));
80000df8: 57 d2 22 93  	vfmul.vf	v4, v18, ft5
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
80000dfc: 87 12 0f 00  	flh	ft5, 0(t5)
;           a__ += N;
80000e00: 13 8f 0f 30  	addi	t5, t6, 768
;           asm volatile("vfmul.vf v6, v18, %0" ::"f"(t3));
80000e04: 57 53 22 93  	vfmul.vf	v6, v18, ft4
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
80000e08: 07 12 0f 00  	flh	ft4, 0(t5)
;           a__ += N;
80000e0c: 13 8f 0f 40  	addi	t5, t6, 1024
;           asm volatile("vfmul.vf v8, v18, %0" ::"f"(t4));
80000e10: 57 d4 21 93  	vfmul.vf	v8, v18, ft3
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
80000e14: 87 11 0f 00  	flh	ft3, 0(t5)
;           a__ += N;
80000e18: 13 8f 0f 50  	addi	t5, t6, 1280
;           asm volatile("vfmul.vf v10, v18, %0" ::"f"(t5));
80000e1c: 57 55 21 93  	vfmul.vf	v10, v18, ft2
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
80000e20: 07 11 0f 00  	flh	ft2, 0(t5)
;           a__ += N;
80000e24: 13 8f 0f 60  	addi	t5, t6, 1536
;           asm volatile("vfmul.vf v12, v18, %0" ::"f"(t6));
80000e28: 57 d6 20 93  	vfmul.vf	v12, v18, ft1
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
80000e2c: 87 10 0f 00  	flh	ft1, 0(t5)
;           a__ += N;
80000e30: 13 8f 0f 70  	addi	t5, t6, 1792
;           asm volatile("vfmul.vf v14, v18, %0" ::"f"(t7));
80000e34: 57 57 20 93  	vfmul.vf	v14, v18, ft0
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));
80000e38: 07 10 0f 00  	flh	ft0, 0(t5)
80000e3c: 13 0f 20 00  	li	t5, 2
;         a__ = a_ + ++n;
80000e40: 93 1f 1f 00  	slli	t6, t5, 1
80000e44: b3 0f fe 01  	add	t6, t3, t6
;         asm volatile("vle16.v v18, (%0);" ::"r"(b__));
80000e48: 07 d9 0e 02  	vle16.v	v18, (t4)
;         asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
80000e4c: 57 d0 43 b3  	vfmacc.vf	v0, ft7, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
80000e50: 87 93 0f 00  	flh	ft7, 0(t6)
;         a__ += N;
80000e54: 93 8a 0f 10  	addi	s5, t6, 256
;         asm volatile("vfmacc.vf v2, %0, v20" ::"f"(t1));
80000e58: 57 51 43 b3  	vfmacc.vf	v2, ft6, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
80000e5c: 07 93 0a 00  	flh	ft6, 0(s5)
;         a__ += N;
80000e60: 93 8a 0f 20  	addi	s5, t6, 512
;         asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t2));
80000e64: 57 d2 42 b3  	vfmacc.vf	v4, ft5, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
80000e68: 87 92 0a 00  	flh	ft5, 0(s5)
;         a__ += N;
80000e6c: 93 8a 0f 30  	addi	s5, t6, 768
;         asm volatile("vfmacc.vf v6, %0, v20" ::"f"(t3));
80000e70: 57 53 42 b3  	vfmacc.vf	v6, ft4, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
80000e74: 07 92 0a 00  	flh	ft4, 0(s5)
;         a__ += N;
80000e78: 93 8a 0f 40  	addi	s5, t6, 1024
;         asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t4));
80000e7c: 57 d4 41 b3  	vfmacc.vf	v8, ft3, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
80000e80: 87 91 0a 00  	flh	ft3, 0(s5)
;         a__ += N;
80000e84: 93 8a 0f 50  	addi	s5, t6, 1280
;         asm volatile("vfmacc.vf v10, %0, v20" ::"f"(t5));
80000e88: 57 55 41 b3  	vfmacc.vf	v10, ft2, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
80000e8c: 07 91 0a 00  	flh	ft2, 0(s5)
;         a__ += N;
80000e90: 93 8a 0f 60  	addi	s5, t6, 1536
;         asm volatile("vfmacc.vf v12, %0, v20" ::"f"(t6));
80000e94: 57 d6 40 b3  	vfmacc.vf	v12, ft1, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
80000e98: 87 90 0a 00  	flh	ft1, 0(s5)
;         a__ += N;
80000e9c: 93 8f 0f 70  	addi	t6, t6, 1792
;         asm volatile("vfmacc.vf v14, %0, v20" ::"f"(t7));
80000ea0: 57 57 40 b3  	vfmacc.vf	v14, ft0, v20
;         asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));
80000ea4: 07 90 0f 00  	flh	ft0, 0(t6)
;       while (n < N) {
80000ea8: 93 8e 0e 20  	addi	t4, t4, 512
80000eac: e3 7a ef e6  	bgeu	t5, a4, 0x80000d20 <.LBB30_33+0x70>
;         a__ = a_ + ++n;
80000eb0: 93 8a 0e f0  	addi	s5, t4, -256
80000eb4: 93 1f 1f 00  	slli	t6, t5, 1
80000eb8: 93 ef 2f 00  	ori	t6, t6, 2
80000ebc: b3 0f fe 01  	add	t6, t3, t6
;         asm volatile("vle16.v v20, (%0);" ::"r"(b__));
80000ec0: 07 da 0a 02  	vle16.v	v20, (s5)
;         if (n == 1) {
80000ec4: e3 0e 0f f0  	beqz	t5, 0x80000de0 <.LBB30_33+0x130>
;           asm volatile("vfmacc.vf v0, %0, v18" ::"f"(t0));
80000ec8: 57 d0 23 b3  	vfmacc.vf	v0, ft7, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
80000ecc: 87 93 0f 00  	flh	ft7, 0(t6)
;           a__ += N;
80000ed0: 93 8a 0f 10  	addi	s5, t6, 256
;           asm volatile("vfmacc.vf v2, %0, v18" ::"f"(t1));
80000ed4: 57 51 23 b3  	vfmacc.vf	v2, ft6, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
80000ed8: 07 93 0a 00  	flh	ft6, 0(s5)
;           a__ += N;
80000edc: 93 8a 0f 20  	addi	s5, t6, 512
;           asm volatile("vfmacc.vf v4, %0, v18" ::"f"(t2));
80000ee0: 57 d2 22 b3  	vfmacc.vf	v4, ft5, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
80000ee4: 87 92 0a 00  	flh	ft5, 0(s5)
;           a__ += N;
80000ee8: 93 8a 0f 30  	addi	s5, t6, 768
;           asm volatile("vfmacc.vf v6, %0, v18" ::"f"(t3));
80000eec: 57 53 22 b3  	vfmacc.vf	v6, ft4, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
80000ef0: 07 92 0a 00  	flh	ft4, 0(s5)
;           a__ += N;
80000ef4: 93 8a 0f 40  	addi	s5, t6, 1024
;           asm volatile("vfmacc.vf v8, %0, v18" ::"f"(t4));
80000ef8: 57 d4 21 b3  	vfmacc.vf	v8, ft3, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
80000efc: 87 91 0a 00  	flh	ft3, 0(s5)
;           a__ += N;
80000f00: 93 8a 0f 50  	addi	s5, t6, 1280
;           asm volatile("vfmacc.vf v10, %0, v18" ::"f"(t5));
80000f04: 57 55 21 b3  	vfmacc.vf	v10, ft2, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
80000f08: 07 91 0a 00  	flh	ft2, 0(s5)
;           a__ += N;
80000f0c: 93 8a 0f 60  	addi	s5, t6, 1536
;           asm volatile("vfmacc.vf v12, %0, v18" ::"f"(t6));
80000f10: 57 d6 20 b3  	vfmacc.vf	v12, ft1, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
80000f14: 87 90 0a 00  	flh	ft1, 0(s5)
;           a__ += N;
80000f18: 93 8f 0f 70  	addi	t6, t6, 1792
;           asm volatile("vfmacc.vf v14, %0, v18" ::"f"(t7));
80000f1c: 57 57 20 b3  	vfmacc.vf	v14, ft0, v18
;           asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));
80000f20: 07 90 0f 00  	flh	ft0, 0(t6)
;         a__ = a_ + ++n;
80000f24: 13 0f 2f 00  	addi	t5, t5, 2
80000f28: e3 1c ef f0  	bne	t5, a4, 0x80000e40 <.LBB30_33+0x190>
80000f2c: 6f f0 5f df  	j	0x80000d20 <.LBB30_33+0x70>
80000f30: 13 05 00 00  	li	a0, 0
80000f34: 93 05 00 08  	li	a1, 128
80000f38: 13 00 00 00  	nop
80000f3c: 13 00 00 00  	nop
;                  : [vl] "r"(p_end - p));
80000f40: 33 86 a5 40  	sub	a2, a1, a0
;     asm volatile("vsetvli %[gvl], %[vl], e16, m2, ta, ma"
80000f44: 57 76 96 0c  	vsetvli	a2, a2, e16, m2, ta, ma
;     p += gvl;
80000f48: 33 05 a6 00  	add	a0, a2, a0
80000f4c: e3 6a b5 fe  	bltu	a0, a1, 0x80000f40 <.LBB30_33+0x290>
;     snrt_cluster_hw_barrier();
80000f50: 97 00 00 00  	auipc	ra, 0
80000f54: e7 80 40 0a  	jalr	164(ra)
;     if (cid == 0)
80000f58: 63 08 04 04  	beqz	s0, 0x80000fa8 <.LBB30_34+0x20>
;     snrt_cluster_hw_barrier();
80000f5c: 97 00 00 00  	auipc	ra, 0
80000f60: e7 80 80 09  	jalr	152(ra)
80000f64: 6f 00 80 06  	j	0x80000fcc <.LBB30_34+0x44>
80000f68: 13 05 00 00  	li	a0, 0
80000f6c: 93 05 00 08  	li	a1, 128
;                  : [vl] "r"(p_end - p));
80000f70: 33 86 a5 40  	sub	a2, a1, a0
;     asm volatile("vsetvli %[gvl], %[vl], e16, m2, ta, ma"
80000f74: 57 76 96 0c  	vsetvli	a2, a2, e16, m2, ta, ma
;     p += gvl;
80000f78: 33 05 a6 00  	add	a0, a2, a0
80000f7c: e3 6a b5 fe  	bltu	a0, a1, 0x80000f70 <.LBB30_33+0x2c0>
;     snrt_cluster_hw_barrier();
80000f80: 97 00 00 00  	auipc	ra, 0
80000f84: e7 80 40 07  	jalr	116(ra)

80000f88 <.LBB30_34>:
;       (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
80000f88: 17 25 00 00  	auipc	a0, 2
80000f8c: 03 25 45 64  	lw	a0, 1604(a0)
80000f90: 33 05 45 00  	add	a0, a0, tp
80000f94: 03 25 05 00  	lw	a0, 0(a0)
80000f98: 03 25 05 00  	lw	a0, 0(a0)
80000f9c: 03 25 85 03  	lw	a0, 56(a0)
;   *bench = 0;
80000fa0: 23 28 05 04  	sw	zero, 80(a0)
;     if (cid == 0)
80000fa4: e3 1c 04 fa  	bnez	s0, 0x80000f5c <.LBB30_33+0x2ac>
; size_t benchmark_get_cycle() { return read_csr(mcycle); }
80000fa8: 73 25 00 b0  	csrr	a0, mcycle
;         timer = benchmark_get_cycle() - timer;
80000fac: 33 04 45 41  	sub	s0, a0, s4
;     snrt_cluster_hw_barrier();
80000fb0: 97 00 00 00  	auipc	ra, 0
80000fb4: e7 80 40 04  	jalr	68(ra)
;         printf("The execution took %u cycles.\n", timer);
80000fb8: 37 35 00 80  	lui	a0, 524291
80000fbc: 13 05 05 34  	addi	a0, a0, 832
80000fc0: 93 05 04 00  	mv	a1, s0
80000fc4: 97 00 00 00  	auipc	ra, 0
80000fc8: e7 80 40 06  	jalr	100(ra)
;     return errors;
80000fcc: 03 25 01 00  	lw	a0, 0(sp)
80000fd0: 83 20 c1 01  	lw	ra, 28(sp)
80000fd4: 03 24 81 01  	lw	s0, 24(sp)
80000fd8: 83 24 41 01  	lw	s1, 20(sp)
80000fdc: 03 29 01 01  	lw	s2, 16(sp)
80000fe0: 83 29 c1 00  	lw	s3, 12(sp)
80000fe4: 03 2a 81 00  	lw	s4, 8(sp)
80000fe8: 83 2a 41 00  	lw	s5, 4(sp)
80000fec: 13 01 01 02  	addi	sp, sp, 32
80000ff0: 67 80 00 00  	ret

80000ff4 <snrt_cluster_hw_barrier>:
; void snrt_cluster_hw_barrier() { _snrt_cluster_barrier(); }
80000ff4: 17 f3 ff ff  	auipc	t1, 1048575
80000ff8: 67 00 83 3d  	jr	984(t1)

80000ffc <snrt_dma_start_1d>:
; snrt_dma_txid_t snrt_dma_start_1d(void *dst, const void *src, size_t size) {
80000ffc: 13 07 06 00  	mv	a4, a2
;     asm volatile(
80001000: 13 86 05 00  	mv	a2, a1
80001004: 93 06 00 00  	li	a3, 0
80001008: 2b 00 d6 00  	dmsrc	a2, a3
;     asm volatile(
8000100c: 93 05 00 00  	li	a1, 0
80001010: 2b 00 b5 02  	dmdst	a0, a1
;     asm volatile(
80001014: 2b 05 07 04  	dmcpyi	a0, a4, 0
;     return snrt_dma_start_1d_wideptr((size_t)dst, (size_t)src, size);
80001018: 67 80 00 00  	ret

8000101c <snrt_dma_wait_all>:
;     asm volatile(
8000101c: ab 02 20 08  	dmstati	t0, 2
80001020: e3 9e 02 fe  	bnez	t0, 0x8000101c <snrt_dma_wait_all>
; }
80001024: 67 80 00 00  	ret

80001028 <printf_>:
; {
80001028: 13 01 01 fd  	addi	sp, sp, -48
8000102c: 23 26 11 00  	sw	ra, 12(sp)
80001030: 93 02 05 00  	mv	t0, a0
80001034: 23 26 11 03  	sw	a7, 44(sp)
80001038: 23 24 01 03  	sw	a6, 40(sp)
8000103c: 23 22 f1 02  	sw	a5, 36(sp)
80001040: 23 20 e1 02  	sw	a4, 32(sp)
80001044: 23 2e d1 00  	sw	a3, 28(sp)
80001048: 23 2c c1 00  	sw	a2, 24(sp)
8000104c: 23 2a b1 00  	sw	a1, 20(sp)
80001050: 13 05 41 01  	addi	a0, sp, 20
;   va_start(va, format);
80001054: 23 24 a1 00  	sw	a0, 8(sp)
;   const int ret = _vsnprintf(_out_char, buffer, (size_t)-1, format, va);
80001058: 37 25 00 80  	lui	a0, 524290
8000105c: 13 05 05 dd  	addi	a0, a0, -560
80001060: 93 05 71 00  	addi	a1, sp, 7
80001064: 13 06 f0 ff  	li	a2, -1
80001068: 13 07 41 01  	addi	a4, sp, 20
8000106c: 93 86 02 00  	mv	a3, t0
80001070: 97 00 00 00  	auipc	ra, 0
80001074: e7 80 00 02  	jalr	32(ra)
;   return ret;
80001078: 83 20 c1 00  	lw	ra, 12(sp)
8000107c: 13 01 01 03  	addi	sp, sp, 48
80001080: 67 80 00 00  	ret
		...

80001090 <_vsnprintf>:
; {
80001090: 13 01 01 f8  	addi	sp, sp, -128
80001094: 23 2e 11 06  	sw	ra, 124(sp)
80001098: 23 2c 81 06  	sw	s0, 120(sp)
8000109c: 23 2a 91 06  	sw	s1, 116(sp)
800010a0: 23 28 21 07  	sw	s2, 112(sp)
800010a4: 23 26 31 07  	sw	s3, 108(sp)
800010a8: 23 24 41 07  	sw	s4, 104(sp)
800010ac: 23 22 51 07  	sw	s5, 100(sp)
800010b0: 23 20 61 07  	sw	s6, 96(sp)
800010b4: 23 2e 71 05  	sw	s7, 92(sp)
800010b8: 23 2c 81 05  	sw	s8, 88(sp)
800010bc: 23 2a 91 05  	sw	s9, 84(sp)
800010c0: 23 28 a1 05  	sw	s10, 80(sp)
800010c4: 23 26 b1 05  	sw	s11, 76(sp)
800010c8: 23 24 e1 02  	sw	a4, 40(sp)
800010cc: 93 8a 06 00  	mv	s5, a3
800010d0: 13 04 06 00  	mv	s0, a2
800010d4: 93 8c 05 00  	mv	s9, a1
;   if (!buffer) {
800010d8: 63 86 05 00  	beqz	a1, 0x800010e4 <_vsnprintf+0x54>
800010dc: 13 0c 05 00  	mv	s8, a0
800010e0: 6f 00 c0 00  	j	0x800010ec <_vsnprintf+0x5c>
800010e4: 37 25 00 80  	lui	a0, 524290
800010e8: 13 0c 05 de  	addi	s8, a0, -544
800010ec: 93 0b 00 00  	li	s7, 0
800010f0: 93 09 50 02  	li	s3, 37
800010f4: 93 0d 00 01  	li	s11, 16
800010f8: 13 0b e0 02  	li	s6, 46
800010fc: 37 35 00 80  	lui	a0, 524291
80001100: 13 05 45 3d  	addi	a0, a0, 980
80001104: 23 20 a1 02  	sw	a0, 32(sp)
80001108: 37 15 00 00  	lui	a0, 1
8000110c: 13 05 05 80  	addi	a0, a0, -2048
80001110: 23 2c a1 00  	sw	a0, 24(sp)
80001114: 37 35 00 80  	lui	a0, 524291
80001118: 13 05 c5 3a  	addi	a0, a0, 940
8000111c: 23 2e a1 00  	sw	a0, 28(sp)
80001120: 37 35 00 80  	lui	a0, 524291
80001124: 13 0d 85 36  	addi	s10, a0, 872
;   while (*format)
80001128: 13 89 2a 00  	addi	s2, s5, 2
8000112c: 13 8a 0b 00  	mv	s4, s7
80001130: 03 c5 0a 00  	lbu	a0, 0(s5)
80001134: e3 00 05 44  	beqz	a0, 0x80001d74 <.LBB1_120+0x49c>
80001138: 13 00 00 00  	nop
8000113c: 13 00 00 00  	nop
80001140: 63 08 35 03  	beq	a0, s3, 0x80001170 <_vsnprintf+0xe0>
;       out(*format, buffer, idx++, maxlen);
80001144: 93 04 1a 00  	addi	s1, s4, 1
80001148: 93 85 0c 00  	mv	a1, s9
8000114c: 13 06 0a 00  	mv	a2, s4
80001150: 93 06 04 00  	mv	a3, s0
80001154: e7 00 0c 00  	jalr	s8
;       format++;
80001158: 93 8a 1a 00  	addi	s5, s5, 1
;       continue;
8000115c: 13 09 19 00  	addi	s2, s2, 1
80001160: 13 8a 04 00  	mv	s4, s1
;   while (*format)
80001164: 03 c5 0a 00  	lbu	a0, 0(s5)
80001168: e3 1c 05 fc  	bnez	a0, 0x80001140 <_vsnprintf+0xb0>
8000116c: 6f 00 90 40  	j	0x80001d74 <.LBB1_120+0x49c>
80001170: 93 0b 00 00  	li	s7, 0
80001174: 6f 00 40 01  	j	0x80001188 <.LBB1_8+0x8>

80001178 <.LBB1_7>:
80001178: 93 05 00 01  	li	a1, 16
8000117c: 13 00 00 00  	nop

80001180 <.LBB1_8>:
80001180: b3 eb bb 00  	or	s7, s7, a1
;     } while (n);
80001184: 13 09 19 00  	addi	s2, s2, 1
;       switch (*format) {
80001188: 03 45 f9 ff  	lbu	a0, -1(s2)
8000118c: 13 06 05 fe  	addi	a2, a0, -32
80001190: 63 e8 cd 02  	bltu	s11, a2, 0x800011c0 <.LBB1_14>
80001194: 93 05 10 00  	li	a1, 1
80001198: 13 16 26 00  	slli	a2, a2, 2
8000119c: 33 06 a6 01  	add	a2, a2, s10
800011a0: 03 26 06 00  	lw	a2, 0(a2)
800011a4: 67 00 06 00  	jr	a2

800011a8 <.LBB1_11>:
800011a8: 93 05 80 00  	li	a1, 8
800011ac: 6f f0 5f fd  	j	0x80001180 <.LBB1_8>

800011b0 <.LBB1_12>:
800011b0: 93 05 40 00  	li	a1, 4
800011b4: 6f f0 df fc  	j	0x80001180 <.LBB1_8>

800011b8 <.LBB1_13>:
800011b8: 93 05 20 00  	li	a1, 2
800011bc: 6f f0 5f fc  	j	0x80001180 <.LBB1_8>

800011c0 <.LBB1_14>:
;   return (ch >= '0') && (ch <= '9');
800011c0: 93 05 05 fd  	addi	a1, a0, -48
800011c4: 13 f6 f5 0f  	andi	a2, a1, 255
;     if (_is_digit(*format)) {
800011c8: 93 05 f9 ff  	addi	a1, s2, -1
800011cc: 93 06 90 00  	li	a3, 9
800011d0: 63 e4 c6 08  	bltu	a3, a2, 0x80001258 <.LBB1_14+0x98>
800011d4: 93 0a 00 00  	li	s5, 0
800011d8: 93 06 a0 00  	li	a3, 10
800011dc: 13 00 00 00  	nop
;     i = i * 10U + (unsigned int)(*((*str)++) - '0');
800011e0: 33 86 da 02  	mul	a2, s5, a3
800011e4: 13 75 f5 0f  	andi	a0, a0, 255
800011e8: 33 05 a6 00  	add	a0, a2, a0
800011ec: 93 0a 05 fd  	addi	s5, a0, -48
;   while (_is_digit(**str)) {
800011f0: 03 c5 15 00  	lbu	a0, 1(a1)
;     i = i * 10U + (unsigned int)(*((*str)++) - '0');
800011f4: 93 85 15 00  	addi	a1, a1, 1
;   return (ch >= '0') && (ch <= '9');
800011f8: 13 06 05 fd  	addi	a2, a0, -48
800011fc: 13 76 f6 0f  	andi	a2, a2, 255
;   while (_is_digit(**str)) {
80001200: e3 60 d6 fe  	bltu	a2, a3, 0x800011e0 <.LBB1_14+0x20>
80001204: 13 89 05 00  	mv	s2, a1
;     if (*format == '.') {
80001208: 63 12 65 0d  	bne	a0, s6, 0x800012cc <.LBB1_14+0x10c>
;       flags |= FLAGS_PRECISION;
8000120c: 93 eb 0b 40  	ori	s7, s7, 1024
;       format++;
80001210: 93 05 19 00  	addi	a1, s2, 1
;       if (_is_digit(*format)) {
80001214: 03 45 19 00  	lbu	a0, 1(s2)
;   return (ch >= '0') && (ch <= '9');
80001218: 13 06 05 fd  	addi	a2, a0, -48
8000121c: 13 76 f6 0f  	andi	a2, a2, 255
;       if (_is_digit(*format)) {
80001220: 93 06 90 00  	li	a3, 9
80001224: 63 e8 c6 06  	bltu	a3, a2, 0x80001294 <.LBB1_14+0xd4>
80001228: 13 0b 00 00  	li	s6, 0
8000122c: 93 06 a0 00  	li	a3, 10
;     i = i * 10U + (unsigned int)(*((*str)++) - '0');
80001230: 33 06 db 02  	mul	a2, s6, a3
80001234: 13 75 f5 0f  	andi	a0, a0, 255
80001238: 33 05 a6 00  	add	a0, a2, a0
8000123c: 13 0b 05 fd  	addi	s6, a0, -48
;   while (_is_digit(**str)) {
80001240: 03 c5 15 00  	lbu	a0, 1(a1)
;     i = i * 10U + (unsigned int)(*((*str)++) - '0');
80001244: 93 85 15 00  	addi	a1, a1, 1
;   return (ch >= '0') && (ch <= '9');
80001248: 13 06 05 fd  	addi	a2, a0, -48
8000124c: 13 76 f6 0f  	andi	a2, a2, 255
;   while (_is_digit(**str)) {
80001250: e3 60 d6 fe  	bltu	a2, a3, 0x80001230 <.LBB1_14+0x70>
80001254: 6f 00 40 08  	j	0x800012d8 <.LBB1_14+0x118>
;     else if (*format == '*') {
80001258: 13 06 a0 02  	li	a2, 42
8000125c: 63 12 c5 06  	bne	a0, a2, 0x800012c0 <.LBB1_14+0x100>
;       const int w = va_arg(va, int);
80001260: 03 25 81 02  	lw	a0, 40(sp)
80001264: 03 25 05 00  	lw	a0, 0(a0)
;       if (w < 0) {
80001268: 63 54 05 00  	bgez	a0, 0x80001270 <.LBB1_14+0xb0>
8000126c: 93 eb 2b 00  	ori	s7, s7, 2
80001270: 83 25 81 02  	lw	a1, 40(sp)
80001274: 93 85 45 00  	addi	a1, a1, 4
80001278: 23 24 b1 02  	sw	a1, 40(sp)
8000127c: 93 55 f5 41  	srai	a1, a0, 31
80001280: 33 05 b5 00  	add	a0, a0, a1
80001284: b3 4a b5 00  	xor	s5, a0, a1
;     if (*format == '.') {
80001288: 03 45 09 00  	lbu	a0, 0(s2)
8000128c: e3 00 65 f9  	beq	a0, s6, 0x8000120c <.LBB1_14+0x4c>
80001290: 6f 00 c0 03  	j	0x800012cc <.LBB1_14+0x10c>
;       else if (*format == '*') {
80001294: 13 06 a0 02  	li	a2, 42
80001298: 63 1e c5 02  	bne	a0, a2, 0x800012d4 <.LBB1_14+0x114>
8000129c: 03 25 81 02  	lw	a0, 40(sp)
;         const int prec = (int)va_arg(va, int);
800012a0: 03 2b 05 00  	lw	s6, 0(a0)
;         precision = prec > 0 ? (unsigned int)prec : 0U;
800012a4: 63 44 60 01  	bgtz	s6, 0x800012ac <.LBB1_14+0xec>
800012a8: 13 0b 00 00  	li	s6, 0
800012ac: 13 05 45 00  	addi	a0, a0, 4
800012b0: 23 24 a1 02  	sw	a0, 40(sp)
;     switch (*format) {
800012b4: 03 45 29 00  	lbu	a0, 2(s2)
;         format++;
800012b8: 13 09 29 00  	addi	s2, s2, 2
800012bc: 6f 00 00 02  	j	0x800012dc <.LBB1_14+0x11c>
800012c0: 93 0a 00 00  	li	s5, 0
800012c4: 13 89 05 00  	mv	s2, a1
;     if (*format == '.') {
800012c8: e3 02 65 f5  	beq	a0, s6, 0x8000120c <.LBB1_14+0x4c>
800012cc: 13 0b 00 00  	li	s6, 0
800012d0: 6f 00 c0 00  	j	0x800012dc <.LBB1_14+0x11c>
800012d4: 13 0b 00 00  	li	s6, 0
800012d8: 13 89 05 00  	mv	s2, a1
800012dc: 23 22 81 03  	sw	s8, 36(sp)
;     switch (*format) {
800012e0: 93 05 85 f9  	addi	a1, a0, -104
800012e4: 13 d6 15 00  	srli	a2, a1, 1
800012e8: 93 95 f5 01  	slli	a1, a1, 31
800012ec: b3 e6 c5 00  	or	a3, a1, a2
800012f0: 93 05 90 00  	li	a1, 9
800012f4: 63 e0 d5 06  	bltu	a1, a3, 0x80001354 <.LBB1_41>
800012f8: 93 05 10 00  	li	a1, 1
800012fc: 13 06 00 10  	li	a2, 256
80001300: 93 96 26 00  	slli	a3, a3, 2
80001304: 03 27 c1 01  	lw	a4, 28(sp)
80001308: b3 86 e6 00  	add	a3, a3, a4
8000130c: 83 a6 06 00  	lw	a3, 0(a3)
80001310: 67 80 06 00  	jr	a3

80001314 <.LBB1_35>:
;         if (*format == 'h') {
80001314: 03 45 19 00  	lbu	a0, 1(s2)
80001318: 93 05 80 06  	li	a1, 104
8000131c: 63 1c b5 12  	bne	a0, a1, 0x80001454 <.LBB1_52+0x54>
80001320: 93 05 20 00  	li	a1, 2
80001324: 13 06 00 0c  	li	a2, 192
80001328: 6f 00 00 02  	j	0x80001348 <.LBB1_40>

8000132c <.LBB1_37>:
8000132c: 13 06 00 20  	li	a2, 512
80001330: 6f 00 80 01  	j	0x80001348 <.LBB1_40>

80001334 <.LBB1_38>:
;         if (*format == 'l') {
80001334: 03 45 19 00  	lbu	a0, 1(s2)
80001338: 93 05 c0 06  	li	a1, 108
8000133c: 63 12 b5 12  	bne	a0, a1, 0x80001460 <.LBB1_52+0x60>
80001340: 93 05 20 00  	li	a1, 2
80001344: 13 06 00 30  	li	a2, 768

80001348 <.LBB1_40>:
80001348: b3 eb cb 00  	or	s7, s7, a2
8000134c: 33 09 b9 00  	add	s2, s2, a1
;     switch (*format) {
80001350: 03 45 09 00  	lbu	a0, 0(s2)

80001354 <.LBB1_41>:
80001354: 13 06 b5 fd  	addi	a2, a0, -37
80001358: 93 05 30 05  	li	a1, 83
8000135c: 63 ee c5 10  	bltu	a1, a2, 0x80001478 <.LBB1_58>
80001360: 93 05 80 00  	li	a1, 8
80001364: 13 0c 00 01  	li	s8, 16
80001368: 13 16 26 00  	slli	a2, a2, 2
8000136c: 83 26 01 02  	lw	a3, 32(sp)
80001370: 33 06 d6 00  	add	a2, a2, a3
80001374: 03 26 06 00  	lw	a2, 0(a2)
80001378: 67 00 06 00  	jr	a2

8000137c <.LBB1_43>:
;           flags &= ~FLAGS_HASH;   // no hash for dec format
8000137c: 93 fb fb fe  	andi	s7, s7, -17
80001380: 13 0c a0 00  	li	s8, 10

80001384 <.LBB1_44>:
;         if (*format == 'X') {
80001384: 93 05 80 05  	li	a1, 88
80001388: 63 14 b5 00  	bne	a0, a1, 0x80001390 <.LBB1_44+0xc>
8000138c: 93 eb 0b 02  	ori	s7, s7, 32
;         if ((*format != 'i') && (*format != 'd')) {
80001390: 93 05 40 06  	li	a1, 100
80001394: 63 06 b5 10  	beq	a0, a1, 0x800014a0 <.LBB1_61+0x8>
80001398: 93 05 0c 00  	mv	a1, s8
;         if ((*format != 'i') && (*format != 'd')) {
8000139c: 13 06 90 06  	li	a2, 105
800013a0: 63 00 c5 10  	beq	a0, a2, 0x800014a0 <.LBB1_61+0x8>
800013a4: 6f 00 40 0f  	j	0x80001498 <.LBB1_61>

800013a8 <.LBB1_48>:
;         if (*format == 'F') flags |= FLAGS_UPPERCASE;
800013a8: 93 05 60 04  	li	a1, 70
800013ac: 63 14 b5 00  	bne	a0, a1, 0x800013b4 <.LBB1_48+0xc>
800013b0: 93 eb 0b 02  	ori	s7, s7, 32
800013b4: 03 2c 41 02  	lw	s8, 36(sp)
800013b8: 03 25 81 02  	lw	a0, 40(sp)
;         idx = _ftoa(out, buffer, idx, maxlen, va_arg(va, double), precision, width, flags);
800013bc: 13 05 75 00  	addi	a0, a0, 7
800013c0: 13 75 85 ff  	andi	a0, a0, -8
800013c4: 93 05 85 00  	addi	a1, a0, 8
800013c8: 23 24 b1 02  	sw	a1, 40(sp)
800013cc: 07 35 05 00  	fld	fa0, 0(a0)
800013d0: 13 05 0c 00  	mv	a0, s8
800013d4: 93 85 0c 00  	mv	a1, s9
800013d8: 13 06 0a 00  	mv	a2, s4
800013dc: 93 06 04 00  	mv	a3, s0
800013e0: 13 07 0b 00  	mv	a4, s6
800013e4: 93 87 0a 00  	mv	a5, s5
800013e8: 13 88 0b 00  	mv	a6, s7
800013ec: 97 10 00 00  	auipc	ra, 1
800013f0: e7 80 40 a0  	jalr	-1532(ra)
800013f4: 6f 00 10 15  	j	0x80001d44 <.LBB1_120+0x46c>

800013f8 <.LBB1_51>:
;         if ((*format == 'g')||(*format == 'G')) flags |= FLAGS_ADAPT_EXP;
800013f8: 83 25 81 01  	lw	a1, 24(sp)
800013fc: b3 eb bb 00  	or	s7, s7, a1

80001400 <.LBB1_52>:
;         if ((*format == 'E')||(*format == 'G')) flags |= FLAGS_UPPERCASE;
80001400: 13 65 25 00  	ori	a0, a0, 2
80001404: 93 05 70 04  	li	a1, 71
80001408: 63 14 b5 00  	bne	a0, a1, 0x80001410 <.LBB1_52+0x10>
8000140c: 93 eb 0b 02  	ori	s7, s7, 32
80001410: 03 2c 41 02  	lw	s8, 36(sp)
;         idx = _etoa(out, buffer, idx, maxlen, va_arg(va, double), precision, width, flags);
80001414: 03 25 81 02  	lw	a0, 40(sp)
80001418: 13 05 75 00  	addi	a0, a0, 7
8000141c: 13 75 85 ff  	andi	a0, a0, -8
80001420: 93 05 85 00  	addi	a1, a0, 8
80001424: 23 24 b1 02  	sw	a1, 40(sp)
80001428: 07 35 05 00  	fld	fa0, 0(a0)
8000142c: 13 05 0c 00  	mv	a0, s8
80001430: 93 85 0c 00  	mv	a1, s9
80001434: 13 06 0a 00  	mv	a2, s4
80001438: 93 06 04 00  	mv	a3, s0
8000143c: 13 07 0b 00  	mv	a4, s6
80001440: 93 87 0a 00  	mv	a5, s5
80001444: 13 88 0b 00  	mv	a6, s7
80001448: 97 10 00 00  	auipc	ra, 1
8000144c: e7 80 80 0f  	jalr	248(ra)
80001450: 6f 00 50 0f  	j	0x80001d44 <.LBB1_120+0x46c>
80001454: 93 eb 0b 08  	ori	s7, s7, 128
80001458: 13 09 19 00  	addi	s2, s2, 1
8000145c: 6f f0 9f ef  	j	0x80001354 <.LBB1_41>
80001460: 93 eb 0b 10  	ori	s7, s7, 256
80001464: 13 09 19 00  	addi	s2, s2, 1
80001468: 6f f0 df ee  	j	0x80001354 <.LBB1_41>

8000146c <.LBB1_57>:
;         out('%', buffer, idx++, maxlen);
8000146c: 93 0b 1a 00  	addi	s7, s4, 1
80001470: 13 05 50 02  	li	a0, 37
80001474: 6f 00 80 00  	j	0x8000147c <.LBB1_58+0x4>

80001478 <.LBB1_58>:
;         out(*format, buffer, idx++, maxlen);
80001478: 93 0b 1a 00  	addi	s7, s4, 1
8000147c: 93 85 0c 00  	mv	a1, s9
80001480: 13 06 0a 00  	mv	a2, s4
80001484: 93 06 04 00  	mv	a3, s0
80001488: 03 2c 41 02  	lw	s8, 36(sp)
8000148c: e7 00 0c 00  	jalr	s8
80001490: 6f 00 90 0b  	j	0x80001d48 <.LBB1_120+0x470>

80001494 <.LBB1_60>:
80001494: 93 05 20 00  	li	a1, 2

80001498 <.LBB1_61>:
;           flags &= ~(FLAGS_PLUS | FLAGS_SPACE);
80001498: 93 fb 3b ff  	andi	s7, s7, -13
8000149c: 13 8c 05 00  	mv	s8, a1
;         if (flags & FLAGS_PRECISION) {
800014a0: 93 f5 0b 40  	andi	a1, s7, 1024
800014a4: 63 84 05 00  	beqz	a1, 0x800014ac <.LBB1_61+0x14>
800014a8: 93 fb eb ff  	andi	s7, s7, -2
;         if ((*format == 'i') || (*format == 'd')) {
800014ac: 93 05 90 06  	li	a1, 105
800014b0: 63 06 b5 00  	beq	a0, a1, 0x800014bc <.LBB1_61+0x24>
800014b4: 93 05 40 06  	li	a1, 100
800014b8: 63 12 b5 14  	bne	a0, a1, 0x800015fc <.LBB1_61+0x164>
;           if (flags & FLAGS_LONG_LONG) {
800014bc: 13 f5 0b 20  	andi	a0, s7, 512
800014c0: 63 18 05 02  	bnez	a0, 0x800014f0 <.LBB1_61+0x58>
;           else if (flags & FLAGS_LONG) {
800014c4: 13 f5 0b 10  	andi	a0, s7, 256
800014c8: 63 14 05 16  	bnez	a0, 0x80001630 <.LBB1_61+0x198>
;             const int value = (flags & FLAGS_CHAR) ? (char)va_arg(va, int) : (flags & FLAGS_SHORT) ? (short int)va_arg(va, int) : va_arg(va, int);
800014cc: 13 f5 0b 04  	andi	a0, s7, 64
800014d0: 63 14 05 58  	bnez	a0, 0x80001a58 <.LBB1_120+0x180>
800014d4: 93 f5 0b 08  	andi	a1, s7, 128
800014d8: 03 25 81 02  	lw	a0, 40(sp)
800014dc: 03 25 05 00  	lw	a0, 0(a0)
;             const int value = (flags & FLAGS_CHAR) ? (char)va_arg(va, int) : (flags & FLAGS_SHORT) ? (short int)va_arg(va, int) : va_arg(va, int);
800014e0: 63 80 05 58  	beqz	a1, 0x80001a60 <.LBB1_120+0x188>
800014e4: 13 15 05 01  	slli	a0, a0, 16
800014e8: 13 55 05 41  	srai	a0, a0, 16
800014ec: 6f 00 40 57  	j	0x80001a60 <.LBB1_120+0x188>
800014f0: 23 2a 91 01  	sw	s9, 20(sp)
;             const long long value = va_arg(va, long long);
800014f4: 03 25 81 02  	lw	a0, 40(sp)
800014f8: 13 05 75 00  	addi	a0, a0, 7
800014fc: 13 75 85 ff  	andi	a0, a0, -8
80001500: 83 29 05 00  	lw	s3, 0(a0)
80001504: 23 26 a1 00  	sw	a0, 12(sp)
;             const long long value = va_arg(va, long long);
80001508: 13 65 45 00  	ori	a0, a0, 4
8000150c: 03 25 05 00  	lw	a0, 0(a0)
;   if (!value) {
80001510: 23 28 a1 00  	sw	a0, 16(sp)
80001514: 33 e5 a9 00  	or	a0, s3, a0
80001518: 63 14 05 00  	bnez	a0, 0x80001520 <.LBB1_61+0x88>
8000151c: 93 fb fb fe  	andi	s7, s7, -17
80001520: 93 07 00 00  	li	a5, 0
80001524: 13 35 15 00  	seqz	a0, a0
;   if (!(flags & FLAGS_PRECISION) || value) {
80001528: 93 95 5b 01  	slli	a1, s7, 21
8000152c: 93 d5 f5 01  	srli	a1, a1, 31
80001530: 33 75 b5 00  	and	a0, a0, a1
80001534: 63 18 05 0a  	bnez	a0, 0x800015e4 <.LBB1_61+0x14c>
80001538: 83 2c 01 01  	lw	s9, 16(sp)
;             idx = _ntoa_long_long(out, buffer, idx, maxlen, (unsigned long long)(value > 0 ? value : 0 - value), value < 0, base, precision, width, flags);
8000153c: 63 da 0c 00  	bgez	s9, 0x80001550 <.LBB1_61+0xb8>
80001540: 33 35 30 01  	snez	a0, s3
80001544: b3 09 30 41  	neg	s3, s3
80001548: 33 85 ac 00  	add	a0, s9, a0
8000154c: b3 0c a0 40  	neg	s9, a0
80001550: 93 04 00 00  	li	s1, 0
80001554: 13 f5 0b 02  	andi	a0, s7, 32
80001558: 13 45 15 06  	xori	a0, a0, 97
8000155c: 13 05 65 0f  	addi	a0, a0, 246
80001560: 23 24 a1 02  	sw	a0, 40(sp)
80001564: 6f 00 c0 02  	j	0x80001590 <.LBB1_61+0xf8>
80001568: 13 00 00 00  	nop
8000156c: 13 00 00 00  	nop
80001570: 13 06 10 00  	li	a2, 1
80001574: 93 87 14 00  	addi	a5, s1, 1
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
80001578: 93 b6 f4 01  	sltiu	a3, s1, 31
8000157c: 33 76 d6 00  	and	a2, a2, a3
80001580: 93 84 07 00  	mv	s1, a5
80001584: 93 09 05 00  	mv	s3, a0
80001588: 93 8c 05 00  	mv	s9, a1
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
8000158c: 63 0c 06 04  	beqz	a2, 0x800015e4 <.LBB1_61+0x14c>
;       value /= base;
80001590: 93 06 00 00  	li	a3, 0
80001594: 13 85 09 00  	mv	a0, s3
80001598: 93 85 0c 00  	mv	a1, s9
8000159c: 13 06 0c 00  	mv	a2, s8
800015a0: 97 f0 ff ff  	auipc	ra, 1048575
800015a4: e7 80 40 e9  	jalr	-364(ra)
800015a8: 33 06 85 03  	mul	a2, a0, s8
800015ac: 33 86 c9 40  	sub	a2, s3, a2
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
800015b0: 13 77 e6 0f  	andi	a4, a2, 254
800015b4: 93 06 00 03  	li	a3, 48
800015b8: 93 07 a0 00  	li	a5, 10
800015bc: 63 64 f7 00  	bltu	a4, a5, 0x800015c4 <.LBB1_61+0x12c>
800015c0: 83 26 81 02  	lw	a3, 40(sp)
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
800015c4: 33 86 c6 00  	add	a2, a3, a2
800015c8: 93 06 c1 02  	addi	a3, sp, 44
800015cc: b3 86 96 00  	add	a3, a3, s1
800015d0: 23 80 c6 00  	sb	a2, 0(a3)
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
800015d4: e3 9e 0c f8  	bnez	s9, 0x80001570 <.LBB1_61+0xd8>
800015d8: 33 b6 89 01  	sltu	a2, s3, s8
800015dc: 13 46 16 00  	xori	a2, a2, 1
800015e0: 6f f0 5f f9  	j	0x80001574 <.LBB1_61+0xdc>
800015e4: 03 25 c1 00  	lw	a0, 12(sp)
800015e8: 13 05 85 00  	addi	a0, a0, 8
;   return _ntoa_format(out, buffer, idx, maxlen, buf, len, negative, (unsigned int)base, prec, width, flags);
800015ec: 23 24 a1 02  	sw	a0, 40(sp)
800015f0: 03 25 01 01  	lw	a0, 16(sp)
800015f4: 13 58 f5 01  	srli	a6, a0, 31
800015f8: 6f 00 c0 1b  	j	0x800017b4 <.LBB1_61+0x31c>
;           if (flags & FLAGS_LONG_LONG) {
800015fc: 13 f5 0b 20  	andi	a0, s7, 512
80001600: 63 1a 05 0c  	bnez	a0, 0x800016d4 <.LBB1_61+0x23c>
;           else if (flags & FLAGS_LONG) {
80001604: 13 f5 0b 10  	andi	a0, s7, 256
80001608: 63 10 05 50  	bnez	a0, 0x80001b08 <.LBB1_120+0x230>
;             const unsigned int value = (flags & FLAGS_CHAR) ? (unsigned char)va_arg(va, unsigned int) : (flags & FLAGS_SHORT) ? (unsigned short int)va_arg(va, unsigned int) : va_arg(va, unsigned int);
8000160c: 13 f5 0b 04  	andi	a0, s7, 64
80001610: 63 1c 05 64  	bnez	a0, 0x80001c68 <.LBB1_120+0x390>
80001614: 93 f5 0b 08  	andi	a1, s7, 128
80001618: 03 25 81 02  	lw	a0, 40(sp)
8000161c: 03 25 05 00  	lw	a0, 0(a0)
;             const unsigned int value = (flags & FLAGS_CHAR) ? (unsigned char)va_arg(va, unsigned int) : (flags & FLAGS_SHORT) ? (unsigned short int)va_arg(va, unsigned int) : va_arg(va, unsigned int);
80001620: 63 88 05 64  	beqz	a1, 0x80001c70 <.LBB1_120+0x398>
80001624: 13 15 05 01  	slli	a0, a0, 16
80001628: 13 55 05 01  	srli	a0, a0, 16
8000162c: 6f 00 40 64  	j	0x80001c70 <.LBB1_120+0x398>
;             const long value = va_arg(va, long);
80001630: 03 25 81 02  	lw	a0, 40(sp)
80001634: 03 25 05 00  	lw	a0, 0(a0)
;   if (!value) {
80001638: 63 14 05 00  	bnez	a0, 0x80001640 <.LBB1_61+0x1a8>
8000163c: 93 fb fb fe  	andi	s7, s7, -17
80001640: 93 07 00 00  	li	a5, 0
80001644: 93 35 15 00  	seqz	a1, a0
;   if (!(flags & FLAGS_PRECISION) || value) {
80001648: 13 96 5b 01  	slli	a2, s7, 21
8000164c: 13 56 f6 01  	srli	a2, a2, 31
80001650: b3 f5 c5 00  	and	a1, a1, a2
80001654: 63 90 05 4a  	bnez	a1, 0x80001af4 <.LBB1_120+0x21c>
80001658: 93 05 00 00  	li	a1, 0
;             idx = _ntoa_long(out, buffer, idx, maxlen, (unsigned long)(value > 0 ? value : 0 - value), value < 0, base, precision, width, flags);
8000165c: 13 56 f5 41  	srai	a2, a0, 31
80001660: b3 06 c5 00  	add	a3, a0, a2
80001664: b3 c6 c6 00  	xor	a3, a3, a2
80001668: 13 f6 0b 02  	andi	a2, s7, 32
8000166c: 13 46 16 06  	xori	a2, a2, 97
80001670: 13 06 66 0f  	addi	a2, a2, 246
80001674: 6f 00 c0 03  	j	0x800016b0 <.LBB1_61+0x218>
80001678: 13 00 00 00  	nop
8000167c: 13 00 00 00  	nop
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
80001680: 33 08 f8 00  	add	a6, a6, a5
80001684: 93 87 15 00  	addi	a5, a1, 1
80001688: 93 08 c1 02  	addi	a7, sp, 44
8000168c: b3 88 b8 00  	add	a7, a7, a1
80001690: 23 80 08 01  	sb	a6, 0(a7)
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
80001694: b3 b6 86 01  	sltu	a3, a3, s8
80001698: 93 c6 16 00  	xori	a3, a3, 1
8000169c: 93 b5 f5 01  	sltiu	a1, a1, 31
800016a0: 33 f8 b6 00  	and	a6, a3, a1
800016a4: 93 85 07 00  	mv	a1, a5
800016a8: 93 06 07 00  	mv	a3, a4
800016ac: 63 04 08 44  	beqz	a6, 0x80001af4 <.LBB1_120+0x21c>
;       value /= base;
800016b0: 33 d7 86 03  	divu	a4, a3, s8
800016b4: b3 07 87 03  	mul	a5, a4, s8
800016b8: b3 87 f6 40  	sub	a5, a3, a5
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
800016bc: 93 f8 e7 0f  	andi	a7, a5, 254
800016c0: 13 08 00 03  	li	a6, 48
800016c4: 93 02 a0 00  	li	t0, 10
800016c8: e3 ec 58 fa  	bltu	a7, t0, 0x80001680 <.LBB1_61+0x1e8>
800016cc: 13 08 06 00  	mv	a6, a2
800016d0: 6f f0 1f fb  	j	0x80001680 <.LBB1_61+0x1e8>
800016d4: 23 2a 91 01  	sw	s9, 20(sp)
;             idx = _ntoa_long_long(out, buffer, idx, maxlen, va_arg(va, unsigned long long), false, base, precision, width, flags);
800016d8: 03 25 81 02  	lw	a0, 40(sp)
800016dc: 13 05 75 00  	addi	a0, a0, 7
800016e0: 93 75 85 ff  	andi	a1, a0, -8
800016e4: 83 a9 05 00  	lw	s3, 0(a1)
800016e8: 13 e5 45 00  	ori	a0, a1, 4
800016ec: 83 2c 05 00  	lw	s9, 0(a0)
;   if (!value) {
800016f0: 33 e5 99 01  	or	a0, s3, s9
800016f4: 23 28 b1 00  	sw	a1, 16(sp)
800016f8: 63 14 05 00  	bnez	a0, 0x80001700 <.LBB1_61+0x268>
800016fc: 93 fb fb fe  	andi	s7, s7, -17
80001700: 93 07 00 00  	li	a5, 0
80001704: 13 35 15 00  	seqz	a0, a0
;   if (!(flags & FLAGS_PRECISION) || value) {
80001708: 93 95 5b 01  	slli	a1, s7, 21
8000170c: 93 d5 f5 01  	srli	a1, a1, 31
80001710: 33 75 b5 00  	and	a0, a0, a1
80001714: 63 18 05 08  	bnez	a0, 0x800017a4 <.LBB1_61+0x30c>
80001718: 93 04 00 00  	li	s1, 0
8000171c: 13 f5 0b 02  	andi	a0, s7, 32
80001720: 13 45 15 06  	xori	a0, a0, 97
80001724: 13 05 65 0f  	addi	a0, a0, 246
80001728: 23 24 a1 02  	sw	a0, 40(sp)
8000172c: 6f 00 40 02  	j	0x80001750 <.LBB1_61+0x2b8>
80001730: 13 06 10 00  	li	a2, 1
80001734: 93 87 14 00  	addi	a5, s1, 1
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
80001738: 93 b6 f4 01  	sltiu	a3, s1, 31
8000173c: 33 76 d6 00  	and	a2, a2, a3
80001740: 93 84 07 00  	mv	s1, a5
80001744: 93 09 05 00  	mv	s3, a0
80001748: 93 8c 05 00  	mv	s9, a1
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
8000174c: 63 0c 06 04  	beqz	a2, 0x800017a4 <.LBB1_61+0x30c>
;       value /= base;
80001750: 93 06 00 00  	li	a3, 0
80001754: 13 85 09 00  	mv	a0, s3
80001758: 93 85 0c 00  	mv	a1, s9
8000175c: 13 06 0c 00  	mv	a2, s8
80001760: 97 f0 ff ff  	auipc	ra, 1048575
80001764: e7 80 40 cd  	jalr	-812(ra)
80001768: 33 06 85 03  	mul	a2, a0, s8
8000176c: 33 86 c9 40  	sub	a2, s3, a2
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
80001770: 13 77 e6 0f  	andi	a4, a2, 254
80001774: 93 06 00 03  	li	a3, 48
80001778: 93 07 a0 00  	li	a5, 10
8000177c: 63 64 f7 00  	bltu	a4, a5, 0x80001784 <.LBB1_61+0x2ec>
80001780: 83 26 81 02  	lw	a3, 40(sp)
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
80001784: 33 86 c6 00  	add	a2, a3, a2
80001788: 93 06 c1 02  	addi	a3, sp, 44
8000178c: b3 86 96 00  	add	a3, a3, s1
80001790: 23 80 c6 00  	sb	a2, 0(a3)
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
80001794: e3 9e 0c f8  	bnez	s9, 0x80001730 <.LBB1_61+0x298>
80001798: 33 b6 89 01  	sltu	a2, s3, s8
8000179c: 13 46 16 00  	xori	a2, a2, 1
800017a0: 6f f0 5f f9  	j	0x80001734 <.LBB1_61+0x29c>
800017a4: 03 25 01 01  	lw	a0, 16(sp)
800017a8: 13 05 85 00  	addi	a0, a0, 8
;   return _ntoa_format(out, buffer, idx, maxlen, buf, len, negative, (unsigned int)base, prec, width, flags);
800017ac: 23 24 a1 02  	sw	a0, 40(sp)
800017b0: 13 08 00 00  	li	a6, 0
800017b4: 13 07 c1 02  	addi	a4, sp, 44
800017b8: 03 25 41 02  	lw	a0, 36(sp)
800017bc: 83 2c 41 01  	lw	s9, 20(sp)
800017c0: 93 85 0c 00  	mv	a1, s9
800017c4: 13 06 0a 00  	mv	a2, s4
800017c8: 93 06 04 00  	mv	a3, s0
800017cc: 93 08 0c 00  	mv	a7, s8
800017d0: 13 0c 05 00  	mv	s8, a0
800017d4: 93 03 0b 00  	mv	t2, s6
800017d8: 13 8e 0a 00  	mv	t3, s5
800017dc: 93 8e 0b 00  	mv	t4, s7
800017e0: 97 10 00 00  	auipc	ra, 1
800017e4: e7 80 00 24  	jalr	576(ra)
800017e8: 93 0b 05 00  	mv	s7, a0
800017ec: 93 09 50 02  	li	s3, 37
800017f0: 6f 00 80 55  	j	0x80001d48 <.LBB1_120+0x470>

800017f4 <.LBB1_108>:
;         if (!(flags & FLAGS_LEFT)) {
800017f4: 93 f5 2b 00  	andi	a1, s7, 2
800017f8: 93 04 10 00  	li	s1, 1
800017fc: 63 9a 05 1e  	bnez	a1, 0x800019f0 <.LBB1_120+0x118>
80001800: 93 04 20 00  	li	s1, 2
;           while (l++ < width) {
80001804: 63 e6 9a 1e  	bltu	s5, s1, 0x800019f0 <.LBB1_120+0x118>
80001808: 23 2a b1 00  	sw	a1, 20(sp)
8000180c: 93 0b 00 00  	li	s7, 0
;           while (l++ < width) {
80001810: 93 84 1a 00  	addi	s1, s5, 1
80001814: 13 8c fa ff  	addi	s8, s5, -1
80001818: 03 2b 41 02  	lw	s6, 36(sp)
8000181c: 13 00 00 00  	nop
;             out(' ', buffer, idx++, maxlen);
80001820: 33 06 7a 01  	add	a2, s4, s7
80001824: 13 05 00 02  	li	a0, 32
80001828: 93 85 0c 00  	mv	a1, s9
8000182c: 93 06 04 00  	mv	a3, s0
80001830: e7 00 0b 00  	jalr	s6
;           while (l++ < width) {
80001834: 93 8b 1b 00  	addi	s7, s7, 1
80001838: e3 14 7c ff  	bne	s8, s7, 0x80001820 <.LBB1_108+0x2c>
;         out((char)va_arg(va, int), buffer, idx++, maxlen);
8000183c: 33 0a 7a 01  	add	s4, s4, s7
80001840: 03 25 81 02  	lw	a0, 40(sp)
80001844: 83 25 41 01  	lw	a1, 20(sp)
80001848: 6f 00 c0 1a  	j	0x800019f4 <.LBB1_120+0x11c>

8000184c <.LBB1_113>:
;         flags |= FLAGS_ZEROPAD | FLAGS_UPPERCASE;
8000184c: 93 ee 1b 02  	ori	t4, s7, 33
;           idx = _ntoa_long(out, buffer, idx, maxlen, (unsigned long)((uintptr_t)va_arg(va, void*)), false, 16U, precision, width, flags);
80001850: 03 25 81 02  	lw	a0, 40(sp)
80001854: 03 25 05 00  	lw	a0, 0(a0)
;   if (!value) {
80001858: 63 14 05 00  	bnez	a0, 0x80001860 <.LBB1_113+0x14>
8000185c: 93 fe fe fe  	andi	t4, t4, -17
80001860: 13 08 f0 00  	li	a6, 15
80001864: 93 07 00 00  	li	a5, 0
80001868: 93 35 15 00  	seqz	a1, a0
;   if (!(flags & FLAGS_PRECISION) || value) {
8000186c: 13 96 5e 01  	slli	a2, t4, 21
80001870: 13 56 f6 01  	srli	a2, a2, 31
80001874: b3 f5 c5 00  	and	a1, a1, a2
80001878: 03 2c 41 02  	lw	s8, 36(sp)
8000187c: 63 90 05 14  	bnez	a1, 0x800019bc <.LBB1_120+0xe4>
80001880: 93 05 00 00  	li	a1, 0
80001884: 6f 00 c0 03  	j	0x800018c0 <.LBB1_113+0x74>
80001888: 13 00 00 00  	nop
8000188c: 13 00 00 00  	nop
80001890: 93 76 f5 00  	andi	a3, a0, 15
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
80001894: 33 06 d6 00  	add	a2, a2, a3
80001898: 93 87 15 00  	addi	a5, a1, 1
8000189c: 93 06 c1 02  	addi	a3, sp, 44
800018a0: b3 86 b6 00  	add	a3, a3, a1
800018a4: 23 80 c6 00  	sb	a2, 0(a3)
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
800018a8: 33 36 a8 00  	sltu	a2, a6, a0
;       value /= base;
800018ac: 13 55 45 00  	srli	a0, a0, 4
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
800018b0: 93 b5 f5 01  	sltiu	a1, a1, 31
800018b4: 33 76 b6 00  	and	a2, a2, a1
800018b8: 93 85 07 00  	mv	a1, a5
800018bc: 63 00 06 10  	beqz	a2, 0x800019bc <.LBB1_120+0xe4>
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
800018c0: 93 76 e5 00  	andi	a3, a0, 14
800018c4: 13 06 00 03  	li	a2, 48
800018c8: 13 07 a0 00  	li	a4, 10
800018cc: e3 e2 e6 fc  	bltu	a3, a4, 0x80001890 <.LBB1_113+0x44>
800018d0: 13 06 70 03  	li	a2, 55
800018d4: 6f f0 df fb  	j	0x80001890 <.LBB1_113+0x44>

800018d8 <.LBB1_120>:
800018d8: 23 2a 91 01  	sw	s9, 20(sp)
;         const char* p = va_arg(va, char*);
800018dc: 03 25 81 02  	lw	a0, 40(sp)
800018e0: 03 28 05 00  	lw	a6, 0(a0)
;   for (s = str; *s && maxsize--; ++s);
800018e4: 03 45 08 00  	lbu	a0, 0(a6)
800018e8: 13 06 08 00  	mv	a2, a6
;   for (s = str; *s && maxsize--; ++s);
800018ec: 63 02 05 04  	beqz	a0, 0x80001930 <.LBB1_120+0x58>
800018f0: 93 05 f0 ff  	li	a1, -1
;         unsigned int l = _strnlen_s(p, precision ? precision : (size_t)-1);
800018f4: 63 04 0b 00  	beqz	s6, 0x800018fc <.LBB1_120+0x24>
800018f8: 93 05 0b 00  	mv	a1, s6
;   for (s = str; *s && maxsize--; ++s);
800018fc: 93 85 f5 ff  	addi	a1, a1, -1
80001900: 93 06 08 00  	mv	a3, a6
80001904: 13 00 00 00  	nop
80001908: 13 00 00 00  	nop
8000190c: 13 00 00 00  	nop
;   for (s = str; *s && maxsize--; ++s);
80001910: 13 86 16 00  	addi	a2, a3, 1
80001914: 03 c7 16 00  	lbu	a4, 1(a3)
80001918: 93 b7 15 00  	seqz	a5, a1
8000191c: 93 85 f5 ff  	addi	a1, a1, -1
80001920: 93 06 06 00  	mv	a3, a2
;   for (s = str; *s && maxsize--; ++s);
80001924: 13 37 17 00  	seqz	a4, a4
80001928: 33 67 f7 00  	or	a4, a4, a5
8000192c: e3 02 07 fe  	beqz	a4, 0x80001910 <.LBB1_120+0x38>
;   return (unsigned int)(s - str);
80001930: b3 05 06 41  	sub	a1, a2, a6
;         if (flags & FLAGS_PRECISION) {
80001934: 13 fc 0b 40  	andi	s8, s7, 1024
80001938: 13 36 1c 00  	seqz	a2, s8
8000193c: b3 b6 65 01  	sltu	a3, a1, s6
80001940: 33 66 d6 00  	or	a2, a2, a3
80001944: 63 14 06 00  	bnez	a2, 0x8000194c <.LBB1_120+0x74>
80001948: 93 05 0b 00  	mv	a1, s6
;         if (!(flags & FLAGS_LEFT)) {
8000194c: 13 f6 2b 00  	andi	a2, s7, 2
80001950: 23 28 c1 00  	sw	a2, 16(sp)
80001954: 63 18 06 24  	bnez	a2, 0x80001ba4 <.LBB1_120+0x2cc>
;           while (l++ < width) {
80001958: 63 f6 55 41  	bgeu	a1, s5, 0x80001d64 <.LBB1_120+0x48c>
8000195c: 23 24 01 01  	sw	a6, 8(sp)
80001960: 93 04 00 00  	li	s1, 0
;           while (l++ < width) {
80001964: 13 85 1a 00  	addi	a0, s5, 1
80001968: 23 26 a1 00  	sw	a0, 12(sp)
8000196c: b3 8b ba 40  	sub	s7, s5, a1
80001970: 83 2c 41 01  	lw	s9, 20(sp)
80001974: 83 29 41 02  	lw	s3, 36(sp)
80001978: 13 00 00 00  	nop
8000197c: 13 00 00 00  	nop
;             out(' ', buffer, idx++, maxlen);
80001980: 33 06 9a 00  	add	a2, s4, s1
80001984: 13 05 00 02  	li	a0, 32
80001988: 93 85 0c 00  	mv	a1, s9
8000198c: 93 06 04 00  	mv	a3, s0
80001990: e7 80 09 00  	jalr	s3
;           while (l++ < width) {
80001994: 93 84 14 00  	addi	s1, s1, 1
80001998: e3 94 9b fe  	bne	s7, s1, 0x80001980 <.LBB1_120+0xa8>
8000199c: 03 28 81 00  	lw	a6, 8(sp)
;         while ((*p != 0) && (!(flags & FLAGS_PRECISION) || precision--)) {
800019a0: 03 45 08 00  	lbu	a0, 0(a6)
800019a4: 33 0a 9a 00  	add	s4, s4, s1
800019a8: 93 09 50 02  	li	s3, 37
800019ac: 63 10 05 20  	bnez	a0, 0x80001bac <.LBB1_120+0x2d4>
800019b0: 93 0b 0a 00  	mv	s7, s4
800019b4: 83 2c 41 01  	lw	s9, 20(sp)
800019b8: 6f 00 40 24  	j	0x80001bfc <.LBB1_120+0x324>
800019bc: 03 25 81 02  	lw	a0, 40(sp)
800019c0: 13 05 45 00  	addi	a0, a0, 4
;   return _ntoa_format(out, buffer, idx, maxlen, buf, len, negative, (unsigned int)base, prec, width, flags);
800019c4: 23 24 a1 02  	sw	a0, 40(sp)
800019c8: 13 08 00 00  	li	a6, 0
800019cc: 13 07 c1 02  	addi	a4, sp, 44
800019d0: 93 08 00 01  	li	a7, 16
800019d4: 13 0e 80 00  	li	t3, 8
800019d8: 13 05 0c 00  	mv	a0, s8
800019dc: 93 85 0c 00  	mv	a1, s9
800019e0: 13 06 0a 00  	mv	a2, s4
800019e4: 93 06 04 00  	mv	a3, s0
800019e8: 93 03 0b 00  	mv	t2, s6
800019ec: 6f 00 00 35  	j	0x80001d3c <.LBB1_120+0x464>
800019f0: 03 25 81 02  	lw	a0, 40(sp)
;         if (!(flags & FLAGS_LEFT)) {
800019f4: 33 3c b0 00  	snez	s8, a1
;         out((char)va_arg(va, int), buffer, idx++, maxlen);
800019f8: 13 0b 45 00  	addi	s6, a0, 4
800019fc: 03 45 05 00  	lbu	a0, 0(a0)
80001a00: 93 0b 1a 00  	addi	s7, s4, 1
80001a04: 93 85 0c 00  	mv	a1, s9
80001a08: 13 06 0a 00  	mv	a2, s4
80001a0c: 93 06 04 00  	mv	a3, s0
80001a10: 03 27 41 02  	lw	a4, 36(sp)
80001a14: e7 00 07 00  	jalr	a4
80001a18: 33 b5 54 01  	sltu	a0, s1, s5
;         if (flags & FLAGS_LEFT) {
80001a1c: 33 75 ac 00  	and	a0, s8, a0
80001a20: 63 0a 05 22  	beqz	a0, 0x80001c54 <.LBB1_120+0x37c>
;           while (l++ < width) {
80001a24: b3 84 9a 40  	sub	s1, s5, s1
80001a28: 03 2c 41 02  	lw	s8, 36(sp)
80001a2c: 13 00 00 00  	nop
80001a30: 13 86 0b 00  	mv	a2, s7
;             out(' ', buffer, idx++, maxlen);
80001a34: 93 8b 1b 00  	addi	s7, s7, 1
80001a38: 13 05 00 02  	li	a0, 32
80001a3c: 93 85 0c 00  	mv	a1, s9
80001a40: 93 06 04 00  	mv	a3, s0
80001a44: e7 00 0c 00  	jalr	s8
;           while (l++ < width) {
80001a48: 93 84 f4 ff  	addi	s1, s1, -1
80001a4c: e3 92 04 fe  	bnez	s1, 0x80001a30 <.LBB1_120+0x158>
80001a50: 23 24 61 03  	sw	s6, 40(sp)
80001a54: 6f 00 40 2f  	j	0x80001d48 <.LBB1_120+0x470>
;             const int value = (flags & FLAGS_CHAR) ? (char)va_arg(va, int) : (flags & FLAGS_SHORT) ? (short int)va_arg(va, int) : va_arg(va, int);
80001a58: 03 25 81 02  	lw	a0, 40(sp)
80001a5c: 03 45 05 00  	lbu	a0, 0(a0)
;   if (!value) {
80001a60: 63 14 05 00  	bnez	a0, 0x80001a68 <.LBB1_120+0x190>
80001a64: 93 fb fb fe  	andi	s7, s7, -17
80001a68: 93 07 00 00  	li	a5, 0
80001a6c: 93 35 15 00  	seqz	a1, a0
;   if (!(flags & FLAGS_PRECISION) || value) {
80001a70: 13 96 5b 01  	slli	a2, s7, 21
80001a74: 13 56 f6 01  	srli	a2, a2, 31
80001a78: b3 f5 c5 00  	and	a1, a1, a2
80001a7c: 63 9c 05 06  	bnez	a1, 0x80001af4 <.LBB1_120+0x21c>
80001a80: 93 05 00 00  	li	a1, 0
;             idx = _ntoa_long(out, buffer, idx, maxlen, (unsigned int)(value > 0 ? value : 0 - value), value < 0, base, precision, width, flags);
80001a84: 13 56 f5 41  	srai	a2, a0, 31
80001a88: b3 06 c5 00  	add	a3, a0, a2
80001a8c: b3 c6 c6 00  	xor	a3, a3, a2
80001a90: 13 f6 0b 02  	andi	a2, s7, 32
80001a94: 13 46 16 06  	xori	a2, a2, 97
80001a98: 13 06 66 0f  	addi	a2, a2, 246
80001a9c: 6f 00 40 03  	j	0x80001ad0 <.LBB1_120+0x1f8>
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
80001aa0: 33 08 f8 00  	add	a6, a6, a5
80001aa4: 93 87 15 00  	addi	a5, a1, 1
80001aa8: 93 08 c1 02  	addi	a7, sp, 44
80001aac: b3 88 b8 00  	add	a7, a7, a1
80001ab0: 23 80 08 01  	sb	a6, 0(a7)
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
80001ab4: b3 b6 86 01  	sltu	a3, a3, s8
80001ab8: 93 c6 16 00  	xori	a3, a3, 1
80001abc: 93 b5 f5 01  	sltiu	a1, a1, 31
80001ac0: 33 f8 b6 00  	and	a6, a3, a1
80001ac4: 93 85 07 00  	mv	a1, a5
80001ac8: 93 06 07 00  	mv	a3, a4
80001acc: 63 04 08 02  	beqz	a6, 0x80001af4 <.LBB1_120+0x21c>
;       value /= base;
80001ad0: 33 d7 86 03  	divu	a4, a3, s8
80001ad4: b3 07 87 03  	mul	a5, a4, s8
80001ad8: b3 87 f6 40  	sub	a5, a3, a5
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
80001adc: 93 f8 e7 0f  	andi	a7, a5, 254
80001ae0: 13 08 00 03  	li	a6, 48
80001ae4: 93 02 a0 00  	li	t0, 10
80001ae8: e3 ec 58 fa  	bltu	a7, t0, 0x80001aa0 <.LBB1_120+0x1c8>
80001aec: 13 08 06 00  	mv	a6, a2
80001af0: 6f f0 1f fb  	j	0x80001aa0 <.LBB1_120+0x1c8>
80001af4: 83 25 81 02  	lw	a1, 40(sp)
80001af8: 93 85 45 00  	addi	a1, a1, 4
80001afc: 23 24 b1 02  	sw	a1, 40(sp)
80001b00: 13 58 f5 01  	srli	a6, a0, 31
80001b04: 6f 00 00 21  	j	0x80001d14 <.LBB1_120+0x43c>
;             idx = _ntoa_long(out, buffer, idx, maxlen, va_arg(va, unsigned long), false, base, precision, width, flags);
80001b08: 03 25 81 02  	lw	a0, 40(sp)
80001b0c: 03 25 05 00  	lw	a0, 0(a0)
;   if (!value) {
80001b10: 63 14 05 00  	bnez	a0, 0x80001b18 <.LBB1_120+0x240>
80001b14: 93 fb fb fe  	andi	s7, s7, -17
80001b18: 93 07 00 00  	li	a5, 0
80001b1c: 93 35 15 00  	seqz	a1, a0
;   if (!(flags & FLAGS_PRECISION) || value) {
80001b20: 13 96 5b 01  	slli	a2, s7, 21
80001b24: 13 56 f6 01  	srli	a2, a2, 31
80001b28: b3 f5 c5 00  	and	a1, a1, a2
80001b2c: 63 9c 05 1c  	bnez	a1, 0x80001d04 <.LBB1_120+0x42c>
80001b30: 13 06 00 00  	li	a2, 0
80001b34: 93 f5 0b 02  	andi	a1, s7, 32
80001b38: 93 c5 15 06  	xori	a1, a1, 97
80001b3c: 93 85 65 0f  	addi	a1, a1, 246
80001b40: 6f 00 00 04  	j	0x80001b80 <.LBB1_120+0x2a8>
80001b44: 13 00 00 00  	nop
80001b48: 13 00 00 00  	nop
80001b4c: 13 00 00 00  	nop
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
80001b50: 33 87 e7 00  	add	a4, a5, a4
80001b54: 93 07 16 00  	addi	a5, a2, 1
80001b58: 13 08 c1 02  	addi	a6, sp, 44
80001b5c: 33 08 c8 00  	add	a6, a6, a2
80001b60: 23 00 e8 00  	sb	a4, 0(a6)
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
80001b64: 33 35 85 01  	sltu	a0, a0, s8
80001b68: 13 45 15 00  	xori	a0, a0, 1
80001b6c: 13 36 f6 01  	sltiu	a2, a2, 31
80001b70: 33 77 c5 00  	and	a4, a0, a2
80001b74: 13 86 07 00  	mv	a2, a5
80001b78: 13 85 06 00  	mv	a0, a3
80001b7c: 63 04 07 18  	beqz	a4, 0x80001d04 <.LBB1_120+0x42c>
;       value /= base;
80001b80: b3 56 85 03  	divu	a3, a0, s8
80001b84: 33 87 86 03  	mul	a4, a3, s8
80001b88: 33 07 e5 40  	sub	a4, a0, a4
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
80001b8c: 13 78 e7 0f  	andi	a6, a4, 254
80001b90: 93 07 00 03  	li	a5, 48
80001b94: 93 08 a0 00  	li	a7, 10
80001b98: e3 6c 18 fb  	bltu	a6, a7, 0x80001b50 <.LBB1_120+0x278>
80001b9c: 93 87 05 00  	mv	a5, a1
80001ba0: 6f f0 1f fb  	j	0x80001b50 <.LBB1_120+0x278>
80001ba4: 23 26 b1 00  	sw	a1, 12(sp)
;         while ((*p != 0) && (!(flags & FLAGS_PRECISION) || precision--)) {
80001ba8: e3 04 05 e0  	beqz	a0, 0x800019b0 <.LBB1_120+0xd8>
80001bac: 93 04 18 00  	addi	s1, a6, 1
80001bb0: 83 2c 41 01  	lw	s9, 20(sp)
80001bb4: 6f 00 c0 03  	j	0x80001bf0 <.LBB1_120+0x318>
80001bb8: 13 00 00 00  	nop
80001bbc: 13 00 00 00  	nop
80001bc0: 13 0b fb ff  	addi	s6, s6, -1
;           out(*(p++), buffer, idx++, maxlen);
80001bc4: 93 0b 1a 00  	addi	s7, s4, 1
80001bc8: 13 75 f5 0f  	andi	a0, a0, 255
80001bcc: 93 85 0c 00  	mv	a1, s9
80001bd0: 13 06 0a 00  	mv	a2, s4
80001bd4: 93 06 04 00  	mv	a3, s0
80001bd8: 03 27 41 02  	lw	a4, 36(sp)
80001bdc: e7 00 07 00  	jalr	a4
;         while ((*p != 0) && (!(flags & FLAGS_PRECISION) || precision--)) {
80001be0: 03 c5 04 00  	lbu	a0, 0(s1)
80001be4: 93 84 14 00  	addi	s1, s1, 1
80001be8: 13 8a 0b 00  	mv	s4, s7
;         while ((*p != 0) && (!(flags & FLAGS_PRECISION) || precision--)) {
80001bec: 63 08 05 00  	beqz	a0, 0x80001bfc <.LBB1_120+0x324>
80001bf0: e3 0a 0c fc  	beqz	s8, 0x80001bc4 <.LBB1_120+0x2ec>
80001bf4: e3 16 0b fc  	bnez	s6, 0x80001bc0 <.LBB1_120+0x2e8>
80001bf8: 93 0b 0a 00  	mv	s7, s4
80001bfc: 03 25 81 02  	lw	a0, 40(sp)
80001c00: 13 05 45 00  	addi	a0, a0, 4
;         if (!(flags & FLAGS_LEFT)) {
80001c04: 23 24 a1 02  	sw	a0, 40(sp)
80001c08: 03 25 01 01  	lw	a0, 16(sp)
80001c0c: 33 35 a0 00  	snez	a0, a0
80001c10: 03 26 c1 00  	lw	a2, 12(sp)
80001c14: b3 35 56 01  	sltu	a1, a2, s5
;         if (flags & FLAGS_LEFT) {
80001c18: 33 75 b5 00  	and	a0, a0, a1
80001c1c: 63 02 05 04  	beqz	a0, 0x80001c60 <.LBB1_120+0x388>
;           while (l++ < width) {
80001c20: b3 84 ca 40  	sub	s1, s5, a2
80001c24: 03 2c 41 02  	lw	s8, 36(sp)
80001c28: 13 0b e0 02  	li	s6, 46
80001c2c: 13 00 00 00  	nop
80001c30: 13 86 0b 00  	mv	a2, s7
;             out(' ', buffer, idx++, maxlen);
80001c34: 93 8b 1b 00  	addi	s7, s7, 1
80001c38: 13 05 00 02  	li	a0, 32
80001c3c: 93 85 0c 00  	mv	a1, s9
80001c40: 93 06 04 00  	mv	a3, s0
80001c44: e7 00 0c 00  	jalr	s8
;           while (l++ < width) {
80001c48: 93 84 f4 ff  	addi	s1, s1, -1
80001c4c: e3 92 04 fe  	bnez	s1, 0x80001c30 <.LBB1_120+0x358>
80001c50: 6f 00 c0 0f  	j	0x80001d4c <.LBB1_120+0x474>
80001c54: 23 24 61 03  	sw	s6, 40(sp)
80001c58: 03 2c 41 02  	lw	s8, 36(sp)
80001c5c: 6f 00 c0 0e  	j	0x80001d48 <.LBB1_120+0x470>
80001c60: 03 2c 41 02  	lw	s8, 36(sp)
80001c64: 6f 00 40 0e  	j	0x80001d48 <.LBB1_120+0x470>
;             const unsigned int value = (flags & FLAGS_CHAR) ? (unsigned char)va_arg(va, unsigned int) : (flags & FLAGS_SHORT) ? (unsigned short int)va_arg(va, unsigned int) : va_arg(va, unsigned int);
80001c68: 03 25 81 02  	lw	a0, 40(sp)
80001c6c: 03 45 05 00  	lbu	a0, 0(a0)
;   if (!value) {
80001c70: 63 14 05 00  	bnez	a0, 0x80001c78 <.LBB1_120+0x3a0>
80001c74: 93 fb fb fe  	andi	s7, s7, -17
80001c78: 93 07 00 00  	li	a5, 0
80001c7c: 93 35 15 00  	seqz	a1, a0
;   if (!(flags & FLAGS_PRECISION) || value) {
80001c80: 13 96 5b 01  	slli	a2, s7, 21
80001c84: 13 56 f6 01  	srli	a2, a2, 31
80001c88: b3 f5 c5 00  	and	a1, a1, a2
80001c8c: 63 9c 05 06  	bnez	a1, 0x80001d04 <.LBB1_120+0x42c>
80001c90: 13 06 00 00  	li	a2, 0
80001c94: 93 f5 0b 02  	andi	a1, s7, 32
80001c98: 93 c5 15 06  	xori	a1, a1, 97
80001c9c: 93 85 65 0f  	addi	a1, a1, 246
80001ca0: 6f 00 00 04  	j	0x80001ce0 <.LBB1_120+0x408>
80001ca4: 13 00 00 00  	nop
80001ca8: 13 00 00 00  	nop
80001cac: 13 00 00 00  	nop
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
80001cb0: 33 87 e7 00  	add	a4, a5, a4
80001cb4: 93 07 16 00  	addi	a5, a2, 1
80001cb8: 13 08 c1 02  	addi	a6, sp, 44
80001cbc: 33 08 c8 00  	add	a6, a6, a2
80001cc0: 23 00 e8 00  	sb	a4, 0(a6)
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
80001cc4: 33 35 85 01  	sltu	a0, a0, s8
80001cc8: 13 45 15 00  	xori	a0, a0, 1
80001ccc: 13 36 f6 01  	sltiu	a2, a2, 31
80001cd0: 33 77 c5 00  	and	a4, a0, a2
80001cd4: 13 86 07 00  	mv	a2, a5
80001cd8: 13 85 06 00  	mv	a0, a3
80001cdc: 63 04 07 02  	beqz	a4, 0x80001d04 <.LBB1_120+0x42c>
;       value /= base;
80001ce0: b3 56 85 03  	divu	a3, a0, s8
80001ce4: 33 87 86 03  	mul	a4, a3, s8
80001ce8: 33 07 e5 40  	sub	a4, a0, a4
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
80001cec: 13 78 e7 0f  	andi	a6, a4, 254
80001cf0: 93 07 00 03  	li	a5, 48
80001cf4: 93 08 a0 00  	li	a7, 10
80001cf8: e3 6c 18 fb  	bltu	a6, a7, 0x80001cb0 <.LBB1_120+0x3d8>
80001cfc: 93 87 05 00  	mv	a5, a1
80001d00: 6f f0 1f fb  	j	0x80001cb0 <.LBB1_120+0x3d8>
80001d04: 03 25 81 02  	lw	a0, 40(sp)
80001d08: 13 05 45 00  	addi	a0, a0, 4
80001d0c: 23 24 a1 02  	sw	a0, 40(sp)
80001d10: 13 08 00 00  	li	a6, 0
80001d14: 13 07 c1 02  	addi	a4, sp, 44
80001d18: 03 25 41 02  	lw	a0, 36(sp)
80001d1c: 93 85 0c 00  	mv	a1, s9
80001d20: 13 06 0a 00  	mv	a2, s4
80001d24: 93 06 04 00  	mv	a3, s0
80001d28: 93 08 0c 00  	mv	a7, s8
80001d2c: 13 0c 05 00  	mv	s8, a0
80001d30: 93 03 0b 00  	mv	t2, s6
80001d34: 13 8e 0a 00  	mv	t3, s5
80001d38: 93 8e 0b 00  	mv	t4, s7
80001d3c: 97 10 00 00  	auipc	ra, 1
80001d40: e7 80 40 ce  	jalr	-796(ra)
80001d44: 93 0b 05 00  	mv	s7, a0
80001d48: 13 0b e0 02  	li	s6, 46
80001d4c: 93 0a 19 00  	addi	s5, s2, 1
;   while (*format)
80001d50: 13 89 2a 00  	addi	s2, s5, 2
80001d54: 13 8a 0b 00  	mv	s4, s7
80001d58: 03 c5 0a 00  	lbu	a0, 0(s5)
80001d5c: 63 12 05 be  	bnez	a0, 0x80001140 <_vsnprintf+0xb0>
80001d60: 6f 00 40 01  	j	0x80001d74 <.LBB1_120+0x49c>
80001d64: 93 85 15 00  	addi	a1, a1, 1
80001d68: 23 26 b1 00  	sw	a1, 12(sp)
;         while ((*p != 0) && (!(flags & FLAGS_PRECISION) || precision--)) {
80001d6c: e3 10 05 e4  	bnez	a0, 0x80001bac <.LBB1_120+0x2d4>
80001d70: 6f f0 1f c4  	j	0x800019b0 <.LBB1_120+0xd8>
80001d74: 13 06 0a 00  	mv	a2, s4
80001d78: 63 64 8a 00  	bltu	s4, s0, 0x80001d80 <.LBB1_120+0x4a8>
80001d7c: 13 06 f4 ff  	addi	a2, s0, -1
;   out((char)0, buffer, idx < maxlen ? idx : maxlen - 1U, maxlen);
80001d80: 13 05 00 00  	li	a0, 0
80001d84: 93 85 0c 00  	mv	a1, s9
80001d88: 93 06 04 00  	mv	a3, s0
80001d8c: e7 00 0c 00  	jalr	s8
;   return (int)idx;
80001d90: 13 05 0a 00  	mv	a0, s4
80001d94: 83 20 c1 07  	lw	ra, 124(sp)
80001d98: 03 24 81 07  	lw	s0, 120(sp)
80001d9c: 83 24 41 07  	lw	s1, 116(sp)
80001da0: 03 29 01 07  	lw	s2, 112(sp)
80001da4: 83 29 c1 06  	lw	s3, 108(sp)
80001da8: 03 2a 81 06  	lw	s4, 104(sp)
80001dac: 83 2a 41 06  	lw	s5, 100(sp)
80001db0: 03 2b 01 06  	lw	s6, 96(sp)
80001db4: 83 2b c1 05  	lw	s7, 92(sp)
80001db8: 03 2c 81 05  	lw	s8, 88(sp)
80001dbc: 83 2c 41 05  	lw	s9, 84(sp)
80001dc0: 03 2d 01 05  	lw	s10, 80(sp)
80001dc4: 83 2d c1 04  	lw	s11, 76(sp)
80001dc8: 13 01 01 08  	addi	sp, sp, 128
80001dcc: 67 80 00 00  	ret

80001dd0 <_out_char>:
;   if (character) {
80001dd0: 63 06 05 00  	beqz	a0, 0x80001ddc <_out_char+0xc>
;     _putchar(character);
80001dd4: 17 13 00 00  	auipc	t1, 1
80001dd8: 67 00 c3 fa  	jr	-84(t1)
; }
80001ddc: 67 80 00 00  	ret

80001de0 <_out_null>:
; }
80001de0: 67 80 00 00  	ret
		...

80001df0 <_ftoa>:
; {
80001df0: 13 01 01 fa  	addi	sp, sp, -96
80001df4: 23 2e 11 04  	sw	ra, 92(sp)
80001df8: 23 2c 81 04  	sw	s0, 88(sp)
80001dfc: 23 2a 91 04  	sw	s1, 84(sp)
80001e00: 23 28 21 05  	sw	s2, 80(sp)
80001e04: 23 26 31 05  	sw	s3, 76(sp)
80001e08: 23 24 41 05  	sw	s4, 72(sp)
80001e0c: 23 22 51 05  	sw	s5, 68(sp)
80001e10: 23 20 61 05  	sw	s6, 64(sp)
80001e14: 23 2e 71 03  	sw	s7, 60(sp)
80001e18: 23 2c 81 03  	sw	s8, 56(sp)
80001e1c: 23 2a 91 03  	sw	s9, 52(sp)
80001e20: 23 28 a1 03  	sw	s10, 48(sp)
80001e24: 27 34 81 02  	fsd	fs0, 40(sp)
80001e28: 93 0a 08 00  	mv	s5, a6
80001e2c: 13 84 07 00  	mv	s0, a5
80001e30: 93 84 06 00  	mv	s1, a3
80001e34: 13 0a 06 00  	mv	s4, a2
80001e38: 13 89 05 00  	mv	s2, a1
80001e3c: 93 09 05 00  	mv	s3, a0
80001e40: 37 35 00 80  	lui	a0, 524291
80001e44: 07 30 05 5e  	fld	ft0, 1504(a0)
;   if (value < -DBL_MAX)
80001e48: 53 05 a0 a2  	fle.d	a0, ft0, fa0
80001e4c: 63 1e 05 0e  	bnez	a0, 0x80001f48 <_ftoa+0x158>
;   if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD)) {
80001e50: 13 fb 2a 00  	andi	s6, s5, 2
80001e54: 13 f5 3a 00  	andi	a0, s5, 3
80001e58: 33 35 a0 00  	snez	a0, a0
80001e5c: 93 35 54 00  	sltiu	a1, s0, 5
80001e60: 33 e5 a5 00  	or	a0, a1, a0
80001e64: 93 0a 0a 00  	mv	s5, s4
80001e68: 63 1c 05 02  	bnez	a0, 0x80001ea0 <_ftoa+0xb0>
;     for (size_t i = len; i < width; i++) {
80001e6c: 93 0b c4 ff  	addi	s7, s0, -4
80001e70: 13 06 0a 00  	mv	a2, s4
80001e74: 13 00 00 00  	nop
80001e78: 13 00 00 00  	nop
80001e7c: 13 00 00 00  	nop
;       out(' ', buffer, idx++, maxlen);
80001e80: 93 0a 16 00  	addi	s5, a2, 1
80001e84: 13 05 00 02  	li	a0, 32
80001e88: 93 05 09 00  	mv	a1, s2
80001e8c: 93 86 04 00  	mv	a3, s1
80001e90: e7 80 09 00  	jalr	s3
;     for (size_t i = len; i < width; i++) {
80001e94: 93 8b fb ff  	addi	s7, s7, -1
80001e98: 13 86 0a 00  	mv	a2, s5
;     for (size_t i = len; i < width; i++) {
80001e9c: e3 92 0b fe  	bnez	s7, 0x80001e80 <_ftoa+0x90>
;   if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD)) {
80001ea0: 13 3c 1b 00  	seqz	s8, s6
;     out(buf[--len], buffer, idx++, maxlen);
80001ea4: 13 8b 1a 00  	addi	s6, s5, 1
80001ea8: 13 05 d0 02  	li	a0, 45
80001eac: 93 05 09 00  	mv	a1, s2
80001eb0: 13 86 0a 00  	mv	a2, s5
80001eb4: 93 86 04 00  	mv	a3, s1
80001eb8: e7 80 09 00  	jalr	s3
80001ebc: 93 8b 2a 00  	addi	s7, s5, 2
80001ec0: 13 05 90 06  	li	a0, 105
80001ec4: 93 05 09 00  	mv	a1, s2
80001ec8: 13 06 0b 00  	mv	a2, s6
80001ecc: 93 86 04 00  	mv	a3, s1
80001ed0: e7 80 09 00  	jalr	s3
80001ed4: 13 8b 3a 00  	addi	s6, s5, 3
80001ed8: 13 05 e0 06  	li	a0, 110
80001edc: 93 05 09 00  	mv	a1, s2
80001ee0: 13 86 0b 00  	mv	a2, s7
80001ee4: 93 86 04 00  	mv	a3, s1
80001ee8: e7 80 09 00  	jalr	s3
80001eec: 93 8a 4a 00  	addi	s5, s5, 4
80001ef0: 13 05 60 06  	li	a0, 102
80001ef4: 93 05 09 00  	mv	a1, s2
80001ef8: 13 06 0b 00  	mv	a2, s6
80001efc: 93 86 04 00  	mv	a3, s1
80001f00: e7 80 09 00  	jalr	s3
80001f04: 33 85 4a 41  	sub	a0, s5, s4
80001f08: 33 35 85 00  	sltu	a0, a0, s0
80001f0c: 13 45 15 00  	xori	a0, a0, 1
;   if (flags & FLAGS_LEFT) {
80001f10: 33 65 ac 00  	or	a0, s8, a0
80001f14: 63 12 05 5e  	bnez	a0, 0x800024f8 <_ftoa+0x708>
;     while (idx - start_idx < width) {
80001f18: 33 0b 40 41  	neg	s6, s4
80001f1c: 13 00 00 00  	nop
;       out(' ', buffer, idx++, maxlen);
80001f20: 13 8a 1a 00  	addi	s4, s5, 1
80001f24: 13 05 00 02  	li	a0, 32
80001f28: 93 05 09 00  	mv	a1, s2
80001f2c: 13 86 0a 00  	mv	a2, s5
80001f30: 93 86 04 00  	mv	a3, s1
80001f34: e7 80 09 00  	jalr	s3
;     while (idx - start_idx < width) {
80001f38: 33 05 4b 01  	add	a0, s6, s4
80001f3c: 93 0a 0a 00  	mv	s5, s4
80001f40: e3 60 85 fe  	bltu	a0, s0, 0x80001f20 <_ftoa+0x130>
80001f44: 6f 00 80 5b  	j	0x800024fc <_ftoa+0x70c>
80001f48: 53 04 a5 22  	fmv.d	fs0, fa0
80001f4c: 37 35 00 80  	lui	a0, 524291
80001f50: 07 30 85 5e  	fld	ft0, 1512(a0)
;   if (value > DBL_MAX)
80001f54: 53 05 05 a2  	fle.d	a0, fa0, ft0
80001f58: 63 1e 05 00  	bnez	a0, 0x80001f74 <_ftoa+0x184>
;     return _out_rev(out, buffer, idx, maxlen, (flags & FLAGS_PLUS) ? "fni+" : "fni", (flags & FLAGS_PLUS) ? 4U : 3U, width, flags);
80001f5c: 93 f5 4a 00  	andi	a1, s5, 4
80001f60: 13 b5 15 00  	seqz	a0, a1
80001f64: 63 86 05 08  	beqz	a1, 0x80001ff0 <_ftoa+0x200>
80001f68: b7 35 00 80  	lui	a1, 524291
80001f6c: 13 8c f5 35  	addi	s8, a1, 863
80001f70: 6f 00 80 08  	j	0x80001ff8 <_ftoa+0x208>
80001f74: 37 35 00 80  	lui	a0, 524291
80001f78: 07 30 05 5f  	fld	ft0, 1520(a0)
80001f7c: 37 35 00 80  	lui	a0, 524291
80001f80: 87 30 85 5f  	fld	ft1, 1528(a0)
;   if ((value > PRINTF_MAX_FLOAT) || (value < -PRINTF_MAX_FLOAT)) {
80001f84: 53 15 80 a2  	flt.d	a0, ft0, fs0
80001f88: d3 15 14 a2  	flt.d	a1, fs0, ft1
80001f8c: 33 65 b5 00  	or	a0, a0, a1
80001f90: 63 04 05 14  	beqz	a0, 0x800020d8 <_ftoa+0x2e8>
;     return _etoa(out, buffer, idx, maxlen, value, prec, width, flags);
80001f94: 13 85 09 00  	mv	a0, s3
80001f98: 93 05 09 00  	mv	a1, s2
80001f9c: 13 06 0a 00  	mv	a2, s4
80001fa0: 93 86 04 00  	mv	a3, s1
80001fa4: 53 05 84 22  	fmv.d	fa0, fs0
80001fa8: 93 07 04 00  	mv	a5, s0
80001fac: 13 88 0a 00  	mv	a6, s5
80001fb0: 83 20 c1 05  	lw	ra, 92(sp)
80001fb4: 03 24 81 05  	lw	s0, 88(sp)
80001fb8: 83 24 41 05  	lw	s1, 84(sp)
80001fbc: 03 29 01 05  	lw	s2, 80(sp)
80001fc0: 83 29 c1 04  	lw	s3, 76(sp)
80001fc4: 03 2a 81 04  	lw	s4, 72(sp)
80001fc8: 83 2a 41 04  	lw	s5, 68(sp)
80001fcc: 03 2b 01 04  	lw	s6, 64(sp)
80001fd0: 83 2b c1 03  	lw	s7, 60(sp)
80001fd4: 03 2c 81 03  	lw	s8, 56(sp)
80001fd8: 83 2c 41 03  	lw	s9, 52(sp)
80001fdc: 03 2d 01 03  	lw	s10, 48(sp)
80001fe0: 07 34 81 02  	fld	fs0, 40(sp)
80001fe4: 13 01 01 06  	addi	sp, sp, 96
80001fe8: 17 03 00 00  	auipc	t1, 0
80001fec: 67 00 83 55  	jr	1368(t1)
80001ff0: b7 35 00 80  	lui	a1, 524291
80001ff4: 13 8c 45 36  	addi	s8, a1, 868
80001ff8: 93 05 40 00  	li	a1, 4
;     return _out_rev(out, buffer, idx, maxlen, (flags & FLAGS_PLUS) ? "fni+" : "fni", (flags & FLAGS_PLUS) ? 4U : 3U, width, flags);
80001ffc: b3 8b a5 40  	sub	s7, a1, a0
;   if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD)) {
80002000: 13 f5 3a 00  	andi	a0, s5, 3
80002004: 33 35 a0 00  	snez	a0, a0
80002008: b3 b5 8b 00  	sltu	a1, s7, s0
8000200c: 93 c5 15 00  	xori	a1, a1, 1
80002010: 33 65 b5 00  	or	a0, a0, a1
80002014: 13 0b 0a 00  	mv	s6, s4
80002018: 63 1c 05 02  	bnez	a0, 0x80002050 <_ftoa+0x260>
;     for (size_t i = len; i < width; i++) {
8000201c: b3 0c 74 41  	sub	s9, s0, s7
80002020: 13 06 0a 00  	mv	a2, s4
80002024: 13 00 00 00  	nop
80002028: 13 00 00 00  	nop
8000202c: 13 00 00 00  	nop
;       out(' ', buffer, idx++, maxlen);
80002030: 13 0b 16 00  	addi	s6, a2, 1
80002034: 13 05 00 02  	li	a0, 32
80002038: 93 05 09 00  	mv	a1, s2
8000203c: 93 86 04 00  	mv	a3, s1
80002040: e7 80 09 00  	jalr	s3
;     for (size_t i = len; i < width; i++) {
80002044: 93 8c fc ff  	addi	s9, s9, -1
80002048: 13 06 0b 00  	mv	a2, s6
;     for (size_t i = len; i < width; i++) {
8000204c: e3 92 0c fe  	bnez	s9, 0x80002030 <_ftoa+0x240>
80002050: 93 fa 2a 00  	andi	s5, s5, 2
;   while (len) {
80002054: 13 0c fc ff  	addi	s8, s8, -1
80002058: 13 00 00 00  	nop
8000205c: 13 00 00 00  	nop
80002060: 13 06 0b 00  	mv	a2, s6
;     out(buf[--len], buffer, idx++, maxlen);
80002064: 93 8c fb ff  	addi	s9, s7, -1
80002068: 33 05 7c 01  	add	a0, s8, s7
8000206c: 03 45 05 00  	lbu	a0, 0(a0)
80002070: 13 0b 1b 00  	addi	s6, s6, 1
80002074: 93 05 09 00  	mv	a1, s2
80002078: 93 86 04 00  	mv	a3, s1
8000207c: e7 80 09 00  	jalr	s3
80002080: 93 8b 0c 00  	mv	s7, s9
;   while (len) {
80002084: e3 9e 0c fc  	bnez	s9, 0x80002060 <_ftoa+0x270>
;   if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD)) {
80002088: 13 b5 1a 00  	seqz	a0, s5
8000208c: b3 05 4b 41  	sub	a1, s6, s4
80002090: b3 b5 85 00  	sltu	a1, a1, s0
80002094: 93 c5 15 00  	xori	a1, a1, 1
;   if (flags & FLAGS_LEFT) {
80002098: 33 65 b5 00  	or	a0, a0, a1
8000209c: 63 16 05 06  	bnez	a0, 0x80002108 <_ftoa+0x318>
;     while (idx - start_idx < width) {
800020a0: b3 0a 40 41  	neg	s5, s4
800020a4: 13 00 00 00  	nop
800020a8: 13 00 00 00  	nop
800020ac: 13 00 00 00  	nop
;       out(' ', buffer, idx++, maxlen);
800020b0: 13 0a 1b 00  	addi	s4, s6, 1
800020b4: 13 05 00 02  	li	a0, 32
800020b8: 93 05 09 00  	mv	a1, s2
800020bc: 13 06 0b 00  	mv	a2, s6
800020c0: 93 86 04 00  	mv	a3, s1
800020c4: e7 80 09 00  	jalr	s3
;     while (idx - start_idx < width) {
800020c8: 33 85 4a 01  	add	a0, s5, s4
800020cc: 13 0b 0a 00  	mv	s6, s4
800020d0: e3 60 85 fe  	bltu	a0, s0, 0x800020b0 <_ftoa+0x2c0>
800020d4: 6f 00 80 42  	j	0x800024fc <_ftoa+0x70c>
800020d8: 53 00 00 d2  	fcvt.d.w	ft0, zero
;   if (value < 0) {
800020dc: d3 1b 04 a2  	flt.d	s7, fs0, ft0
800020e0: 63 98 0b 02  	bnez	s7, 0x80002110 <_ftoa+0x320>
;   if (!(flags & FLAGS_PRECISION)) {
800020e4: 13 f5 0a 40  	andi	a0, s5, 1024
800020e8: 93 0c 60 00  	li	s9, 6
800020ec: 63 0a 05 02  	beqz	a0, 0x80002120 <_ftoa+0x330>
800020f0: 93 0c 07 00  	mv	s9, a4
800020f4: 13 05 a0 00  	li	a0, 10
;   while ((len < PRINTF_FTOA_BUFFER_SIZE) && (prec > 9U)) {
800020f8: 63 f8 ac 02  	bgeu	s9, a0, 0x80002128 <_ftoa+0x338>
800020fc: 13 0b 00 00  	li	s6, 0
80002100: 13 06 10 00  	li	a2, 1
80002104: 6f 00 00 07  	j	0x80002174 <_ftoa+0x384>
80002108: 13 0a 0b 00  	mv	s4, s6
8000210c: 6f 00 00 3f  	j	0x800024fc <_ftoa+0x70c>
80002110: 53 14 84 22  	fneg.d	fs0, fs0
;   if (!(flags & FLAGS_PRECISION)) {
80002114: 13 f5 0a 40  	andi	a0, s5, 1024
80002118: 93 0c 60 00  	li	s9, 6
8000211c: e3 1a 05 fc  	bnez	a0, 0x800020f0 <_ftoa+0x300>
80002120: 13 05 a0 00  	li	a0, 10
;   while ((len < PRINTF_FTOA_BUFFER_SIZE) && (prec > 9U)) {
80002124: e3 ec ac fc  	bltu	s9, a0, 0x800020fc <_ftoa+0x30c>
80002128: 13 85 6c ff  	addi	a0, s9, -10
8000212c: 93 05 f0 01  	li	a1, 31
80002130: 63 64 b5 00  	bltu	a0, a1, 0x80002138 <_ftoa+0x348>
80002134: 13 05 f0 01  	li	a0, 31
;   while ((len < PRINTF_FTOA_BUFFER_SIZE) && (prec > 9U)) {
80002138: 13 0b 15 00  	addi	s6, a0, 1
;     buf[len++] = '0';
8000213c: 13 05 81 00  	addi	a0, sp, 8
80002140: 93 05 00 03  	li	a1, 48
80002144: 13 06 0b 00  	mv	a2, s6
80002148: 97 e0 ff ff  	auipc	ra, 1048574
8000214c: e7 80 00 72  	jalr	1824(ra)
80002150: 13 05 00 00  	li	a0, 0
80002154: 13 00 00 00  	nop
80002158: 13 00 00 00  	nop
8000215c: 13 00 00 00  	nop
;     buf[len++] = '0';
80002160: 13 05 15 00  	addi	a0, a0, 1
;   while ((len < PRINTF_FTOA_BUFFER_SIZE) && (prec > 9U)) {
80002164: e3 1e ab fe  	bne	s6, a0, 0x80002160 <_ftoa+0x370>
80002168: 93 05 f5 ff  	addi	a1, a0, -1
8000216c: 13 b6 f5 01  	sltiu	a2, a1, 31
;   int whole = (int)value;
80002170: b3 8c ac 40  	sub	s9, s9, a0
80002174: 53 15 04 c2  	fcvt.w.d	a0, fs0, rtz
;   double tmp = (value - whole) * pow10[prec];
80002178: 53 00 05 d2  	fcvt.d.w	ft0, a0
8000217c: 53 71 04 0a  	fsub.d	ft2, fs0, ft0
80002180: b7 35 00 80  	lui	a1, 524291
80002184: 93 85 85 1e  	addi	a1, a1, 488
80002188: 93 96 3c 00  	slli	a3, s9, 3
8000218c: b3 85 b6 00  	add	a1, a3, a1
80002190: 07 b0 05 00  	fld	ft0, 0(a1)
80002194: b7 35 00 80  	lui	a1, 524291
80002198: 87 b0 05 60  	fld	ft1, 1536(a1)
8000219c: 53 71 20 12  	fmul.d	ft2, ft0, ft2
;   unsigned long frac = (unsigned long)tmp;
800021a0: d3 15 11 c2  	fcvt.wu.d	a1, ft2, rtz
;   diff = tmp - frac;
800021a4: d3 81 15 d2  	fcvt.d.wu	ft3, a1
800021a8: 53 71 31 0a  	fsub.d	ft2, ft2, ft3
;   if (diff > 0.5) {
800021ac: d3 06 11 a2  	fle.d	a3, ft2, ft1
800021b0: 63 90 06 02  	bnez	a3, 0x800021d0 <_ftoa+0x3e0>
;     ++frac;
800021b4: 93 85 15 00  	addi	a1, a1, 1
;     if (frac >= pow10[prec]) {
800021b8: d3 80 15 d2  	fcvt.d.wu	ft1, a1
800021bc: d3 96 00 a2  	flt.d	a3, ft1, ft0
800021c0: 63 94 06 02  	bnez	a3, 0x800021e8 <_ftoa+0x3f8>
800021c4: 93 05 00 00  	li	a1, 0
;       ++whole;
800021c8: 13 05 15 00  	addi	a0, a0, 1
800021cc: 6f 00 c0 01  	j	0x800021e8 <_ftoa+0x3f8>
;   else if (diff < 0.5) {
800021d0: d3 16 11 a2  	flt.d	a3, ft2, ft1
800021d4: 63 9a 06 00  	bnez	a3, 0x800021e8 <_ftoa+0x3f8>
;   else if ((frac == 0U) || (frac & 1U)) {
800021d8: 93 b6 15 00  	seqz	a3, a1
800021dc: b3 e6 b6 00  	or	a3, a3, a1
800021e0: 93 f6 16 00  	andi	a3, a3, 1
800021e4: b3 85 b6 00  	add	a1, a3, a1
;   if (prec == 0U) {
800021e8: 63 84 0c 08  	beqz	s9, 0x80002270 <_ftoa+0x480>
800021ec: 13 0c 00 02  	li	s8, 32
;     while (len < PRINTF_FTOA_BUFFER_SIZE) {
800021f0: 63 0a 06 18  	beqz	a2, 0x80002384 <_ftoa+0x594>
800021f4: 13 06 00 00  	li	a2, 0
800021f8: 93 06 81 00  	addi	a3, sp, 8
;     while (len < PRINTF_FTOA_BUFFER_SIZE) {
800021fc: b3 86 66 01  	add	a3, a3, s6
80002200: 37 d7 cc cc  	lui	a4, 838861
80002204: 13 07 d7 cc  	addi	a4, a4, -819
80002208: 93 07 a0 00  	li	a5, 10
8000220c: 13 08 90 00  	li	a6, 9
;       --count;
80002210: b3 08 cb 00  	add	a7, s6, a2
;       if (!(frac /= 10U)) {
80002214: b3 b2 e5 02  	mulhu	t0, a1, a4
80002218: 93 d2 32 00  	srli	t0, t0, 3
8000221c: 33 83 f2 02  	mul	t1, t0, a5
80002220: 33 83 65 40  	sub	t1, a1, t1
;       buf[len++] = (char)(48U + (frac % 10U));
80002224: 13 63 03 03  	ori	t1, t1, 48
80002228: b3 83 c6 00  	add	t2, a3, a2
8000222c: 23 80 63 00  	sb	t1, 0(t2)
;       if (!(frac /= 10U)) {
80002230: b3 35 b8 00  	sltu	a1, a6, a1
80002234: 93 b8 f8 01  	sltiu	a7, a7, 31
80002238: b3 f8 15 01  	and	a7, a1, a7
8000223c: 13 06 16 00  	addi	a2, a2, 1
80002240: 93 85 02 00  	mv	a1, t0
;       if (!(frac /= 10U)) {
80002244: e3 96 08 fc  	bnez	a7, 0x80002210 <_ftoa+0x420>
;     while ((len < PRINTF_FTOA_BUFFER_SIZE) && (count-- > 0U)) {
80002248: 33 0c cb 00  	add	s8, s6, a2
8000224c: 93 05 fc ff  	addi	a1, s8, -1
80002250: 93 06 e0 01  	li	a3, 30
;     while ((len < PRINTF_FTOA_BUFFER_SIZE) && (count-- > 0U)) {
80002254: b3 b6 b6 00  	sltu	a3, a3, a1
80002258: 33 c7 cc 00  	xor	a4, s9, a2
8000225c: 13 37 17 00  	seqz	a4, a4
80002260: b3 e6 e6 00  	or	a3, a3, a4
80002264: 63 84 06 04  	beqz	a3, 0x800022ac <_ftoa+0x4bc>
80002268: 13 b6 f5 01  	sltiu	a2, a1, 31
8000226c: 6f 00 40 09  	j	0x80002300 <_ftoa+0x510>
;     diff = value - (double)whole;
80002270: 53 00 05 d2  	fcvt.d.w	ft0, a0
80002274: 53 70 04 0a  	fsub.d	ft0, fs0, ft0
80002278: b7 35 00 80  	lui	a1, 524291
8000227c: 87 b0 05 60  	fld	ft1, 1536(a1)
80002280: 93 05 05 00  	mv	a1, a0
;     if ((!(diff < 0.5) || (diff > 0.5)) && (whole & 1)) {
80002284: 53 86 00 a2  	fle.d	a2, ft1, ft0
80002288: 63 16 06 00  	bnez	a2, 0x80002294 <_ftoa+0x4a4>
8000228c: d3 95 00 a2  	flt.d	a1, ft1, ft0
80002290: b3 75 b5 00  	and	a1, a0, a1
;     if ((!(diff < 0.5) || (diff > 0.5)) && (whole & 1)) {
80002294: 93 f5 15 00  	andi	a1, a1, 1
80002298: 33 05 b5 00  	add	a0, a0, a1
8000229c: 13 0c 00 02  	li	s8, 32
800022a0: 93 05 f0 01  	li	a1, 31
;   while (len < PRINTF_FTOA_BUFFER_SIZE) {
800022a4: 63 f0 65 09  	bgeu	a1, s6, 0x80002324 <_ftoa+0x534>
800022a8: 6f 00 c0 0d  	j	0x80002384 <_ftoa+0x594>
800022ac: 93 05 00 00  	li	a1, 0
;     while ((len < PRINTF_FTOA_BUFFER_SIZE) && (count-- > 0U)) {
800022b0: 93 46 f6 ff  	not	a3, a2
800022b4: 33 87 96 01  	add	a4, a3, s9
;       buf[len++] = '0';
800022b8: b3 06 cb 00  	add	a3, s6, a2
800022bc: 93 07 81 00  	addi	a5, sp, 8
;     while ((len < PRINTF_FTOA_BUFFER_SIZE) && (count-- > 0U)) {
800022c0: b3 87 67 01  	add	a5, a5, s6
800022c4: 33 86 c7 00  	add	a2, a5, a2
800022c8: 93 07 00 03  	li	a5, 48
800022cc: 13 08 f0 01  	li	a6, 31
;       buf[len++] = '0';
800022d0: b3 88 b6 00  	add	a7, a3, a1
800022d4: 93 88 18 00  	addi	a7, a7, 1
800022d8: b3 02 b6 00  	add	t0, a2, a1
800022dc: 23 80 f2 00  	sb	a5, 0(t0)
;     while ((len < PRINTF_FTOA_BUFFER_SIZE) && (count-- > 0U)) {
800022e0: b3 32 18 01  	sltu	t0, a6, a7
800022e4: 33 43 b7 00  	xor	t1, a4, a1
800022e8: 13 33 13 00  	seqz	t1, t1
800022ec: b3 e2 62 00  	or	t0, t0, t1
800022f0: 93 85 15 00  	addi	a1, a1, 1
800022f4: e3 8e 02 fc  	beqz	t0, 0x800022d0 <_ftoa+0x4e0>
800022f8: 13 b6 08 02  	sltiu	a2, a7, 32
;     if (len < PRINTF_FTOA_BUFFER_SIZE) {
800022fc: 33 8c b6 00  	add	s8, a3, a1
80002300: 63 02 06 08  	beqz	a2, 0x80002384 <_ftoa+0x594>
;       buf[len++] = '.';
80002304: 13 0b 1c 00  	addi	s6, s8, 1
80002308: 93 05 81 00  	addi	a1, sp, 8
;       buf[len++] = '.';
8000230c: b3 85 85 01  	add	a1, a1, s8
80002310: 13 06 e0 02  	li	a2, 46
80002314: 23 80 c5 00  	sb	a2, 0(a1)
80002318: 13 0c 00 02  	li	s8, 32
8000231c: 93 05 f0 01  	li	a1, 31
;   while (len < PRINTF_FTOA_BUFFER_SIZE) {
80002320: 63 e2 65 07  	bltu	a1, s6, 0x80002384 <_ftoa+0x594>
80002324: b7 65 66 66  	lui	a1, 419430
80002328: 93 85 75 66  	addi	a1, a1, 1639
8000232c: 13 06 a0 00  	li	a2, 10
80002330: 93 06 81 00  	addi	a3, sp, 8
80002334: 13 00 00 00  	nop
80002338: 13 00 00 00  	nop
8000233c: 13 00 00 00  	nop
;     if (!(whole /= 10)) {
80002340: 33 17 b5 02  	mulh	a4, a0, a1
80002344: 93 57 f7 01  	srli	a5, a4, 31
80002348: 13 57 27 40  	srai	a4, a4, 2
8000234c: 33 07 f7 00  	add	a4, a4, a5
80002350: b3 07 c7 02  	mul	a5, a4, a2
80002354: b3 07 f5 40  	sub	a5, a0, a5
;     buf[len++] = (char)(48 + (whole % 10));
80002358: 93 87 07 03  	addi	a5, a5, 48
8000235c: 13 0c 1b 00  	addi	s8, s6, 1
80002360: 33 88 66 01  	add	a6, a3, s6
80002364: 23 00 f8 00  	sb	a5, 0(a6)
;     if (!(whole /= 10)) {
80002368: 13 05 65 ff  	addi	a0, a0, -10
8000236c: 13 35 d5 fe  	sltiu	a0, a0, -19
80002370: 93 37 fb 01  	sltiu	a5, s6, 31
80002374: b3 77 f5 00  	and	a5, a0, a5
80002378: 13 0b 0c 00  	mv	s6, s8
8000237c: 13 05 07 00  	mv	a0, a4
80002380: e3 90 07 fc  	bnez	a5, 0x80002340 <_ftoa+0x550>
;   if (!(flags & FLAGS_LEFT) && (flags & FLAGS_ZEROPAD)) {
80002384: 13 fb 3a 00  	andi	s6, s5, 3
80002388: 13 05 10 00  	li	a0, 1
8000238c: 63 16 ab 06  	bne	s6, a0, 0x800023f8 <_ftoa+0x608>
;     if (width && (negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) {
80002390: 63 0c 04 00  	beqz	s0, 0x800023a8 <_ftoa+0x5b8>
80002394: 13 f5 ca 00  	andi	a0, s5, 12
80002398: 33 35 a0 00  	snez	a0, a0
;     if (width && (negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) {
8000239c: 33 e5 ab 00  	or	a0, s7, a0
800023a0: 33 04 a4 40  	sub	s0, s0, a0
800023a4: 6f 00 80 00  	j	0x800023ac <_ftoa+0x5bc>
800023a8: 13 04 00 00  	li	s0, 0
;     while ((len < width) && (len < PRINTF_FTOA_BUFFER_SIZE)) {
800023ac: 33 35 8c 00  	sltu	a0, s8, s0
800023b0: 13 45 15 00  	xori	a0, a0, 1
800023b4: 93 05 f0 01  	li	a1, 31
800023b8: 33 b6 85 01  	sltu	a2, a1, s8
800023bc: 33 65 c5 00  	or	a0, a0, a2
800023c0: 63 1c 05 02  	bnez	a0, 0x800023f8 <_ftoa+0x608>
800023c4: 13 05 81 00  	addi	a0, sp, 8
;     while ((len < width) && (len < PRINTF_FTOA_BUFFER_SIZE)) {
800023c8: 33 05 85 01  	add	a0, a0, s8
800023cc: 13 46 fc ff  	not	a2, s8
800023d0: b3 0c c4 00  	add	s9, s0, a2
800023d4: b3 85 85 41  	sub	a1, a1, s8
800023d8: 63 e4 bc 00  	bltu	s9, a1, 0x800023e0 <_ftoa+0x5f0>
800023dc: 93 8c 05 00  	mv	s9, a1
;     while ((len < width) && (len < PRINTF_FTOA_BUFFER_SIZE)) {
800023e0: 13 86 1c 00  	addi	a2, s9, 1
;       buf[len++] = '0';
800023e4: 93 05 00 03  	li	a1, 48
800023e8: 97 e0 ff ff  	auipc	ra, 1048574
800023ec: e7 80 00 48  	jalr	1152(ra)
;     while ((len < width) && (len < PRINTF_FTOA_BUFFER_SIZE)) {
800023f0: 33 05 9c 01  	add	a0, s8, s9
800023f4: 13 0c 15 00  	addi	s8, a0, 1
800023f8: 13 05 f0 01  	li	a0, 31
;   if (len < PRINTF_FTOA_BUFFER_SIZE) {
800023fc: 63 6a 85 03  	bltu	a0, s8, 0x80002430 <_ftoa+0x640>
80002400: 13 05 d0 02  	li	a0, 45
;     if (negative) {
80002404: 63 9e 0b 00  	bnez	s7, 0x80002420 <_ftoa+0x630>
;     else if (flags & FLAGS_PLUS) {
80002408: 93 f5 4a 00  	andi	a1, s5, 4
8000240c: 13 05 b0 02  	li	a0, 43
80002410: 63 98 05 00  	bnez	a1, 0x80002420 <_ftoa+0x630>
;     else if (flags & FLAGS_SPACE) {
80002414: 93 f5 8a 00  	andi	a1, s5, 8
80002418: 13 05 00 02  	li	a0, 32
8000241c: 63 8a 05 00  	beqz	a1, 0x80002430 <_ftoa+0x640>
80002420: 93 05 81 00  	addi	a1, sp, 8
80002424: b3 85 85 01  	add	a1, a1, s8
80002428: 13 0c 1c 00  	addi	s8, s8, 1
8000242c: 23 80 a5 00  	sb	a0, 0(a1)
;   if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD)) {
80002430: 33 35 60 01  	snez	a0, s6
80002434: b3 35 8c 00  	sltu	a1, s8, s0
80002438: 93 c5 15 00  	xori	a1, a1, 1
8000243c: 33 65 b5 00  	or	a0, a0, a1
80002440: 13 0b 0a 00  	mv	s6, s4
;   if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD)) {
80002444: 63 16 05 02  	bnez	a0, 0x80002470 <_ftoa+0x680>
;     for (size_t i = len; i < width; i++) {
80002448: b3 0b 84 41  	sub	s7, s0, s8
8000244c: 13 06 0a 00  	mv	a2, s4
;       out(' ', buffer, idx++, maxlen);
80002450: 13 0b 16 00  	addi	s6, a2, 1
80002454: 13 05 00 02  	li	a0, 32
80002458: 93 05 09 00  	mv	a1, s2
8000245c: 93 86 04 00  	mv	a3, s1
80002460: e7 80 09 00  	jalr	s3
;     for (size_t i = len; i < width; i++) {
80002464: 93 8b fb ff  	addi	s7, s7, -1
80002468: 13 06 0b 00  	mv	a2, s6
;     for (size_t i = len; i < width; i++) {
8000246c: e3 92 0b fe  	bnez	s7, 0x80002450 <_ftoa+0x660>
80002470: 93 fb 2a 00  	andi	s7, s5, 2
;   while (len) {
80002474: 63 0e 0c 02  	beqz	s8, 0x800024b0 <_ftoa+0x6c0>
80002478: 93 0c 71 00  	addi	s9, sp, 7
8000247c: 13 00 00 00  	nop
;     out(buf[--len], buffer, idx++, maxlen);
80002480: 13 0d fc ff  	addi	s10, s8, -1
80002484: 33 85 8c 01  	add	a0, s9, s8
80002488: 03 45 05 00  	lbu	a0, 0(a0)
8000248c: 93 0a 1b 00  	addi	s5, s6, 1
80002490: 93 05 09 00  	mv	a1, s2
80002494: 13 06 0b 00  	mv	a2, s6
80002498: 93 86 04 00  	mv	a3, s1
8000249c: e7 80 09 00  	jalr	s3
800024a0: 13 8b 0a 00  	mv	s6, s5
800024a4: 13 0c 0d 00  	mv	s8, s10
;   while (len) {
800024a8: e3 1c 0d fc  	bnez	s10, 0x80002480 <_ftoa+0x690>
800024ac: 6f 00 80 00  	j	0x800024b4 <_ftoa+0x6c4>
800024b0: 93 0a 0b 00  	mv	s5, s6
;   if (!(flags & FLAGS_LEFT) && (flags & FLAGS_ZEROPAD)) {
800024b4: 13 b5 1b 00  	seqz	a0, s7
800024b8: b3 85 4a 41  	sub	a1, s5, s4
800024bc: b3 b5 85 00  	sltu	a1, a1, s0
800024c0: 93 c5 15 00  	xori	a1, a1, 1
;   if (flags & FLAGS_LEFT) {
800024c4: 33 65 b5 00  	or	a0, a0, a1
800024c8: 63 18 05 02  	bnez	a0, 0x800024f8 <_ftoa+0x708>
;     while (idx - start_idx < width) {
800024cc: 33 0b 40 41  	neg	s6, s4
;       out(' ', buffer, idx++, maxlen);
800024d0: 13 8a 1a 00  	addi	s4, s5, 1
800024d4: 13 05 00 02  	li	a0, 32
800024d8: 93 05 09 00  	mv	a1, s2
800024dc: 13 86 0a 00  	mv	a2, s5
800024e0: 93 86 04 00  	mv	a3, s1
800024e4: e7 80 09 00  	jalr	s3
;     while (idx - start_idx < width) {
800024e8: 33 05 4b 01  	add	a0, s6, s4
800024ec: 93 0a 0a 00  	mv	s5, s4
800024f0: e3 60 85 fe  	bltu	a0, s0, 0x800024d0 <_ftoa+0x6e0>
800024f4: 6f 00 80 00  	j	0x800024fc <_ftoa+0x70c>
800024f8: 13 8a 0a 00  	mv	s4, s5
; }
800024fc: 13 05 0a 00  	mv	a0, s4
80002500: 83 20 c1 05  	lw	ra, 92(sp)
80002504: 03 24 81 05  	lw	s0, 88(sp)
80002508: 83 24 41 05  	lw	s1, 84(sp)
8000250c: 03 29 01 05  	lw	s2, 80(sp)
80002510: 83 29 c1 04  	lw	s3, 76(sp)
80002514: 03 2a 81 04  	lw	s4, 72(sp)
80002518: 83 2a 41 04  	lw	s5, 68(sp)
8000251c: 03 2b 01 04  	lw	s6, 64(sp)
80002520: 83 2b c1 03  	lw	s7, 60(sp)
80002524: 03 2c 81 03  	lw	s8, 56(sp)
80002528: 83 2c 41 03  	lw	s9, 52(sp)
8000252c: 03 2d 01 03  	lw	s10, 48(sp)
80002530: 07 34 81 02  	fld	fs0, 40(sp)
80002534: 13 01 01 06  	addi	sp, sp, 96
80002538: 67 80 00 00  	ret
8000253c: 00 00        	<unknown>
8000253e: 00 00        	<unknown>

80002540 <_etoa>:
; {
80002540: 13 01 01 f9  	addi	sp, sp, -112
80002544: 23 26 11 06  	sw	ra, 108(sp)
80002548: 23 24 81 06  	sw	s0, 104(sp)
8000254c: 23 22 91 06  	sw	s1, 100(sp)
80002550: 23 20 21 07  	sw	s2, 96(sp)
80002554: 23 2e 31 05  	sw	s3, 92(sp)
80002558: 23 2c 41 05  	sw	s4, 88(sp)
8000255c: 23 2a 51 05  	sw	s5, 84(sp)
80002560: 23 28 61 05  	sw	s6, 80(sp)
80002564: 23 26 71 05  	sw	s7, 76(sp)
80002568: 23 24 81 05  	sw	s8, 72(sp)
8000256c: 23 22 91 05  	sw	s9, 68(sp)
80002570: 23 20 a1 05  	sw	s10, 64(sp)
80002574: 23 2e b1 03  	sw	s11, 60(sp)
80002578: 93 0a 08 00  	mv	s5, a6
8000257c: 13 84 07 00  	mv	s0, a5
80002580: 93 07 07 00  	mv	a5, a4
80002584: 93 84 06 00  	mv	s1, a3
80002588: 13 0a 06 00  	mv	s4, a2
8000258c: 13 89 05 00  	mv	s2, a1
80002590: 93 09 05 00  	mv	s3, a0
80002594: 37 35 00 80  	lui	a0, 524291
80002598: 07 30 85 60  	fld	ft0, 1544(a0)
8000259c: 37 35 00 80  	lui	a0, 524291
800025a0: 87 30 05 61  	fld	ft1, 1552(a0)
800025a4: 53 15 a0 a2  	flt.d	a0, ft0, fa0
800025a8: d3 15 15 a2  	flt.d	a1, fa0, ft1
;   if ((value != value) || (value > DBL_MAX) || (value < -DBL_MAX)) {
800025ac: 33 65 b5 00  	or	a0, a0, a1
800025b0: 63 00 05 06  	beqz	a0, 0x80002610 <_etoa+0xd0>
;     return _ftoa(out, buffer, idx, maxlen, value, prec, width, flags);
800025b4: 13 85 09 00  	mv	a0, s3
800025b8: 93 05 09 00  	mv	a1, s2
800025bc: 13 06 0a 00  	mv	a2, s4
800025c0: 93 86 04 00  	mv	a3, s1
800025c4: 13 87 07 00  	mv	a4, a5
800025c8: 93 07 04 00  	mv	a5, s0
800025cc: 13 88 0a 00  	mv	a6, s5
800025d0: 83 20 c1 06  	lw	ra, 108(sp)
800025d4: 03 24 81 06  	lw	s0, 104(sp)
800025d8: 83 24 41 06  	lw	s1, 100(sp)
800025dc: 03 29 01 06  	lw	s2, 96(sp)
800025e0: 83 29 c1 05  	lw	s3, 92(sp)
800025e4: 03 2a 81 05  	lw	s4, 88(sp)
800025e8: 83 2a 41 05  	lw	s5, 84(sp)
800025ec: 03 2b 01 05  	lw	s6, 80(sp)
800025f0: 83 2b c1 04  	lw	s7, 76(sp)
800025f4: 03 2c 81 04  	lw	s8, 72(sp)
800025f8: 83 2c 41 04  	lw	s9, 68(sp)
800025fc: 03 2d 01 04  	lw	s10, 64(sp)
80002600: 83 2d c1 03  	lw	s11, 60(sp)
80002604: 13 01 01 07  	addi	sp, sp, 112
80002608: 17 f3 ff ff  	auipc	t1, 1048575
8000260c: 67 00 83 7e  	jr	2024(t1)
80002610: 53 00 00 d2  	fcvt.d.w	ft0, zero
;   const bool negative = value < 0;
80002614: 53 15 05 a2  	flt.d	a0, fa0, ft0
;   if (negative) {
80002618: 63 04 05 00  	beqz	a0, 0x80002620 <_etoa+0xe0>
8000261c: 53 15 a5 22  	fneg.d	fa0, fa0
;   if (!(flags & FLAGS_PRECISION)) {
80002620: 93 f5 0a 40  	andi	a1, s5, 1024
80002624: 13 07 60 00  	li	a4, 6
80002628: 63 84 05 00  	beqz	a1, 0x80002630 <_etoa+0xf0>
8000262c: 13 87 07 00  	mv	a4, a5
;   conv.F = value;
80002630: 27 38 a1 00  	fsd	fa0, 16(sp)
80002634: 03 26 01 01  	lw	a2, 16(sp)
80002638: 83 26 41 01  	lw	a3, 20(sp)
8000263c: b7 07 f0 3f  	lui	a5, 261888
80002640: 37 38 00 80  	lui	a6, 524291
;   conv.U = (conv.U & ((1ULL << 52U) - 1U)) | (1023ULL << 52U);  // drop the exponent so conv.F is now in [1,2)
80002644: 23 24 c1 00  	sw	a2, 8(sp)
;   int exp2 = (int)((conv.U >> 52U) & 0x07FFU) - 1023;           // effectively log2
80002648: 13 d6 46 01  	srli	a2, a3, 20
8000264c: 13 76 f6 7f  	andi	a2, a2, 2047
80002650: 13 06 16 c0  	addi	a2, a2, -1023
;   conv.U = (conv.U & ((1ULL << 52U) - 1U)) | (1023ULL << 52U);  // drop the exponent so conv.F is now in [1,2)
80002654: 93 96 c6 00  	slli	a3, a3, 12
80002658: 93 d6 c6 00  	srli	a3, a3, 12
8000265c: b3 e6 f6 00  	or	a3, a3, a5
80002660: 23 26 d1 00  	sw	a3, 12(sp)
80002664: 07 30 81 00  	fld	ft0, 8(sp)
;   int expval = (int)(0.1760912590558 + exp2 * 0.301029995663981 + (conv.F - 1.5) * 0.289529654602168);
80002668: d3 00 06 d2  	fcvt.d.w	ft1, a2
8000266c: 07 31 88 61  	fld	ft2, 1560(a6)
80002670: 37 36 00 80  	lui	a2, 524291
80002674: 87 31 06 62  	fld	ft3, 1568(a2)
80002678: 37 36 00 80  	lui	a2, 524291
8000267c: 07 32 86 62  	fld	ft4, 1576(a2)
80002680: 37 36 00 80  	lui	a2, 524291
;   int expval = (int)(0.1760912590558 + exp2 * 0.301029995663981 + (conv.F - 1.5) * 0.289529654602168);
80002684: 43 70 30 12  	fmadd.d	ft0, ft0, ft3, ft2
80002688: 07 31 06 63  	fld	ft2, 1584(a2)
8000268c: 43 f0 40 02  	fmadd.d	ft0, ft1, ft4, ft0
80002690: 53 16 00 c2  	fcvt.w.d	a2, ft0, rtz
;   exp2 = (int)(expval * 3.321928094887362 + 0.5);
80002694: 53 00 06 d2  	fcvt.d.w	ft0, a2
80002698: b7 36 00 80  	lui	a3, 524291
8000269c: 87 b0 86 63  	fld	ft1, 1592(a3)
800026a0: b7 36 00 80  	lui	a3, 524291
800026a4: 87 b1 06 64  	fld	ft3, 1600(a3)
800026a8: b7 36 00 80  	lui	a3, 524291
800026ac: c3 70 10 12  	fmadd.d	ft1, ft0, ft1, ft2
800026b0: d3 97 00 c2  	fcvt.w.d	a5, ft1, rtz
;   const double z  = expval * 2.302585092994046 - exp2 * 0.6931471805599453;
800026b4: d3 80 07 d2  	fcvt.d.w	ft1, a5
800026b8: d3 f0 30 12  	fmul.d	ft1, ft1, ft3
800026bc: 07 b1 86 64  	fld	ft2, 1608(a3)
;   conv.U = (uint64_t)(exp2 + 1023) << 52U;
800026c0: 93 86 f7 3f  	addi	a3, a5, 1023
800026c4: 23 20 01 00  	sw	zero, 0(sp)
800026c8: 93 96 46 01  	slli	a3, a3, 20
;   const double z  = expval * 2.302585092994046 - exp2 * 0.6931471805599453;
800026cc: 43 70 20 0a  	fmadd.d	ft0, ft0, ft2, ft1
;   const double z2 = z * z;
800026d0: d3 70 00 12  	fmul.d	ft1, ft0, ft0
;   conv.U = (uint64_t)(exp2 + 1023) << 52U;
800026d4: 23 22 d1 00  	sw	a3, 4(sp)
800026d8: 07 31 01 00  	fld	ft2, 0(sp)
;   conv.F *= 1 + 2 * z / (2 - z + (z2 / (6 + (z2 / (10 + z2 / 14)))));
800026dc: d3 71 00 02  	fadd.d	ft3, ft0, ft0
800026e0: b7 36 00 80  	lui	a3, 524291
800026e4: 07 b2 06 65  	fld	ft4, 1616(a3)
800026e8: b7 36 00 80  	lui	a3, 524291
800026ec: 87 b2 86 65  	fld	ft5, 1624(a3)
800026f0: b7 36 00 80  	lui	a3, 524291
800026f4: 53 70 02 0a  	fsub.d	ft0, ft4, ft0
800026f8: 07 b2 06 66  	fld	ft4, 1632(a3)
800026fc: b7 36 00 80  	lui	a3, 524291
80002700: 07 b3 86 66  	fld	ft6, 1640(a3)
;   conv.F *= 1 + 2 * z / (2 - z + (z2 / (6 + (z2 / (10 + z2 / 14)))));
80002704: 43 f2 40 2a  	fmadd.d	ft4, ft1, ft4, ft5
80002708: 53 f2 40 1a  	fdiv.d	ft4, ft1, ft4
8000270c: 53 72 62 02  	fadd.d	ft4, ft4, ft6
80002710: d3 f0 40 1a  	fdiv.d	ft1, ft1, ft4
80002714: 53 70 10 02  	fadd.d	ft0, ft0, ft1
80002718: 53 f0 01 1a  	fdiv.d	ft0, ft3, ft0
8000271c: 43 70 20 12  	fmadd.d	ft0, ft0, ft2, ft2
;   if (value < conv.F) {
80002720: d3 16 05 a2  	flt.d	a3, fa0, ft0
80002724: 63 88 06 00  	beqz	a3, 0x80002734 <_etoa+0x1f4>
80002728: b7 37 00 80  	lui	a5, 524291
8000272c: 87 b0 07 67  	fld	ft1, 1648(a5)
80002730: 53 70 10 12  	fmul.d	ft0, ft0, ft1
;   if (value < conv.F) {
80002734: b3 0b d6 40  	sub	s7, a2, a3
;   unsigned int minwidth = ((expval < 100) && (expval > -100)) ? 4U : 5U;
80002738: 13 86 3b 06  	addi	a2, s7, 99
8000273c: 13 36 76 0c  	sltiu	a2, a2, 199
80002740: 93 06 50 00  	li	a3, 5
80002744: b3 8c c6 40  	sub	s9, a3, a2
80002748: 37 16 00 00  	lui	a2, 1
8000274c: 13 06 06 80  	addi	a2, a2, -2048
;   if (flags & FLAGS_ADAPT_EXP) {
80002750: 33 f6 ca 00  	and	a2, s5, a2
80002754: 63 00 06 06  	beqz	a2, 0x800027b4 <_etoa+0x274>
80002758: 37 36 00 80  	lui	a2, 524291
8000275c: 87 30 86 67  	fld	ft1, 1656(a2)
80002760: 37 36 00 80  	lui	a2, 524291
80002764: 07 31 06 68  	fld	ft2, 1664(a2)
;     if ((value >= 1e-4) && (value < 1e6)) {
80002768: 53 86 a0 a2  	fle.d	a2, ft1, fa0
8000276c: d3 16 25 a2  	flt.d	a3, fa0, ft2
80002770: 33 76 d6 00  	and	a2, a2, a3
80002774: 63 08 06 00  	beqz	a2, 0x80002784 <_etoa+0x244>
;       if ((int)prec > expval) {
80002778: 63 c4 eb 02  	blt	s7, a4, 0x800027a0 <_etoa+0x260>
8000277c: 13 07 00 00  	li	a4, 0
80002780: 6f 00 80 02  	j	0x800027a8 <_etoa+0x268>
;       if ((prec > 0) && (flags & FLAGS_PRECISION)) {
80002784: 63 02 07 28  	beqz	a4, 0x80002a08 <_etoa+0x4c8>
;   if (!(flags & FLAGS_PRECISION)) {
80002788: b3 35 b0 00  	snez	a1, a1
;       if ((prec > 0) && (flags & FLAGS_PRECISION)) {
8000278c: 33 07 b7 40  	sub	a4, a4, a1
80002790: 93 05 00 00  	li	a1, 0
;   if (width > minwidth) {
80002794: 33 06 94 41  	sub	a2, s0, s9
80002798: 63 74 c4 02  	bgeu	s0, a2, 0x800027c0 <_etoa+0x280>
8000279c: 6f 00 80 02  	j	0x800027c4 <_etoa+0x284>
800027a0: 93 c5 fb ff  	not	a1, s7
800027a4: 33 07 b7 00  	add	a4, a4, a1
800027a8: 93 0b 00 00  	li	s7, 0
800027ac: 93 0c 00 00  	li	s9, 0
;       flags |= FLAGS_PRECISION;   // make sure _ftoa respects precision
800027b0: 93 ea 0a 40  	ori	s5, s5, 1024
800027b4: 93 05 00 00  	li	a1, 0
;   if (width > minwidth) {
800027b8: 33 06 94 41  	sub	a2, s0, s9
800027bc: 63 64 c4 00  	bltu	s0, a2, 0x800027c4 <_etoa+0x284>
800027c0: 93 05 06 00  	mv	a1, a2
800027c4: 93 07 00 00  	li	a5, 0
;   if ((flags & FLAGS_LEFT) && minwidth) {
800027c8: 13 fc 2a 00  	andi	s8, s5, 2
800027cc: 13 56 1c 00  	srli	a2, s8, 1
800027d0: b3 36 90 01  	snez	a3, s9
800027d4: 33 76 d6 00  	and	a2, a2, a3
800027d8: 63 08 06 1c  	beqz	a2, 0x800029a8 <_etoa+0x468>
;   if (expval) {
800027dc: 63 8a 0b 1c  	beqz	s7, 0x800029b0 <_etoa+0x470>
800027e0: 53 75 05 1a  	fdiv.d	fa0, fa0, ft0
;   idx = _ftoa(out, buffer, idx, maxlen, negative ? -value : value, prec, fwidth, flags & ~FLAGS_ADAPT_EXP);
800027e4: 63 04 05 00  	beqz	a0, 0x800027ec <_etoa+0x2ac>
800027e8: 53 15 a5 22  	fneg.d	fa0, fa0
800027ec: 37 f5 ff ff  	lui	a0, 1048575
800027f0: 13 05 f5 7f  	addi	a0, a0, 2047
;   idx = _ftoa(out, buffer, idx, maxlen, negative ? -value : value, prec, fwidth, flags & ~FLAGS_ADAPT_EXP);
800027f4: 33 f8 aa 00  	and	a6, s5, a0
800027f8: 13 85 09 00  	mv	a0, s3
800027fc: 93 05 09 00  	mv	a1, s2
80002800: 13 06 0a 00  	mv	a2, s4
80002804: 93 86 04 00  	mv	a3, s1
80002808: 97 f0 ff ff  	auipc	ra, 1048575
8000280c: e7 80 80 5e  	jalr	1512(ra)
80002810: 13 0b 05 00  	mv	s6, a0
;   if (minwidth) {
80002814: 63 8a 0c 1a  	beqz	s9, 0x800029c8 <_etoa+0x488>
;     out((flags & FLAGS_UPPERCASE) ? 'E' : 'e', buffer, idx++, maxlen);
80002818: 13 f5 0a 02  	andi	a0, s5, 32
8000281c: 13 45 55 06  	xori	a0, a0, 101
80002820: 93 05 09 00  	mv	a1, s2
80002824: 13 06 0b 00  	mv	a2, s6
80002828: 93 86 04 00  	mv	a3, s1
8000282c: e7 80 09 00  	jalr	s3
80002830: 13 0d 00 00  	li	s10, 0
;     idx = _ntoa_long(out, buffer, idx, maxlen, (expval < 0) ? -expval : expval, expval < 0, 10, 0, minwidth-1, FLAGS_ZEROPAD | FLAGS_PLUS);
80002834: 13 d5 fb 41  	srai	a0, s7, 31
80002838: b3 85 ab 00  	add	a1, s7, a0
8000283c: 33 c7 a5 00  	xor	a4, a1, a0
;     do {
80002840: 93 8a dc ff  	addi	s5, s9, -3
80002844: 93 0d f0 01  	li	s11, 31
80002848: 37 d5 cc cc  	lui	a0, 838861
8000284c: 13 05 d5 cc  	addi	a0, a0, -819
80002850: 93 05 a0 00  	li	a1, 10
80002854: 13 06 c1 01  	addi	a2, sp, 28
80002858: 93 06 90 00  	li	a3, 9
8000285c: 13 00 00 00  	nop
80002860: 93 07 0d 00  	mv	a5, s10
;       value /= base;
80002864: 33 38 a7 02  	mulhu	a6, a4, a0
80002868: 13 58 38 00  	srli	a6, a6, 3
8000286c: b3 08 b8 02  	mul	a7, a6, a1
80002870: b3 08 17 41  	sub	a7, a4, a7
;       buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
80002874: 93 e8 08 03  	ori	a7, a7, 48
80002878: 13 0d 1d 00  	addi	s10, s10, 1
8000287c: b3 02 f6 00  	add	t0, a2, a5
80002880: 23 80 12 01  	sb	a7, 0(t0)
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
80002884: 33 b7 e6 00  	sltu	a4, a3, a4
80002888: 93 b8 f7 01  	sltiu	a7, a5, 31
8000288c: b3 78 17 01  	and	a7, a4, a7
80002890: 93 8a fa ff  	addi	s5, s5, -1
80002894: 93 8d fd ff  	addi	s11, s11, -1
80002898: 13 07 08 00  	mv	a4, a6
8000289c: e3 92 08 fc  	bnez	a7, 0x80002860 <_etoa+0x320>
800028a0: 13 05 e0 01  	li	a0, 30
;     } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
800028a4: 33 35 f5 00  	sltu	a0, a0, a5
;     if (width && (flags & FLAGS_ZEROPAD) && (negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) {
800028a8: 93 85 ec ff  	addi	a1, s9, -2
;     while ((flags & FLAGS_ZEROPAD) && (len < width) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
800028ac: b3 35 bd 00  	sltu	a1, s10, a1
800028b0: 93 c5 15 00  	xori	a1, a1, 1
800028b4: 33 65 b5 00  	or	a0, a0, a1
800028b8: 63 12 05 04  	bnez	a0, 0x800028fc <_etoa+0x3bc>
800028bc: 13 05 c1 01  	addi	a0, sp, 28
;     while ((flags & FLAGS_ZEROPAD) && (len < width) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
800028c0: 33 05 a5 01  	add	a0, a0, s10
800028c4: b3 85 ac 41  	sub	a1, s9, s10
800028c8: 93 85 d5 ff  	addi	a1, a1, -3
800028cc: 13 06 f0 01  	li	a2, 31
800028d0: 33 06 a6 41  	sub	a2, a2, s10
800028d4: 63 e4 c5 00  	bltu	a1, a2, 0x800028dc <_etoa+0x39c>
800028d8: 93 05 06 00  	mv	a1, a2
;     while ((flags & FLAGS_ZEROPAD) && (len < width) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
800028dc: 13 86 15 00  	addi	a2, a1, 1
;       buf[len++] = '0';
800028e0: 93 05 00 03  	li	a1, 48
800028e4: 97 e0 ff ff  	auipc	ra, 1048574
800028e8: e7 80 40 f8  	jalr	-124(ra)
;   if (flags & FLAGS_HASH) {
800028ec: 63 e4 ba 01  	bltu	s5, s11, 0x800028f4 <_etoa+0x3b4>
800028f0: 93 8a 0d 00  	mv	s5, s11
;   if (flags & FLAGS_HASH) {
800028f4: 33 85 aa 01  	add	a0, s5, s10
800028f8: 13 0d 15 00  	addi	s10, a0, 1
800028fc: 13 05 f0 01  	li	a0, 31
;   if (len < PRINTF_NTOA_BUFFER_SIZE) {
80002900: 63 60 a5 03  	bltu	a0, s10, 0x80002920 <_etoa+0x3e0>
80002904: 13 05 d0 02  	li	a0, 45
;     if (negative) {
80002908: 63 c4 0b 00  	bltz	s7, 0x80002910 <_etoa+0x3d0>
8000290c: 13 05 b0 02  	li	a0, 43
80002910: 93 05 c1 01  	addi	a1, sp, 28
80002914: b3 85 a5 01  	add	a1, a1, s10
80002918: 13 0d 1d 00  	addi	s10, s10, 1
8000291c: 23 80 a5 00  	sb	a0, 0(a1)
80002920: 93 0a 1b 00  	addi	s5, s6, 1
;   while (len) {
80002924: 13 0b b1 01  	addi	s6, sp, 27
80002928: 13 00 00 00  	nop
8000292c: 13 00 00 00  	nop
80002930: 13 86 0a 00  	mv	a2, s5
;     out(buf[--len], buffer, idx++, maxlen);
80002934: 93 0b fd ff  	addi	s7, s10, -1
80002938: 33 05 ab 01  	add	a0, s6, s10
8000293c: 03 45 05 00  	lbu	a0, 0(a0)
80002940: 93 8a 1a 00  	addi	s5, s5, 1
80002944: 93 05 09 00  	mv	a1, s2
80002948: 93 86 04 00  	mv	a3, s1
8000294c: e7 80 09 00  	jalr	s3
80002950: 13 8d 0b 00  	mv	s10, s7
;   while (len) {
80002954: e3 9e 0b fc  	bnez	s7, 0x80002930 <_etoa+0x3f0>
;   if ((flags & FLAGS_LEFT) && minwidth) {
80002958: 13 35 1c 00  	seqz	a0, s8
8000295c: b3 85 4a 41  	sub	a1, s5, s4
80002960: b3 b5 85 00  	sltu	a1, a1, s0
80002964: 93 c5 15 00  	xori	a1, a1, 1
;     if (flags & FLAGS_LEFT) {
80002968: 33 65 b5 00  	or	a0, a0, a1
8000296c: 63 1c 05 04  	bnez	a0, 0x800029c4 <_etoa+0x484>
;       while (idx - start_idx < width) out(' ', buffer, idx++, maxlen);
80002970: 33 0a 40 41  	neg	s4, s4
80002974: 13 00 00 00  	nop
80002978: 13 00 00 00  	nop
8000297c: 13 00 00 00  	nop
80002980: 13 8b 1a 00  	addi	s6, s5, 1
80002984: 13 05 00 02  	li	a0, 32
80002988: 93 05 09 00  	mv	a1, s2
8000298c: 13 86 0a 00  	mv	a2, s5
80002990: 93 86 04 00  	mv	a3, s1
80002994: e7 80 09 00  	jalr	s3
80002998: 33 05 6a 01  	add	a0, s4, s6
8000299c: 93 0a 0b 00  	mv	s5, s6
800029a0: e3 60 85 fe  	bltu	a0, s0, 0x80002980 <_etoa+0x440>
800029a4: 6f 00 40 02  	j	0x800029c8 <_etoa+0x488>
800029a8: 93 87 05 00  	mv	a5, a1
;   if (expval) {
800029ac: e3 9a 0b e2  	bnez	s7, 0x800027e0 <_etoa+0x2a0>
800029b0: b7 35 00 80  	lui	a1, 524291
800029b4: 07 b0 85 68  	fld	ft0, 1672(a1)
;   if (expval) {
800029b8: 53 75 05 1a  	fdiv.d	fa0, fa0, ft0
;   idx = _ftoa(out, buffer, idx, maxlen, negative ? -value : value, prec, fwidth, flags & ~FLAGS_ADAPT_EXP);
800029bc: e3 16 05 e2  	bnez	a0, 0x800027e8 <_etoa+0x2a8>
800029c0: 6f f0 df e2  	j	0x800027ec <_etoa+0x2ac>
800029c4: 13 8b 0a 00  	mv	s6, s5
; }
800029c8: 13 05 0b 00  	mv	a0, s6
800029cc: 83 20 c1 06  	lw	ra, 108(sp)
800029d0: 03 24 81 06  	lw	s0, 104(sp)
800029d4: 83 24 41 06  	lw	s1, 100(sp)
800029d8: 03 29 01 06  	lw	s2, 96(sp)
800029dc: 83 29 c1 05  	lw	s3, 92(sp)
800029e0: 03 2a 81 05  	lw	s4, 88(sp)
800029e4: 83 2a 41 05  	lw	s5, 84(sp)
800029e8: 03 2b 01 05  	lw	s6, 80(sp)
800029ec: 83 2b c1 04  	lw	s7, 76(sp)
800029f0: 03 2c 81 04  	lw	s8, 72(sp)
800029f4: 83 2c 41 04  	lw	s9, 68(sp)
800029f8: 03 2d 01 04  	lw	s10, 64(sp)
800029fc: 83 2d c1 03  	lw	s11, 60(sp)
80002a00: 13 01 01 07  	addi	sp, sp, 112
80002a04: 67 80 00 00  	ret
80002a08: 13 07 00 00  	li	a4, 0
80002a0c: 93 05 00 00  	li	a1, 0
;   if (width > minwidth) {
80002a10: 33 06 94 41  	sub	a2, s0, s9
80002a14: e3 76 c4 da  	bgeu	s0, a2, 0x800027c0 <_etoa+0x280>
80002a18: 6f f0 df da  	j	0x800027c4 <_etoa+0x284>
80002a1c: 00 00        	<unknown>
80002a1e: 00 00        	<unknown>

80002a20 <_ntoa_format>:
; {
80002a20: 13 01 01 fc  	addi	sp, sp, -64
80002a24: 23 2e 11 02  	sw	ra, 60(sp)
80002a28: 23 2c 81 02  	sw	s0, 56(sp)
80002a2c: 23 2a 91 02  	sw	s1, 52(sp)
80002a30: 23 28 21 03  	sw	s2, 48(sp)
80002a34: 23 26 31 03  	sw	s3, 44(sp)
80002a38: 23 24 41 03  	sw	s4, 40(sp)
80002a3c: 23 22 51 03  	sw	s5, 36(sp)
80002a40: 23 20 61 03  	sw	s6, 32(sp)
80002a44: 23 2e 71 01  	sw	s7, 28(sp)
80002a48: 23 2c 81 01  	sw	s8, 24(sp)
80002a4c: 23 2a 91 01  	sw	s9, 20(sp)
80002a50: 23 28 a1 01  	sw	s10, 16(sp)
80002a54: 23 26 b1 01  	sw	s11, 12(sp)
80002a58: 93 8b 0e 00  	mv	s7, t4
80002a5c: 13 04 0e 00  	mv	s0, t3
80002a60: 13 8d 03 00  	mv	s10, t2
80002a64: 93 0c 08 00  	mv	s9, a6
80002a68: 13 8b 07 00  	mv	s6, a5
80002a6c: 93 0a 07 00  	mv	s5, a4
80002a70: 93 84 06 00  	mv	s1, a3
80002a74: 13 0a 06 00  	mv	s4, a2
80002a78: 13 89 05 00  	mv	s2, a1
80002a7c: 93 09 05 00  	mv	s3, a0
;   if (!(flags & FLAGS_LEFT)) {
80002a80: 93 fd 2e 00  	andi	s11, t4, 2
80002a84: 63 94 0d 0e  	bnez	s11, 0x80002b6c <_ntoa_format+0x14c>
80002a88: 13 fc 1b 00  	andi	s8, s7, 1
;     if (width && (flags & FLAGS_ZEROPAD) && (negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) {
80002a8c: 63 0e 04 00  	beqz	s0, 0x80002aa8 <_ntoa_format+0x88>
80002a90: 63 0e 0c 00  	beqz	s8, 0x80002aac <_ntoa_format+0x8c>
80002a94: 13 f5 cb 00  	andi	a0, s7, 12
80002a98: 33 35 a0 00  	snez	a0, a0
;     if (width && (flags & FLAGS_ZEROPAD) && (negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) {
80002a9c: 33 65 95 01  	or	a0, a0, s9
80002aa0: 33 04 a4 40  	sub	s0, s0, a0
80002aa4: 6f 00 80 00  	j	0x80002aac <_ntoa_format+0x8c>
80002aa8: 13 04 00 00  	li	s0, 0
;     while ((len < prec) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
80002aac: 33 35 ab 01  	sltu	a0, s6, s10
80002ab0: 13 45 15 00  	xori	a0, a0, 1
80002ab4: 93 05 f0 01  	li	a1, 31
80002ab8: 33 b6 65 01  	sltu	a2, a1, s6
80002abc: 33 65 c5 00  	or	a0, a0, a2
80002ac0: 63 1c 05 04  	bnez	a0, 0x80002b18 <_ntoa_format+0xf8>
80002ac4: 23 24 11 01  	sw	a7, 8(sp)
80002ac8: 23 22 91 01  	sw	s9, 4(sp)
80002acc: 13 85 0a 00  	mv	a0, s5
80002ad0: 93 8a 0d 00  	mv	s5, s11
80002ad4: 93 0c 05 00  	mv	s9, a0
;     while ((len < prec) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
80002ad8: 33 05 65 01  	add	a0, a0, s6
80002adc: 13 46 fb ff  	not	a2, s6
80002ae0: b3 0d a6 01  	add	s11, a2, s10
80002ae4: b3 85 65 41  	sub	a1, a1, s6
80002ae8: 63 e4 bd 00  	bltu	s11, a1, 0x80002af0 <_ntoa_format+0xd0>
80002aec: 93 8d 05 00  	mv	s11, a1
;     while ((len < prec) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
80002af0: 13 86 1d 00  	addi	a2, s11, 1
;       buf[len++] = '0';
80002af4: 93 05 00 03  	li	a1, 48
80002af8: 97 e0 ff ff  	auipc	ra, 1048574
80002afc: e7 80 00 d7  	jalr	-656(ra)
;     while ((len < prec) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
80002b00: 33 85 6d 01  	add	a0, s11, s6
80002b04: 13 0b 15 00  	addi	s6, a0, 1
80002b08: 93 8d 0a 00  	mv	s11, s5
80002b0c: 93 8a 0c 00  	mv	s5, s9
80002b10: 83 2c 41 00  	lw	s9, 4(sp)
80002b14: 83 28 81 00  	lw	a7, 8(sp)
;     while ((flags & FLAGS_ZEROPAD) && (len < width) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
80002b18: 63 0a 0c 04  	beqz	s8, 0x80002b6c <_ntoa_format+0x14c>
80002b1c: 33 35 8b 00  	sltu	a0, s6, s0
80002b20: 13 45 15 00  	xori	a0, a0, 1
80002b24: 93 05 f0 01  	li	a1, 31
80002b28: 33 b6 65 01  	sltu	a2, a1, s6
80002b2c: 33 65 c5 00  	or	a0, a0, a2
80002b30: 63 1e 05 02  	bnez	a0, 0x80002b6c <_ntoa_format+0x14c>
80002b34: 23 24 11 01  	sw	a7, 8(sp)
;     while ((flags & FLAGS_ZEROPAD) && (len < width) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
80002b38: 33 85 6a 01  	add	a0, s5, s6
80002b3c: 13 46 fb ff  	not	a2, s6
80002b40: 33 0c c4 00  	add	s8, s0, a2
80002b44: b3 85 65 41  	sub	a1, a1, s6
80002b48: 63 64 bc 00  	bltu	s8, a1, 0x80002b50 <_ntoa_format+0x130>
80002b4c: 13 8c 05 00  	mv	s8, a1
;     while ((flags & FLAGS_ZEROPAD) && (len < width) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
80002b50: 13 06 1c 00  	addi	a2, s8, 1
;       buf[len++] = '0';
80002b54: 93 05 00 03  	li	a1, 48
80002b58: 97 e0 ff ff  	auipc	ra, 1048574
80002b5c: e7 80 00 d1  	jalr	-752(ra)
;     while ((flags & FLAGS_ZEROPAD) && (len < width) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
80002b60: 33 05 8b 01  	add	a0, s6, s8
80002b64: 13 0b 15 00  	addi	s6, a0, 1
80002b68: 83 28 81 00  	lw	a7, 8(sp)
;   if (flags & FLAGS_HASH) {
80002b6c: 13 f5 0b 01  	andi	a0, s7, 16
80002b70: 63 00 05 0c  	beqz	a0, 0x80002c30 <_ntoa_format+0x210>
;     if (!(flags & FLAGS_PRECISION) && len && ((len == prec) || (len == width))) {
80002b74: 13 f5 0b 40  	andi	a0, s7, 1024
80002b78: 13 35 15 00  	seqz	a0, a0
80002b7c: b3 35 60 01  	snez	a1, s6
80002b80: 33 75 b5 00  	and	a0, a0, a1
80002b84: 63 0e 05 02  	beqz	a0, 0x80002bc0 <_ntoa_format+0x1a0>
80002b88: 33 45 ab 01  	xor	a0, s6, s10
80002b8c: 33 35 a0 00  	snez	a0, a0
80002b90: b3 45 8b 00  	xor	a1, s6, s0
80002b94: b3 35 b0 00  	snez	a1, a1
80002b98: 33 75 b5 00  	and	a0, a0, a1
80002b9c: 63 12 05 02  	bnez	a0, 0x80002bc0 <_ntoa_format+0x1a0>
;       len--;
80002ba0: 13 05 fb ff  	addi	a0, s6, -1
;       if (len && (base == 16U)) {
80002ba4: b3 35 a0 00  	snez	a1, a0
80002ba8: 13 86 08 ff  	addi	a2, a7, -16
80002bac: 13 36 16 00  	seqz	a2, a2
80002bb0: b3 75 b6 00  	and	a1, a2, a1
80002bb4: 63 84 05 00  	beqz	a1, 0x80002bbc <_ntoa_format+0x19c>
80002bb8: 13 05 eb ff  	addi	a0, s6, -2
80002bbc: 13 0b 05 00  	mv	s6, a0
80002bc0: 13 05 00 01  	li	a0, 16
;     if ((base == 16U) && !(flags & FLAGS_UPPERCASE) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
80002bc4: 63 98 a8 02  	bne	a7, a0, 0x80002bf4 <_ntoa_format+0x1d4>
80002bc8: 93 f5 0b 02  	andi	a1, s7, 32
80002bcc: 13 b5 15 00  	seqz	a0, a1
80002bd0: 13 36 0b 02  	sltiu	a2, s6, 32
80002bd4: b3 76 c5 00  	and	a3, a0, a2
80002bd8: 13 05 80 07  	li	a0, 120
80002bdc: 63 98 06 02  	bnez	a3, 0x80002c0c <_ntoa_format+0x1ec>
;     else if ((base == 16U) && (flags & FLAGS_UPPERCASE) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
80002be0: 33 35 b0 00  	snez	a0, a1
80002be4: b3 75 c5 00  	and	a1, a0, a2
80002be8: 13 05 80 05  	li	a0, 88
80002bec: 63 90 05 02  	bnez	a1, 0x80002c0c <_ntoa_format+0x1ec>
80002bf0: 6f 00 80 02  	j	0x80002c18 <_ntoa_format+0x1f8>
;     else if ((base == 2U) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
80002bf4: 13 85 e8 ff  	addi	a0, a7, -2
80002bf8: 13 35 15 00  	seqz	a0, a0
80002bfc: 93 35 0b 02  	sltiu	a1, s6, 32
80002c00: b3 75 b5 00  	and	a1, a0, a1
80002c04: 13 05 20 06  	li	a0, 98
80002c08: 63 88 05 00  	beqz	a1, 0x80002c18 <_ntoa_format+0x1f8>
80002c0c: b3 85 6a 01  	add	a1, s5, s6
80002c10: 13 0b 1b 00  	addi	s6, s6, 1
80002c14: 23 80 a5 00  	sb	a0, 0(a1)
80002c18: 13 05 f0 01  	li	a0, 31
;     if (len < PRINTF_NTOA_BUFFER_SIZE) {
80002c1c: 63 64 65 05  	bltu	a0, s6, 0x80002c64 <_ntoa_format+0x244>
;       buf[len++] = '0';
80002c20: 33 85 6a 01  	add	a0, s5, s6
80002c24: 13 0b 1b 00  	addi	s6, s6, 1
80002c28: 93 05 00 03  	li	a1, 48
;       buf[len++] = '0';
80002c2c: 23 00 b5 00  	sb	a1, 0(a0)
80002c30: 13 05 f0 01  	li	a0, 31
;   if (len < PRINTF_NTOA_BUFFER_SIZE) {
80002c34: 63 68 65 03  	bltu	a0, s6, 0x80002c64 <_ntoa_format+0x244>
80002c38: 13 05 d0 02  	li	a0, 45
;     if (negative) {
80002c3c: 63 9e 0c 00  	bnez	s9, 0x80002c58 <_ntoa_format+0x238>
;     else if (flags & FLAGS_PLUS) {
80002c40: 93 f5 4b 00  	andi	a1, s7, 4
80002c44: 13 05 b0 02  	li	a0, 43
80002c48: 63 98 05 00  	bnez	a1, 0x80002c58 <_ntoa_format+0x238>
;     else if (flags & FLAGS_SPACE) {
80002c4c: 93 f5 8b 00  	andi	a1, s7, 8
80002c50: 13 05 00 02  	li	a0, 32
80002c54: 63 88 05 00  	beqz	a1, 0x80002c64 <_ntoa_format+0x244>
80002c58: b3 85 6a 01  	add	a1, s5, s6
80002c5c: 13 0b 1b 00  	addi	s6, s6, 1
80002c60: 23 80 a5 00  	sb	a0, 0(a1)
;   if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD)) {
80002c64: 13 f5 3b 00  	andi	a0, s7, 3
80002c68: 33 35 a0 00  	snez	a0, a0
80002c6c: b3 35 8b 00  	sltu	a1, s6, s0
80002c70: 93 c5 15 00  	xori	a1, a1, 1
80002c74: 33 65 b5 00  	or	a0, a0, a1
80002c78: 93 0b 0a 00  	mv	s7, s4
80002c7c: 63 1a 05 02  	bnez	a0, 0x80002cb0 <_ntoa_format+0x290>
;     for (size_t i = len; i < width; i++) {
80002c80: 33 0c 64 41  	sub	s8, s0, s6
80002c84: 13 06 0a 00  	mv	a2, s4
80002c88: 13 00 00 00  	nop
80002c8c: 13 00 00 00  	nop
;       out(' ', buffer, idx++, maxlen);
80002c90: 93 0b 16 00  	addi	s7, a2, 1
80002c94: 13 05 00 02  	li	a0, 32
80002c98: 93 05 09 00  	mv	a1, s2
80002c9c: 93 86 04 00  	mv	a3, s1
80002ca0: e7 80 09 00  	jalr	s3
;     for (size_t i = len; i < width; i++) {
80002ca4: 13 0c fc ff  	addi	s8, s8, -1
80002ca8: 13 86 0b 00  	mv	a2, s7
;     for (size_t i = len; i < width; i++) {
80002cac: e3 12 0c fe  	bnez	s8, 0x80002c90 <_ntoa_format+0x270>
;   while (len) {
80002cb0: 63 00 0b 04  	beqz	s6, 0x80002cf0 <_ntoa_format+0x2d0>
80002cb4: 13 8c fa ff  	addi	s8, s5, -1
80002cb8: 13 00 00 00  	nop
80002cbc: 13 00 00 00  	nop
;     out(buf[--len], buffer, idx++, maxlen);
80002cc0: 93 0c fb ff  	addi	s9, s6, -1
80002cc4: 33 05 6c 01  	add	a0, s8, s6
80002cc8: 03 45 05 00  	lbu	a0, 0(a0)
80002ccc: 93 8a 1b 00  	addi	s5, s7, 1
80002cd0: 93 05 09 00  	mv	a1, s2
80002cd4: 13 86 0b 00  	mv	a2, s7
80002cd8: 93 86 04 00  	mv	a3, s1
80002cdc: e7 80 09 00  	jalr	s3
80002ce0: 93 8b 0a 00  	mv	s7, s5
80002ce4: 13 8b 0c 00  	mv	s6, s9
;   while (len) {
80002ce8: e3 9c 0c fc  	bnez	s9, 0x80002cc0 <_ntoa_format+0x2a0>
80002cec: 6f 00 80 00  	j	0x80002cf4 <_ntoa_format+0x2d4>
80002cf0: 93 8a 0b 00  	mv	s5, s7
;   if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD)) {
80002cf4: 13 b5 1d 00  	seqz	a0, s11
80002cf8: b3 85 4a 41  	sub	a1, s5, s4
80002cfc: b3 b5 85 00  	sltu	a1, a1, s0
80002d00: 93 c5 15 00  	xori	a1, a1, 1
;   if (flags & FLAGS_LEFT) {
80002d04: 33 65 b5 00  	or	a0, a0, a1
80002d08: 63 18 05 02  	bnez	a0, 0x80002d38 <_ntoa_format+0x318>
;     while (idx - start_idx < width) {
80002d0c: 33 0b 40 41  	neg	s6, s4
;       out(' ', buffer, idx++, maxlen);
80002d10: 13 8a 1a 00  	addi	s4, s5, 1
80002d14: 13 05 00 02  	li	a0, 32
80002d18: 93 05 09 00  	mv	a1, s2
80002d1c: 13 86 0a 00  	mv	a2, s5
80002d20: 93 86 04 00  	mv	a3, s1
80002d24: e7 80 09 00  	jalr	s3
;     while (idx - start_idx < width) {
80002d28: 33 05 4b 01  	add	a0, s6, s4
80002d2c: 93 0a 0a 00  	mv	s5, s4
80002d30: e3 60 85 fe  	bltu	a0, s0, 0x80002d10 <_ntoa_format+0x2f0>
80002d34: 6f 00 80 00  	j	0x80002d3c <_ntoa_format+0x31c>
80002d38: 13 8a 0a 00  	mv	s4, s5
;   return _out_rev(out, buffer, idx, maxlen, buf, len, width, flags);
80002d3c: 13 05 0a 00  	mv	a0, s4
80002d40: 83 20 c1 03  	lw	ra, 60(sp)
80002d44: 03 24 81 03  	lw	s0, 56(sp)
80002d48: 83 24 41 03  	lw	s1, 52(sp)
80002d4c: 03 29 01 03  	lw	s2, 48(sp)
80002d50: 83 29 c1 02  	lw	s3, 44(sp)
80002d54: 03 2a 81 02  	lw	s4, 40(sp)
80002d58: 83 2a 41 02  	lw	s5, 36(sp)
80002d5c: 03 2b 01 02  	lw	s6, 32(sp)
80002d60: 83 2b c1 01  	lw	s7, 28(sp)
80002d64: 03 2c 81 01  	lw	s8, 24(sp)
80002d68: 83 2c 41 01  	lw	s9, 20(sp)
80002d6c: 03 2d 01 01  	lw	s10, 16(sp)
80002d70: 83 2d c1 00  	lw	s11, 12(sp)
80002d74: 13 01 01 04  	addi	sp, sp, 64
80002d78: 67 80 00 00  	ret
80002d7c: 00 00        	<unknown>
80002d7e: 00 00        	<unknown>

80002d80 <snrt_putchar>:
;     asm("csrr %0, mhartid" : "=r"(hartid));
80002d80: f3 25 40 f1  	csrr	a1, mhartid
80002d84: 13 06 80 44  	li	a2, 1096
;     buf->data[buf->hdr.size++] = character;
80002d88: 33 86 c5 02  	mul	a2, a1, a2
80002d8c: b7 d5 00 80  	lui	a1, 524301
80002d90: 93 86 45 69  	addi	a3, a1, 1684
80002d94: b3 05 d6 00  	add	a1, a2, a3
80002d98: 03 a7 05 00  	lw	a4, 0(a1)
80002d9c: 93 07 65 ff  	addi	a5, a0, -10
80002da0: b3 37 f0 00  	snez	a5, a5
80002da4: 13 08 17 00  	addi	a6, a4, 1
80002da8: 23 a0 05 01  	sw	a6, 0(a1)
80002dac: 33 87 e5 00  	add	a4, a1, a4
80002db0: 23 04 a7 04  	sb	a0, 72(a4)
;     if (buf->hdr.size == PUTC_BUFFER_LEN || character == '\n') {
80002db4: 03 a5 05 00  	lw	a0, 0(a1)
80002db8: 13 05 45 c0  	addi	a0, a0, -1020
80002dbc: 33 35 a0 00  	snez	a0, a0
80002dc0: 33 f5 a7 00  	and	a0, a5, a0
80002dc4: 63 10 05 06  	bnez	a0, 0x80002e24 <snrt_putchar+0xa4>
80002dc8: 33 86 c6 00  	add	a2, a3, a2
;     buf->data[buf->hdr.size++] = character;
80002dcc: 13 05 86 04  	addi	a0, a2, 72
;         buf->hdr.syscall_mem[0] = 64;  // sys_write
80002dd0: 93 06 86 00  	addi	a3, a2, 8
80002dd4: 23 26 06 00  	sw	zero, 12(a2)
80002dd8: 13 07 00 04  	li	a4, 64
80002ddc: 23 24 e6 00  	sw	a4, 8(a2)
;         buf->hdr.syscall_mem[1] = 1;   // file descriptor (1 = stdout)
80002de0: 23 2a 06 00  	sw	zero, 20(a2)
80002de4: 13 07 10 00  	li	a4, 1
80002de8: 23 28 e6 00  	sw	a4, 16(a2)
;         buf->hdr.syscall_mem[2] = (uintptr_t)&buf->data;  // buffer
80002dec: 23 2e 06 00  	sw	zero, 28(a2)
80002df0: 23 2c a6 00  	sw	a0, 24(a2)
;         buf->hdr.syscall_mem[3] = buf->hdr.size;          // length
80002df4: 03 a7 05 00  	lw	a4, 0(a1)
80002df8: 23 22 06 02  	sw	zero, 36(a2)
;         tohost = (uintptr_t)buf->hdr.syscall_mem;
80002dfc: b7 37 00 80  	lui	a5, 524291
80002e00: 37 35 00 80  	lui	a0, 524291
;         buf->hdr.syscall_mem[3] = buf->hdr.size;          // length
80002e04: 23 20 e6 02  	sw	a4, 32(a2)
;         tohost = (uintptr_t)buf->hdr.syscall_mem;
80002e08: 23 a0 d7 58  	sw	a3, 1408(a5)
80002e0c: 13 00 00 00  	nop
;         while (fromhost == 0)
80002e10: 03 26 05 5c  	lw	a2, 1472(a0)
80002e14: e3 0e 06 fe  	beqz	a2, 0x80002e10 <snrt_putchar+0x90>
;         fromhost = 0;
80002e18: 37 35 00 80  	lui	a0, 524291
80002e1c: 23 20 05 5c  	sw	zero, 1472(a0)
;         buf->hdr.size = 0;
80002e20: 23 a0 05 00  	sw	zero, 0(a1)
; }
80002e24: 67 80 00 00  	ret

80002e28 <snrt_current_team>:
;     return _snrt_team_current->root;
80002e28: 37 05 00 00  	lui	a0, 0
80002e2c: 33 05 45 00  	add	a0, a0, tp
80002e30: 03 25 05 00  	lw	a0, 0(a0)
80002e34: 03 25 05 00  	lw	a0, 0(a0)
80002e38: 67 80 00 00  	ret

80002e3c <snrt_global_core_idx>:
;     asm("csrr %0, mhartid" : "=r"(hartid));
80002e3c: 73 25 40 f1  	csrr	a0, mhartid
;     return snrt_hartid() - _snrt_team_current->root->global_core_base_hartid;
80002e40: b7 05 00 00  	lui	a1, 0
80002e44: b3 85 45 00  	add	a1, a1, tp
80002e48: 83 a5 05 00  	lw	a1, 0(a1)
80002e4c: 83 a5 05 00  	lw	a1, 0(a1)
80002e50: 83 a5 85 00  	lw	a1, 8(a1)
80002e54: 33 05 b5 40  	sub	a0, a0, a1
80002e58: 67 80 00 00  	ret

80002e5c <snrt_cluster_core_idx>:
; uint32_t snrt_cluster_core_idx() { return _snrt_core_idx; }
80002e5c: 37 05 00 00  	lui	a0, 0
80002e60: 33 05 45 00  	add	a0, a0, tp
80002e64: 03 25 45 00  	lw	a0, 4(a0)
80002e68: 67 80 00 00  	ret

80002e6c <snrt_cluster_core_num>:
;     return _snrt_team_current->root->cluster_core_num;
80002e6c: 37 05 00 00  	lui	a0, 0
80002e70: 33 05 45 00  	add	a0, a0, tp
80002e74: 03 25 05 00  	lw	a0, 0(a0)
80002e78: 03 25 05 00  	lw	a0, 0(a0)
80002e7c: 03 25 c5 01  	lw	a0, 28(a0)
80002e80: 67 80 00 00  	ret

80002e84 <_snrt_barrier_reg_ptr>:
;     return _snrt_team_current->root->barrier_reg_ptr;
80002e84: 37 05 00 00  	lui	a0, 0
80002e88: 33 05 45 00  	add	a0, a0, tp
80002e8c: 03 25 05 00  	lw	a0, 0(a0)
80002e90: 03 25 05 00  	lw	a0, 0(a0)
80002e94: 03 25 05 06  	lw	a0, 96(a0)
80002e98: 67 80 00 00  	ret

80002e9c <snrt_l1alloc>:
; void *snrt_l1alloc(size_t size) {
80002e9c: 13 01 01 ff  	addi	sp, sp, -16
80002ea0: 23 26 11 00  	sw	ra, 12(sp)
80002ea4: 23 24 81 00  	sw	s0, 8(sp)
80002ea8: 13 04 05 00  	mv	s0, a0
;     struct snrt_allocator_inst *alloc = &snrt_current_team()->allocator.l1;
80002eac: 97 00 00 00  	auipc	ra, 0
80002eb0: e7 80 c0 f7  	jalr	-132(ra)
80002eb4: 93 05 05 00  	mv	a1, a0
;     size = ALIGN_UP(size, MIN_CHUNK_SIZE);
80002eb8: 13 05 74 00  	addi	a0, s0, 7
80002ebc: 13 76 85 ff  	andi	a2, a0, -8
;     if (alloc->next + size > alloc->base + alloc->size) {
80002ec0: 03 a5 85 04  	lw	a0, 72(a1)
80002ec4: 83 a6 05 04  	lw	a3, 64(a1)
80002ec8: 03 a7 45 04  	lw	a4, 68(a1)
80002ecc: 33 06 c5 00  	add	a2, a0, a2
80002ed0: b3 06 d7 00  	add	a3, a4, a3
80002ed4: 63 fc c6 00  	bgeu	a3, a2, 0x80002eec <snrt_l1alloc+0x50>
80002ed8: 13 05 00 00  	li	a0, 0
; }
80002edc: 83 20 c1 00  	lw	ra, 12(sp)
80002ee0: 03 24 81 00  	lw	s0, 8(sp)
80002ee4: 13 01 01 01  	addi	sp, sp, 16
80002ee8: 67 80 00 00  	ret
;     alloc->next += size;
80002eec: 23 a4 c5 04  	sw	a2, 72(a1)
; }
80002ef0: 83 20 c1 00  	lw	ra, 12(sp)
80002ef4: 03 24 81 00  	lw	s0, 8(sp)
80002ef8: 13 01 01 01  	addi	sp, sp, 16
80002efc: 67 80 00 00  	ret

80002f00 <snrt_alloc_init>:
;         ALIGN_UP((uint32_t)team->cluster_mem.start, MIN_CHUNK_SIZE);
80002f00: 03 26 05 03  	lw	a2, 48(a0)
;         (uint32_t)(team->cluster_mem.end - team->cluster_mem.start);
80002f04: 83 26 85 03  	lw	a3, 56(a0)
;         ALIGN_UP((uint32_t)_edram + l3off, MIN_CHUNK_SIZE);
80002f08: 37 d7 00 80  	lui	a4, 524301
;         ALIGN_UP((uint32_t)team->cluster_mem.start, MIN_CHUNK_SIZE);
80002f0c: 93 07 76 00  	addi	a5, a2, 7
80002f10: 93 f7 87 ff  	andi	a5, a5, -8
;     team->allocator.l1.base =
80002f14: 23 20 f5 04  	sw	a5, 64(a0)
;         (uint32_t)(team->cluster_mem.end - team->cluster_mem.start);
80002f18: 33 86 c6 40  	sub	a2, a3, a2
;     team->allocator.l1.size =
80002f1c: 23 22 c5 04  	sw	a2, 68(a0)
;     team->allocator.l1.next = team->allocator.l1.base;
80002f20: 23 24 f5 04  	sw	a5, 72(a0)
;         ALIGN_UP((uint32_t)_edram + l3off, MIN_CHUNK_SIZE);
80002f24: 03 26 47 69  	lw	a2, 1684(a4)
;     team->allocator.l3.size = 0;
80002f28: 23 28 05 04  	sw	zero, 80(a0)
;         ALIGN_UP((uint32_t)_edram + l3off, MIN_CHUNK_SIZE);
80002f2c: b3 85 c5 00  	add	a1, a1, a2
80002f30: 93 85 75 00  	addi	a1, a1, 7
80002f34: 93 f5 85 ff  	andi	a1, a1, -8
;     team->allocator.l3.base =
80002f38: 23 26 b5 04  	sw	a1, 76(a0)
;     team->allocator.l3.next = team->allocator.l3.base;
80002f3c: 23 2a b5 04  	sw	a1, 84(a0)
; }
80002f40: 67 80 00 00  	ret

80002f44 <_snrt_init_team>:
;                      struct snrt_team_root *team) {
80002f44: 13 01 01 fe  	addi	sp, sp, -32
80002f48: 23 2e 11 00  	sw	ra, 28(sp)
80002f4c: 23 2c 81 00  	sw	s0, 24(sp)
80002f50: 23 2a 91 00  	sw	s1, 20(sp)
80002f54: 23 28 21 01  	sw	s2, 16(sp)
80002f58: 23 26 31 01  	sw	s3, 12(sp)
80002f5c: 23 24 41 01  	sw	s4, 8(sp)
80002f60: 23 22 51 01  	sw	s5, 4(sp)
80002f64: 13 84 07 00  	mv	s0, a5
80002f68: 93 04 07 00  	mv	s1, a4
80002f6c: 13 09 06 00  	mv	s2, a2
80002f70: 93 89 05 00  	mv	s3, a1
;     team->base.root = team;
80002f74: 23 a0 f7 00  	sw	a5, 0(a5)
;     team->bootdata = (void *)bootdata;
80002f78: 23 a2 e7 00  	sw	a4, 4(a5)
;     team->global_core_base_hartid = bootdata->hartid_base;
80002f7c: 83 25 47 01  	lw	a1, 20(a4)
80002f80: 03 2a 07 01  	lw	s4, 16(a4)
;     asm("csrr %0, mhartid" : "=r"(hartid));
80002f84: f3 2a 40 f1  	csrr	s5, mhartid
;     team->global_core_base_hartid = bootdata->hartid_base;
80002f88: 23 a4 47 01  	sw	s4, 8(a5)
;     team->global_core_num = bootdata->core_count;
80002f8c: 83 26 c7 00  	lw	a3, 12(a4)
80002f90: 03 26 87 00  	lw	a2, 8(a4)
;         (snrt_hartid() - bootdata->hartid_base) / bootdata->core_count;
80002f94: 33 85 4a 41  	sub	a0, s5, s4
80002f98: 33 b7 4a 01  	sltu	a4, s5, s4
80002f9c: b3 85 e5 00  	add	a1, a1, a4
;     team->global_core_num = bootdata->core_count;
80002fa0: 23 a6 c7 00  	sw	a2, 12(a5)
;         (snrt_hartid() - bootdata->hartid_base) / bootdata->core_count;
80002fa4: b3 05 b0 40  	neg	a1, a1
80002fa8: 97 d0 ff ff  	auipc	ra, 1048573
80002fac: e7 80 c0 48  	jalr	1164(ra)
;     team->cluster_idx =
80002fb0: 23 28 a4 00  	sw	a0, 16(s0)
;     team->cluster_num = 0;
80002fb4: 23 2a 04 00  	sw	zero, 20(s0)
;     team->cluster_core_base_hartid = bootdata->hartid_base;
80002fb8: 23 2c 44 01  	sw	s4, 24(s0)
;     team->cluster_core_num = cluster_core_num;
80002fbc: 23 2e 34 01  	sw	s3, 28(s0)
;     team->global_mem.start = (uint64_t)bootdata->global_mem_start;
80002fc0: 03 a5 44 03  	lw	a0, 52(s1)
80002fc4: 83 a5 04 03  	lw	a1, 48(s1)
80002fc8: 13 06 80 44  	li	a2, 1096
;     putc_buffer[snrt_hartid()].hdr.size = 0;
80002fcc: 33 86 ca 02  	mul	a2, s5, a2
;     team->global_mem.start = (uint64_t)bootdata->global_mem_start;
80002fd0: 23 22 a4 02  	sw	a0, 36(s0)
80002fd4: 23 20 b4 02  	sw	a1, 32(s0)
;     team->global_mem.end = (uint64_t)bootdata->global_mem_end;
80002fd8: 03 a5 84 03  	lw	a0, 56(s1)
80002fdc: 83 a5 c4 03  	lw	a1, 60(s1)
;     team->cluster_mem.start = (uint64_t)spm_start;
80002fe0: 23 2a 04 02  	sw	zero, 52(s0)
80002fe4: 23 28 24 03  	sw	s2, 48(s0)
;     team->global_mem.end = (uint64_t)bootdata->global_mem_end;
80002fe8: 23 24 a4 02  	sw	a0, 40(s0)
80002fec: 23 26 b4 02  	sw	a1, 44(s0)
;     team->cluster_mem.end = (uint64_t)spm_start + bootdata->tcdm_size;
80002ff0: 03 a5 44 02  	lw	a0, 36(s1)
80002ff4: 83 a5 04 02  	lw	a1, 32(s1)
;     putc_buffer[snrt_hartid()].hdr.size = 0;
80002ff8: b7 d6 00 80  	lui	a3, 524301
80002ffc: 93 86 46 69  	addi	a3, a3, 1684
80003000: 33 06 d6 00  	add	a2, a2, a3
;     team->barrier_reg_ptr = (uint32_t)spm_start + bootdata->tcdm_size +
80003004: b3 06 b9 00  	add	a3, s2, a1
;     team->cluster_mem.end = (uint64_t)spm_start + bootdata->tcdm_size;
80003008: b3 b5 b6 00  	sltu	a1, a3, a1
8000300c: 33 05 b5 00  	add	a0, a0, a1
80003010: 23 2c d4 02  	sw	a3, 56(s0)
80003014: 23 2e a4 02  	sw	a0, 60(s0)
;     team->barrier_reg_ptr = (uint32_t)spm_start + bootdata->tcdm_size +
80003018: 13 85 06 04  	addi	a0, a3, 64
8000301c: 23 20 a4 06  	sw	a0, 96(s0)
;     team->cluster_barrier.barrier = 0;
80003020: 23 2c 04 04  	sw	zero, 88(s0)
;     team->cluster_barrier.barrier_iteration = 0;
80003024: 23 2e 04 04  	sw	zero, 92(s0)

80003028 <.LBB0_1>:
;     _snrt_team_current = &team->base;
80003028: 17 05 00 00  	auipc	a0, 0
8000302c: 03 25 45 5a  	lw	a0, 1444(a0)
;         (uint32_t *)(spm_start + bootdata->tcdm_size +
80003030: 93 85 06 03  	addi	a1, a3, 48
;     _snrt_team_current = &team->base;
80003034: 33 05 45 00  	add	a0, a0, tp
80003038: 23 20 85 00  	sw	s0, 0(a0)
;         (snrt_hartid() - _snrt_team_current->root->cluster_core_base_hartid) %
8000303c: 03 25 04 00  	lw	a0, 0(s0)
80003040: 03 27 85 01  	lw	a4, 24(a0)
;         _snrt_team_current->root->cluster_core_num;
80003044: 03 25 c5 01  	lw	a0, 28(a0)

80003048 <.LBB0_2>:
;     _snrt_core_idx =
80003048: 97 07 00 00  	auipc	a5, 0
8000304c: 83 a7 87 58  	lw	a5, 1416(a5)
;         (snrt_hartid() - _snrt_team_current->root->cluster_core_base_hartid) %
80003050: 33 87 ea 40  	sub	a4, s5, a4
80003054: 33 75 a7 02  	remu	a0, a4, a0
;     _snrt_core_idx =
80003058: 33 87 47 00  	add	a4, a5, tp
8000305c: 23 20 a7 00  	sw	a0, 0(a4)
;     putc_buffer[snrt_hartid()].hdr.size = 0;
80003060: 23 20 06 00  	sw	zero, 0(a2)
;     team->peripherals.perf_counters =
80003064: 23 24 d4 06  	sw	a3, 104(s0)
;     team->peripherals.wakeup = (uint32_t *)0;  // not supported in RTL anymore
80003068: 23 22 04 06  	sw	zero, 100(s0)
;     team->peripherals.cl_clint =
8000306c: 23 26 b4 06  	sw	a1, 108(s0)
;     snrt_alloc_init(team, sizeof(struct putc_buffer));
80003070: 93 05 80 44  	li	a1, 1096
80003074: 13 05 04 00  	mv	a0, s0
80003078: 97 00 00 00  	auipc	ra, 0
8000307c: e7 80 80 e8  	jalr	-376(ra)
;     snrt_int_init(team);
80003080: 13 05 04 00  	mv	a0, s0
80003084: 83 20 c1 01  	lw	ra, 28(sp)
80003088: 03 24 81 01  	lw	s0, 24(sp)
8000308c: 83 24 41 01  	lw	s1, 20(sp)
80003090: 03 29 01 01  	lw	s2, 16(sp)
80003094: 83 29 c1 00  	lw	s3, 12(sp)
80003098: 03 2a 81 00  	lw	s4, 8(sp)
8000309c: 83 2a 41 00  	lw	s5, 4(sp)
800030a0: 13 01 01 02  	addi	sp, sp, 32
800030a4: 17 03 00 00  	auipc	t1, 0
800030a8: 67 00 83 00  	jr	8(t1)

800030ac <snrt_int_init>:
;     clint_p = team->peripherals.cl_clint;
800030ac: 03 25 c5 06  	lw	a0, 108(a0)
800030b0: b7 05 00 00  	lui	a1, 0
800030b4: b3 85 45 00  	add	a1, a1, tp
800030b8: 23 a4 a5 00  	sw	a0, 8(a1)
; }
800030bc: 67 80 00 00  	ret

800030c0 <__snrt_isr>:
; void __snrt_isr(void) {
800030c0: 13 01 01 ff  	addi	sp, sp, -16
;     uint32_t core_idx = snrt_global_core_idx();
800030c4: 23 26 11 00  	sw	ra, 12(sp)
800030c8: 97 00 00 00  	auipc	ra, 0
800030cc: e7 80 40 d7  	jalr	-652(ra)
;     uint32_t cause = read_csr(mcause);
800030d0: f3 25 20 34  	csrr	a1, mcause
;     if (cause & MCAUSE_INTERRUPT) {
800030d4: 63 c8 05 00  	bltz	a1, 0x800030e4 <__snrt_isr+0x24>
800030d8: 13 00 00 00  	nop
800030dc: 13 00 00 00  	nop
;         while (1)
800030e0: 6f 00 00 00  	j	0x800030e0 <__snrt_isr+0x20>
;         switch (cause & ~MCAUSE_INTERRUPT) {
800030e4: 93 95 15 00  	slli	a1, a1, 1
800030e8: 93 d5 15 00  	srli	a1, a1, 1
800030ec: 93 85 d5 ff  	addi	a1, a1, -3
800030f0: 13 d6 25 00  	srli	a2, a1, 2
800030f4: 93 95 e5 01  	slli	a1, a1, 30
800030f8: b3 e5 c5 00  	or	a1, a1, a2
800030fc: 13 06 10 00  	li	a2, 1
80003100: 63 40 b6 02  	blt	a2, a1, 0x80003120 <__snrt_isr+0x60>
80003104: 63 82 05 04  	beqz	a1, 0x80003148 <__snrt_isr+0x88>
80003108: 13 06 10 00  	li	a2, 1
;         switch (cause & ~MCAUSE_INTERRUPT) {
8000310c: 63 9e c5 04  	bne	a1, a2, 0x80003168 <__snrt_isr+0xa8>
;                 irq_m_timer(core_idx);
80003110: 83 20 c1 00  	lw	ra, 12(sp)
80003114: 13 01 01 01  	addi	sp, sp, 16
80003118: 17 03 00 00  	auipc	t1, 0
8000311c: 67 00 c3 0a  	jr	172(t1)
80003120: 13 06 20 00  	li	a2, 2
;         switch (cause & ~MCAUSE_INTERRUPT) {
80003124: 63 8a c5 02  	beq	a1, a2, 0x80003158 <__snrt_isr+0x98>
80003128: 13 05 40 00  	li	a0, 4
;         switch (cause & ~MCAUSE_INTERRUPT) {
8000312c: 63 9e a5 02  	bne	a1, a0, 0x80003168 <__snrt_isr+0xa8>
;                 core_idx = snrt_cluster_core_idx();
80003130: 97 00 00 00  	auipc	ra, 0
80003134: e7 80 c0 d2  	jalr	-724(ra)
;                 irq_m_cluster(core_idx);
80003138: 83 20 c1 00  	lw	ra, 12(sp)
8000313c: 13 01 01 01  	addi	sp, sp, 16
80003140: 17 03 00 00  	auipc	t1, 0
80003144: 67 00 c3 08  	jr	140(t1)
;                 irq_m_soft(core_idx);
80003148: 83 20 c1 00  	lw	ra, 12(sp)
8000314c: 13 01 01 01  	addi	sp, sp, 16
80003150: 17 03 00 00  	auipc	t1, 0
80003154: 67 00 43 02  	jr	36(t1)
;                 irq_m_ext(core_idx);
80003158: 83 20 c1 00  	lw	ra, 12(sp)
8000315c: 13 01 01 01  	addi	sp, sp, 16
80003160: 17 03 00 00  	auipc	t1, 0
80003164: 67 00 83 06  	jr	104(t1)
; }
80003168: 83 20 c1 00  	lw	ra, 12(sp)
8000316c: 13 01 01 01  	addi	sp, sp, 16
80003170: 67 80 00 00  	ret

80003174 <irq_m_soft>:
;     asm volatile(
80003174: b7 d5 00 80  	lui	a1, 524301
80003178: 93 85 05 69  	addi	a1, a1, 1680
8000317c: 13 86 05 00  	mv	a2, a1
80003180: 93 06 10 00  	li	a3, 1
80003184: 93 02 10 00  	li	t0, 1
80003188: af 22 56 0c  	amoswap.w.aq	t0, t0, (a2)
8000318c: e3 9e 02 fe  	bnez	t0, 0x80003188 <irq_m_soft+0x14>
;     *(clint_p + ((hartid & ~0x1f) >> 5)) &= ~(1 << (hartid & 0x1f));
80003190: 33 96 a6 00  	sll	a2, a3, a0
80003194: 13 46 f6 ff  	not	a2, a2
80003198: b7 06 00 00  	lui	a3, 0
8000319c: b3 86 46 00  	add	a3, a3, tp
800031a0: 83 a6 86 00  	lw	a3, 8(a3)
800031a4: 13 55 35 00  	srli	a0, a0, 3
800031a8: 13 75 c5 ff  	andi	a0, a0, -4
800031ac: 33 85 a6 00  	add	a0, a3, a0
800031b0: 83 26 05 00  	lw	a3, 0(a0)
800031b4: 33 f6 c6 00  	and	a2, a3, a2
800031b8: 23 20 c5 00  	sw	a2, 0(a0)
;     asm volatile("amoswap.w.rl  x0,x0,(%0)   # Release lock by storing 0\n"
800031bc: 2f a0 05 0a  	amoswap.w.rl	zero, zero, (a1)
; }
800031c0: 67 80 00 00  	ret

800031c4 <irq_m_timer>:
; void __attribute__((weak)) irq_m_timer(uint32_t core_idx) { (void)core_idx; }
800031c4: 67 80 00 00  	ret

800031c8 <irq_m_ext>:
; void __attribute__((weak)) irq_m_ext(uint32_t core_idx) { (void)core_idx; }
800031c8: 67 80 00 00  	ret

800031cc <irq_m_cluster>:
; void __attribute__((weak)) irq_m_cluster(uint32_t cluster_core_idx) {
800031cc: 93 05 10 00  	li	a1, 1
;     snrt_int_cluster_clr(1 << cluster_core_idx);
800031d0: 33 95 a5 00  	sll	a0, a1, a0
; void snrt_int_cluster_clr(uint32_t mask) { cl_clint_clr(mask); }
800031d4: b7 05 00 00  	lui	a1, 0
800031d8: b3 85 45 00  	add	a1, a1, tp
800031dc: 83 a5 85 00  	lw	a1, 8(a1)
800031e0: 23 a4 a5 00  	sw	a0, 8(a1)
; }
800031e4: 67 80 00 00  	ret
