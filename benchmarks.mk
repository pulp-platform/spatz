# Small runner for Spatz VQ GEMM/GEMV benchmark variants.
#
# Examples:
#   make -f benchmarks.mk build-vqgemm
#   make -f benchmarks.mk run-vqgemm-vlxblk-materialized M=32 N=128 K=32
#   make -f benchmarks.mk run-vqgemv-rvv-fused N=128 K=32 BLKLEN=8
#   make -f benchmarks.mk run-all-vq
#
# BLKLEN selects the generated block-length data and VQ_BLOCK_LEN.

.DEFAULT_GOAL := help

ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
CLUSTER ?= $(ROOT)hw/system/spatz_cluster
BUILD ?= $(CLUSTER)/sw/build
SIM ?= $(CLUSTER)/bin/spatz_cluster.vsim
PREFIX ?= test-spatzBenchmarks-

M ?= 32
N ?= 128
K ?= 32
BLKLEN ?= 8
DIM := M$(M)_N$(N)_K$(K)
GEMV_DIM := M1_N$(N)_K$(K)
BLK_SUFFIX := $(if $(filter 8,$(BLKLEN)),,-d$(BLKLEN))

DENSE_M ?= 128
DENSE_N ?= 128
DENSE_K ?= 128
DENSE_DIM := M$(DENSE_M)_N$(DENSE_N)_K$(DENSE_K)

VQGEMM_DIMS ?= M32_N32_K32 M32_N128_K32 M128_N128_K128 M128_N256_K128
VQGEMV_DIMS ?= M1_N32_K32 M1_N128_K32 M1_N128_K128 M1_N256_K128 M1_N512_K128
VQDOTP_DIMS ?= M1_N32_K32
DENSE_DIMS ?= M64_N64_K64 M64_N128_K64 M128_N128_K128

VQGEMM_RVV_MATERIALIZED_TARGETS := $(addprefix hp-vqgemm-rvv-materialized_,$(VQGEMM_DIMS))
VQGEMM_RVV_FUSED_TARGETS := $(addprefix hp-vqgemm-rvv-fused_,$(VQGEMM_DIMS))
VQGEMM_VLXBLK_MATERIALIZED_TARGETS := $(addprefix hp-vqgemm-vlxblk-materialized_,$(VQGEMM_DIMS))
VQGEMM_VLXBLK_FUSED_TARGETS := $(addprefix hp-vqgemm-vlxblk-fused_,$(VQGEMM_DIMS))
VQGEMM_TARGETS := $(VQGEMM_RVV_MATERIALIZED_TARGETS) $(VQGEMM_RVV_FUSED_TARGETS) $(VQGEMM_VLXBLK_MATERIALIZED_TARGETS) $(VQGEMM_VLXBLK_FUSED_TARGETS)

VQGEMV_RVV_MATERIALIZED_TARGETS := $(addprefix hp-vqgemv-rvv-materialized_,$(VQGEMV_DIMS))
VQGEMV_RVV_FUSED_TARGETS := $(addprefix hp-vqgemv-rvv-fused_,$(VQGEMV_DIMS))
VQGEMV_VLXBLK_MATERIALIZED_TARGETS := $(addprefix hp-vqgemv-vlxblk-materialized_,$(VQGEMV_DIMS))
VQGEMV_VLXBLK_FUSED_TARGETS := $(addprefix hp-vqgemv-vlxblk-fused_,$(VQGEMV_DIMS))
VQGEMV_TARGETS := $(VQGEMV_RVV_MATERIALIZED_TARGETS) $(VQGEMV_RVV_FUSED_TARGETS) $(VQGEMV_VLXBLK_MATERIALIZED_TARGETS) $(VQGEMV_VLXBLK_FUSED_TARGETS)
VQDOTP_RVV_TARGETS := $(addprefix hp-vqdotp-rvv-fused_,$(VQDOTP_DIMS))
VQDOTP_VLXBLK_TARGETS := $(addprefix hp-vqdotp-vlxblk-fused_,$(VQDOTP_DIMS))
VQDOTP_TARGETS := $(VQDOTP_RVV_TARGETS) $(VQDOTP_VLXBLK_TARGETS)

DENSE_HP_TARGETS := $(addprefix hp-fmatmul_,$(DENSE_DIMS))
ALL_TARGETS := $(VQGEMM_TARGETS) $(VQGEMV_TARGETS) $(VQDOTP_TARGETS) $(DENSE_HP_TARGETS)

DENSE_HP := hp-fmatmul_$(DENSE_DIM)
VQGEMM_RVV_MATERIALIZED := hp-vqgemm-rvv-materialized$(BLK_SUFFIX)_$(DIM)
VQGEMM_RVV_FUSED := hp-vqgemm-rvv-fused$(BLK_SUFFIX)_$(DIM)
VQGEMM_VLXBLK_MATERIALIZED := hp-vqgemm-vlxblk-materialized$(BLK_SUFFIX)_$(DIM)
VQGEMM_VLXBLK_FUSED := hp-vqgemm-vlxblk-fused$(BLK_SUFFIX)_$(DIM)
VQGEMV_RVV_MATERIALIZED := hp-vqgemv-rvv-materialized$(BLK_SUFFIX)_$(GEMV_DIM)
VQGEMV_RVV_FUSED := hp-vqgemv-rvv-fused$(BLK_SUFFIX)_$(GEMV_DIM)
VQGEMV_VLXBLK_MATERIALIZED := hp-vqgemv-vlxblk-materialized$(BLK_SUFFIX)_$(GEMV_DIM)
VQGEMV_VLXBLK_FUSED := hp-vqgemv-vlxblk-fused$(BLK_SUFFIX)_$(GEMV_DIM)
VQDOTP_RVV := hp-vqdotp-rvv-fused$(BLK_SUFFIX)_$(GEMV_DIM)
VQDOTP_VLXBLK := hp-vqdotp-vlxblk-fused$(BLK_SUFFIX)_$(GEMV_DIM)

VQGEMM_SELECTED := $(VQGEMM_RVV_MATERIALIZED) $(VQGEMM_RVV_FUSED) $(VQGEMM_VLXBLK_MATERIALIZED) $(VQGEMM_VLXBLK_FUSED)
VQGEMV_SELECTED := $(VQGEMV_RVV_MATERIALIZED) $(VQGEMV_RVV_FUSED) $(VQGEMV_VLXBLK_MATERIALIZED) $(VQGEMV_VLXBLK_FUSED)
VQDOTP_SELECTED := $(VQDOTP_RVV) $(VQDOTP_VLXBLK)

define require_build_dir
	@if [ ! -f "$(BUILD)/CMakeCache.txt" ]; then \
		echo "Missing $(BUILD)/CMakeCache.txt"; \
		echo "First create the SW build dir with:"; \
		echo "  make -C $(CLUSTER) sw.vsim"; \
		exit 2; \
	fi
endef

define require_cluster_config
	@if ! grep -Eq '^SNRT_CLUSTER_CORE_NUM:STRING=.+$$' "$(BUILD)/CMakeCache.txt" || \
	    ! grep -Eq '^SNRT_TCDM_START_ADDR:STRING=.+$$' "$(BUILD)/CMakeCache.txt" || \
	    ! grep -Eq '^SNRT_TCDM_SIZE:STRING=.+$$' "$(BUILD)/CMakeCache.txt"; then \
		echo "$(BUILD) was not configured through the cluster Makefile."; \
		echo "Refresh it with:"; \
		echo "  make -f benchmarks.mk refresh-vsim"; \
		echo "or:"; \
		echo "  cd $(CLUSTER) && make sw.vsim"; \
		exit 2; \
	fi
endef

define build_targets
	$(call require_build_dir)
	@for target in $(1); do \
		echo "==> build $$target"; \
		cmake --build "$(BUILD)" --target "$(PREFIX)$$target" || exit $$?; \
	done
endef

define run_targets
	$(call build_targets,$(1))
	@for target in $(1); do \
		echo "==> run $$target"; \
		cd "$(CLUSTER)" && "$(SIM)" "./sw/build/spatzBenchmarks/$(PREFIX)$$target" || exit $$?; \
	done
endef

.PHONY: help list configure refresh-vsim
help:
	@echo "Spatz benchmark helper"
	@echo ""
	@echo "Selected VQ GEMM dimension: M=$(M) N=$(N) K=$(K) BLKLEN=$(BLKLEN)"
	@echo "Selected VQ GEMV dimension: N=$(N) K=$(K) BLKLEN=$(BLKLEN)"
	@echo "Selected dense dimension: DENSE_M=$(DENSE_M) DENSE_N=$(DENSE_N) DENSE_K=$(DENSE_K)"
	@echo ""
	@echo "VQ GEMM modes:"
	@echo "  make -f benchmarks.mk run-vqgemm-rvv-materialized M=32 N=128 K=32 BLKLEN=8"
	@echo "  make -f benchmarks.mk run-vqgemm-rvv-fused       M=32 N=128 K=32 BLKLEN=8"
	@echo "  make -f benchmarks.mk run-vqgemm-vlxblk-materialized  M=32 N=128 K=32 BLKLEN=8"
	@echo "  make -f benchmarks.mk run-vqgemm-vlxblk-fused         M=32 N=128 K=32 BLKLEN=8"
	@echo ""
	@echo "VQ GEMV modes:"
	@echo "  make -f benchmarks.mk run-vqgemv-rvv-materialized N=128 K=32 BLKLEN=8"
	@echo "  make -f benchmarks.mk run-vqgemv-rvv-fused        N=128 K=32 BLKLEN=8"
	@echo "  make -f benchmarks.mk run-vqgemv-vlxblk-materialized   N=128 K=32 BLKLEN=8"
	@echo "  make -f benchmarks.mk run-vqgemv-vlxblk-fused          N=128 K=32 BLKLEN=8"
	@echo ""
	@echo "VQ DOTP modes:"
	@echo "  make -f benchmarks.mk run-vqdotp-rvv    N=32 K=32 BLKLEN=8"
	@echo "  make -f benchmarks.mk run-vqdotp-vlxblk N=32 K=32 BLKLEN=8"
	@echo ""
	@echo "Groups:"
	@echo "  make -f benchmarks.mk build-vqgemm"
	@echo "  make -f benchmarks.mk build-vqgemv"
	@echo "  make -f benchmarks.mk run-all-vq"
	@echo "  make -f benchmarks.mk run-dense-hp DENSE_M=128 DENSE_N=128 DENSE_K=128"
	@echo "  make -f benchmarks.mk refresh-vsim"

list:
	@echo "Selected block length: $(BLKLEN)"
	@echo "VQ GEMM rvv-materialized targets: $(VQGEMM_RVV_MATERIALIZED_TARGETS)"
	@echo "VQ GEMM rvv-fused targets: $(VQGEMM_RVV_FUSED_TARGETS)"
	@echo "VQ GEMM vlxblk-materialized targets: $(VQGEMM_VLXBLK_MATERIALIZED_TARGETS)"
	@echo "VQ GEMM vlxblk-fused targets: $(VQGEMM_VLXBLK_FUSED_TARGETS)"
	@echo "Selected VQ GEMM targets: $(VQGEMM_SELECTED)"
	@echo "VQ GEMV rvv-materialized targets: $(VQGEMV_RVV_MATERIALIZED_TARGETS)"
	@echo "VQ GEMV rvv-fused targets: $(VQGEMV_RVV_FUSED_TARGETS)"
	@echo "VQ GEMV vlxblk-materialized targets: $(VQGEMV_VLXBLK_MATERIALIZED_TARGETS)"
	@echo "VQ GEMV vlxblk-fused targets: $(VQGEMV_VLXBLK_FUSED_TARGETS)"
	@echo "Selected VQ GEMV targets: $(VQGEMV_SELECTED)"
	@echo "VQ DOTP rvv targets: $(VQDOTP_RVV_TARGETS)"
	@echo "VQ DOTP vlxblk targets: $(VQDOTP_VLXBLK_TARGETS)"
	@echo "Selected VQ DOTP targets: $(VQDOTP_SELECTED)"
	@echo "Dense HP targets: $(DENSE_HP_TARGETS)"

configure:
	$(call require_build_dir)
	$(call require_cluster_config)

refresh-vsim:
	@$(MAKE) -C "$(CLUSTER)" sw.vsim

.PHONY: build-all run-all build-vq run-all-vq build-vqgemm run-vqgemm build-vqgemv run-vqgemv build-dense run-dense
build-all: configure
	$(call build_targets,$(ALL_TARGETS))

run-all: configure
	$(call run_targets,$(ALL_TARGETS))

build-vq: configure
	$(call build_targets,$(VQGEMM_TARGETS) $(VQGEMV_TARGETS) $(VQDOTP_TARGETS))

run-all-vq: configure
	$(call run_targets,$(VQGEMM_TARGETS) $(VQGEMV_TARGETS) $(VQDOTP_TARGETS))

build-vqgemm: configure
	$(call build_targets,$(VQGEMM_SELECTED))

run-vqgemm: configure
	$(call run_targets,$(VQGEMM_SELECTED))

build-vqgemv: configure
	$(call build_targets,$(VQGEMV_SELECTED))

run-vqgemv: configure
	$(call run_targets,$(VQGEMV_SELECTED))

build-vqdotp: configure
	$(call build_targets,$(VQDOTP_SELECTED))

run-vqdotp: configure
	$(call run_targets,$(VQDOTP_SELECTED))

build-dense: configure
	$(call build_targets,$(DENSE_HP_TARGETS))

run-dense: configure
	$(call run_targets,$(DENSE_HP_TARGETS))

.PHONY: build-vqgemm-rvv-materialized run-vqgemm-rvv-materialized build-vqgemm-rvv-fused run-vqgemm-rvv-fused build-vqgemm-vlxblk-materialized run-vqgemm-vlxblk-materialized build-vqgemm-vlxblk-fused run-vqgemm-vlxblk-fused
build-vqgemm-rvv-materialized: configure
	$(call build_targets,$(VQGEMM_RVV_MATERIALIZED))

run-vqgemm-rvv-materialized: configure
	$(call run_targets,$(VQGEMM_RVV_MATERIALIZED))

build-vqgemm-rvv-fused: configure
	$(call build_targets,$(VQGEMM_RVV_FUSED))

run-vqgemm-rvv-fused: configure
	$(call run_targets,$(VQGEMM_RVV_FUSED))

build-vqgemm-vlxblk-materialized: configure
	$(call build_targets,$(VQGEMM_VLXBLK_MATERIALIZED))

run-vqgemm-vlxblk-materialized: configure
	$(call run_targets,$(VQGEMM_VLXBLK_MATERIALIZED))

build-vqgemm-vlxblk-fused: configure
	$(call build_targets,$(VQGEMM_VLXBLK_FUSED))

run-vqgemm-vlxblk-fused: configure
	$(call run_targets,$(VQGEMM_VLXBLK_FUSED))

.PHONY: build-vqgemv-rvv-materialized run-vqgemv-rvv-materialized build-vqgemv-rvv-fused run-vqgemv-rvv-fused build-vqgemv-vlxblk-materialized run-vqgemv-vlxblk-materialized build-vqgemv-vlxblk-fused run-vqgemv-vlxblk-fused
build-vqgemv-rvv-materialized: configure
	$(call build_targets,$(VQGEMV_RVV_MATERIALIZED))

run-vqgemv-rvv-materialized: configure
	$(call run_targets,$(VQGEMV_RVV_MATERIALIZED))

build-vqgemv-rvv-fused: configure
	$(call build_targets,$(VQGEMV_RVV_FUSED))

run-vqgemv-rvv-fused: configure
	$(call run_targets,$(VQGEMV_RVV_FUSED))

build-vqgemv-vlxblk-materialized: configure
	$(call build_targets,$(VQGEMV_VLXBLK_MATERIALIZED))

run-vqgemv-vlxblk-materialized: configure
	$(call run_targets,$(VQGEMV_VLXBLK_MATERIALIZED))

build-vqgemv-vlxblk-fused: configure
	$(call build_targets,$(VQGEMV_VLXBLK_FUSED))

run-vqgemv-vlxblk-fused: configure
	$(call run_targets,$(VQGEMV_VLXBLK_FUSED))

.PHONY: build-vqdotp-rvv run-vqdotp-rvv build-vqdotp-vlxblk run-vqdotp-vlxblk
build-vqdotp-rvv: configure
	$(call build_targets,$(VQDOTP_RVV))

run-vqdotp-rvv: configure
	$(call run_targets,$(VQDOTP_RVV))

build-vqdotp-vlxblk: configure
	$(call build_targets,$(VQDOTP_VLXBLK))

run-vqdotp-vlxblk: configure
	$(call run_targets,$(VQDOTP_VLXBLK))

.PHONY: build-dense-hp run-dense-hp build-gemv run-gemv
build-dense-hp: configure
	$(call build_targets,$(DENSE_HP))

run-dense-hp: configure
	$(call run_targets,$(DENSE_HP))

build-gemv: build-vqgemv

run-gemv: run-vqgemv
