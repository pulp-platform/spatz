from __future__ import annotations

from dataclasses import asdict, dataclass
from itertools import product
from typing import Any, Dict, Iterable, List, Optional, Union


# ----------------------------------------------------------------------------
# ZVT KMAX table from the architecture specification.
# Key: (TEW, SEW) -> KMAX
# ----------------------------------------------------------------------------
KMAX_TABLE: Dict[tuple, int] = {
    (8, 8): 4,
    (16, 8): 4,
    (16, 16): 2,
    (32, 8): 4,
    (32, 16): 2,
    (32, 32): 1,
    (64, 16): 2,
    (64, 32): 1,
    (64, 64): 1,
}


def _ceil_div(a: int, b: int) -> int:
    """Integer ceiling division. Returns 0 when numerator <= 0."""
    if b <= 0:
        raise ValueError("Division by zero")
    if a <= 0:
        return 0
    return (a + b - 1) // b


def _floor_power_of_two(x: int) -> int:
    """Floor x to a power of two. Returns 0 for x <= 0."""
    if x <= 0:
        return 0
    return 1 << (x.bit_length() - 1)


@dataclass(frozen=True)
class GemmShape:
    """Application GEMM dimensions: C[M,N] = A[M,K] * B[K,N]."""
    M: int
    N: int
    K: int


@dataclass
class MicroArchParams:
    """
    Hardware and microarchitecture parameters.
    Default: VLEN=512, TE=16, CE=8, SEW=32, TEW=32
    """
    VLEN: int = 512
    TE: int = 16
    CE: int = 8
    SEW: int = 32
    TEW: int = 32

    # LSU fixed pipeline latencies.
    load_latency: int = 3
    store_latency: int = 2

    # Frontend and scalar execution.
    frontend_issue_width: int = 1
    scalar_ops_per_cycle: int = 1
    branch_ops_per_cycle: int = 1

    # Matrix pipeline parameters.
    matrix_min_latency: int = 4

    # vtzero assumed to take one cycle via flash-clear flag.
    vtzero_cycles: int = 1

    # Store strategy: "vtse" or "vtmv_vse".
    store_strategy: str = "vtse"

    # OoO-style resource overlap. If False, use conservative sum of resources.
    overlap_units: bool = True

    # Hardware vector chaining enable. If False, model scoreboard load-use stalls.
    enable_chaining: bool = True

    # Branch inclusion flag.
    enable_branch: bool = True

    # TEW=64 disabled by current assumptions.
    allow_tew64: bool = False

    # 0: concise summary, 1: detailed debug.
    debug_mode: int = 0

    # Loop unrolling
    unroll_n: int = 1
    software_pipelined_store: bool = True


@dataclass(frozen=True)
class DerivedArch:
    """Architecture values derived from hardware parameters."""
    TWIDEN: int
    KMAX: int
    ETE: int
    EVE: int
    LMUL: int
    tile_edge: int
    mac_units: int
    macs_per_cycle: int
    lsu_bw_bits: int
    tile_bw_bits: int
    lsu_load_elems_per_cycle: int
    lsu_store_elems_per_cycle: int
    tile_elems_per_cycle: int


def derive_arch(params: MicroArchParams) -> DerivedArch:
    """
    Derive ZVT configuration and microarchitecture throughput parameters.
    """
    if params.TEW == 64 and not params.allow_tew64:
        raise ValueError("TEW=64 is disabled by current assumptions")

    if params.VLEN <= 0 or params.TE <= 0 or params.CE <= 0:
        raise ValueError("VLEN, TE, and CE must be positive")

    if params.SEW <= 0 or params.TEW <= 0:
        raise ValueError("SEW and TEW must be positive")

    if params.TEW < params.SEW:
        raise ValueError("TEW must be >= SEW")

    if params.TEW % params.SEW != 0:
        raise ValueError("TEW must be a multiple of SEW")

    twiden = params.TEW // params.SEW
    if twiden not in (1, 2, 4):
        raise ValueError("TWIDEN must be 1, 2, or 4")

    key = (params.TEW, params.SEW)
    if key not in KMAX_TABLE:
        raise ValueError(
            f"Unsupported SEW/TEW combination: SEW={params.SEW}, TEW={params.TEW}"
        )

    kmax = KMAX_TABLE[key]

    ete = params.TE if params.TEW < 64 else params.TE // 2

    eve = params.VLEN // params.SEW
    if eve <= 0:
        raise ValueError("EVE must be positive")

    lmul_raw = min(
        8 // kmax,
        8 // twiden,
        _ceil_div(ete, eve),
    )
    lmul = _floor_power_of_two(lmul_raw)
    if lmul <= 0:
        raise ValueError("Derived LMUL is zero")

    tile_edge = min(ete, lmul * eve)
    if tile_edge <= 0:
        raise ValueError("Derived tile edge must be positive")

    mac_units = params.CE * params.CE
    macs_per_cycle = mac_units * twiden

    lsu_bw_bits = 2 * params.CE * params.TEW
    tile_bw_bits = params.TE * params.TEW

    lsu_load_elems_per_cycle = lsu_bw_bits // params.SEW
    lsu_store_elems_per_cycle = lsu_bw_bits // params.TEW
    tile_elems_per_cycle = tile_bw_bits // params.TEW

    return DerivedArch(
        TWIDEN=twiden,
        KMAX=kmax,
        ETE=ete,
        EVE=eve,
        LMUL=lmul,
        tile_edge=tile_edge,
        mac_units=mac_units,
        macs_per_cycle=macs_per_cycle,
        lsu_bw_bits=lsu_bw_bits,
        tile_bw_bits=tile_bw_bits,
        lsu_load_elems_per_cycle=lsu_load_elems_per_cycle,
        lsu_store_elems_per_cycle=lsu_store_elems_per_cycle,
        tile_elems_per_cycle=tile_elems_per_cycle,
    )


def evaluate_gemm(
    gemm: GemmShape,
    params: MicroArchParams,
    debug_mode: Optional[int] = None,
) -> Dict[str, Any]:
    dm = params.debug_mode if debug_mode is None else debug_mode
    d = derive_arch(params)

    strategy = params.store_strategy.lower()
    if strategy not in {"vtse", "vtmv_vse"}:
        raise ValueError("store_strategy must be 'vtse' or 'vtmv_vse'")

    tm = d.tile_edge
    tn = d.tile_edge
    tk = d.KMAX

    # -------------------------------------------------------------------------
    # 1. Application Level (2x2 Tile Blocking: m = 2*tm, n = 2*tn)
    # -------------------------------------------------------------------------
    if strategy == "vtse":
        m_block = 2 * tm
        n_block = 2 * tn
    else:
        m_block = 1 * tm
        n_block = 4 * tn

    blocks_m = _ceil_div(gemm.M, m_block)
    blocks_n = _ceil_div(gemm.N, n_block)
    num_blocks = blocks_m * blocks_n

    tiles_per_block = 4
    total_tiles = num_blocks * tiles_per_block

    k_steps = _ceil_div(gemm.K, tk)
    full_k_steps = gemm.K // tk
    rem_k = gemm.K % tk

    # -------------------------------------------------------------------------
    # 2. Instruction Counts
    # -------------------------------------------------------------------------
    matrix_csr_setup_instr = 4
    pointer_setup_instr = 8
    global_setup_instr = matrix_csr_setup_instr + pointer_setup_instr

    fringe_m = 1 if (gemm.M % m_block != 0) else 0
    fringe_n = 1 if (gemm.N % n_block != 0) else 0
    fringe_k = 1 if (rem_k != 0) else 0
    fringe_csr_instr = (fringe_m * blocks_n * 2) + (fringe_n * blocks_m * 2) + (fringe_k * num_blocks * 2)

    vtzero_total = total_tiles

    # Load
    if strategy == "vtse":
        vector_load_total = num_blocks * (4 * gemm.K)
        load_addr_total = num_blocks * (4 * gemm.K)
    else:
        vector_load_total = num_blocks * (5 * gemm.K)
        load_addr_total = num_blocks * (5 * gemm.K)

    # MAC
    vtfmm_total = num_blocks * 4 * k_steps

    # Store
    if strategy == "vtse":
        vtse_total = total_tiles * tm
        vector_store_total = 0
        vtmv_total = 0
        tss_update_total = total_tiles * tm
        store_addr_total = total_tiles * tm
    else:
        vtse_total = 0
        vector_store_total = total_tiles * tm
        vtmv_total = total_tiles * tm
        tss_update_total = total_tiles * tm
        store_addr_total = total_tiles * tm

    # Loop Control
    k_counter_total = num_blocks * k_steps
    store_counter_total = num_blocks * tm

    effective_n_branches = _ceil_div(blocks_n, params.unroll_n)
    middle_loop_counter = blocks_m * effective_n_branches
    outer_loop_counter = blocks_m

    if params.enable_branch:
        k_branch_total = num_blocks * k_steps
        store_branch_total = num_blocks * tm
        middle_loop_branch = blocks_m * effective_n_branches
        outer_loop_branch = blocks_m
        branch_total = k_branch_total + store_branch_total + middle_loop_branch + outer_loop_branch
    else:
        k_branch_total = 0
        store_branch_total = 0
        middle_loop_branch = 0
        outer_loop_branch = 0
        branch_total = 0

    transition_scalar_instr = 6
    block_transition_scalar = num_blocks * transition_scalar_instr

    scalar_add_sub_total = (
        global_setup_instr
        + fringe_csr_instr
        + load_addr_total
        + store_addr_total
        + tss_update_total
        + k_counter_total
        + store_counter_total
        + middle_loop_counter
        + outer_loop_counter
        + block_transition_scalar
    )

    total_instructions = (
        vtzero_total
        + vector_load_total
        + vtfmm_total
        + vtse_total
        + vtmv_total
        + vector_store_total
        + scalar_add_sub_total
        + branch_total
    )

    # -------------------------------------------------------------------------
    # 3. Microarchitectural Hardware Execution Cycles
    # -------------------------------------------------------------------------
    frontend_cycles = _ceil_div(total_instructions, params.frontend_issue_width)
    scalar_cycles = _ceil_div(scalar_add_sub_total, params.scalar_ops_per_cycle)
    branch_cycles = _ceil_div(branch_total, params.branch_ops_per_cycle) if params.enable_branch else 0

    load_row_a_cycles = max(1, _ceil_div(tm, d.lsu_load_elems_per_cycle))
    load_row_b_cycles = max(1, _ceil_div(tn, d.lsu_load_elems_per_cycle))
    lsu_vector_read_cycle = (
        num_blocks
        * gemm.K
        * (m_block/tm * load_row_a_cycles + n_block/tn * load_row_b_cycles)
    )

    store_row_lsu_cycles = max(1, _ceil_div(tn, min(d.tile_elems_per_cycle, d.lsu_store_elems_per_cycle)))
    lsu_vector_write_cycle = vector_store_total * store_row_lsu_cycles

    lsu_tile_read_cycle = 0
    lsu_tile_write_cycle = vtse_total * store_row_lsu_cycles if strategy == "vtse" else 0

    lsu_cycle_for_vector = lsu_vector_read_cycle + lsu_vector_write_cycle
    lsu_cycle_for_tile = lsu_tile_read_cycle + lsu_tile_write_cycle
    lsu_cycles = lsu_cycle_for_vector + lsu_cycle_for_tile

    tile_row_cycles = max(1, _ceil_div(tn, d.tile_elems_per_cycle))
    tile_store_read_cycles = (vtse_total + vtmv_total) * tile_row_cycles if strategy == "vtse" else 0
    tile_cycles = tile_store_read_cycles
    
    tile_zero_cycles = vtzero_total * params.vtzero_cycles

    def spatial_beats_for(eff_tm: int, eff_tn: int, eff_tk: int) -> int:
        if eff_tm <= 0 or eff_tn <= 0 or eff_tk <= 0:
            return 0
        return (_ceil_div(eff_tm, params.CE) * _ceil_div(eff_tn, params.CE) * _ceil_div(eff_tk, d.TWIDEN))

    step_cycle = spatial_beats_for(tm, tn, tk)
    matrix_cycles = vtfmm_total * step_cycle 

    # -------------------------------------------------------------------------
    # 4. Scoreboard Dependency Hazards & Latency Penalty
    # -------------------------------------------------------------------------
    chaining_penalty = 0
    if not params.enable_chaining:
        load_to_use_stall = max(0, params.load_latency - 1)
        chaining_penalty = num_blocks * k_steps * load_to_use_stall

    effective_load_latency = load_row_b_cycles + params.load_latency - 1 if gemm.K > 0 else 0
    effective_store_latency = (
        store_row_lsu_cycles + params.store_latency - 1
        if tm > 0 and (vtse_total + vector_store_total) > 0
        else 0
    )

    # -------------------------------------------------------------------------
    # 5. Throughput Factor & Issue Starvation Analysis
    # -------------------------------------------------------------------------
    if vtfmm_total > 0:
        issue_interval = total_instructions / (vtfmm_total * params.frontend_issue_width)
    else:
        issue_interval = 0.0

    effective_step_cycle = max(step_cycle, issue_interval)
    
    if issue_interval > 0:
        starvation_factor = min(1.0, step_cycle / issue_interval)
    else:
        starvation_factor = 1.0

    resource_cycles = {
        "frontend_fetch_decode": frontend_cycles,
        "scalar_alu": scalar_cycles,
        "branch": branch_cycles,
        "lsu": lsu_cycles,
        "tile_port": tile_cycles,
        "matrix_mac": matrix_cycles + chaining_penalty,
    }

    total_cycles = (
        max(resource_cycles.values())
        if params.overlap_units
        else sum(resource_cycles.values())
    )

    max_cycles = max(resource_cycles.values()) if resource_cycles else 0
    bottleneck = (
        [k for k, v in resource_cycles.items() if v == max_cycles]
        if max_cycles > 0
        else ["none"]
    )

    resource_cycles = {
        "frontend_fetch_decode": frontend_cycles,
        "scalar_alu": scalar_cycles,
        "branch": branch_cycles,
        "lsu": lsu_cycles,
        "tile_port": tile_cycles,
        "matrix_mac": matrix_cycles + chaining_penalty,
    }

    total_cycles = (
        max(resource_cycles.values())
        if params.overlap_units
        else sum(resource_cycles.values())
    )

    max_cycles = max(resource_cycles.values()) if resource_cycles else 0
    bottleneck = (
        [k for k, v in resource_cycles.items() if v == max_cycles]
        if max_cycles > 0
        else ["none"]
    )

    result: Dict[str, Any] = {
        "inputs": {
            "VLEN": params.VLEN,
            "TE": params.TE,
            "CE": params.CE,
            "SEW": params.SEW,
            "TEW": params.TEW,
            "M": gemm.M,
            "N": gemm.N,
            "K": gemm.K,
            "store_strategy": strategy,
            "enable_branch": params.enable_branch,
        },
        "derived_arch": asdict(d),
        "tiles": {
            "tile_m": tm,
            "tile_n": tn,
            "tile_k_max": tk,
            "block_m": m_block,
            "block_n": n_block,
            "blocks_m": blocks_m,
            "blocks_n": blocks_n,
            "num_blocks": num_blocks,
            "total_tiles": total_tiles,
            "full_k_steps": full_k_steps,
            "rem_k": rem_k,
            "fringe_blocks": fringe_m + fringe_n + fringe_k,
        },
        "instructions": {
            "setup_and_fringe_csr": global_setup_instr + fringe_csr_instr,
            "vtzero": vtzero_total,
            "vector_load": vector_load_total,
            "vector_store": vector_store_total,
            "vtse": vtse_total,
            "vtmv": vtmv_total,
            "matrix": vtfmm_total,
            "scalar_add_sub": scalar_add_sub_total,
            "branch": branch_total,
            "total": total_instructions,
        },
        "throughput_analysis": {
            "spatial_beats_per_vtfmm": step_cycle,
            "avg_issue_interval": issue_interval,
            "effective_step_cycle": effective_step_cycle,
            "mac_starvation_factor": starvation_factor,
        },
        "unit_cycles": resource_cycles,
        "lsu_breakdown": {
            "lsu_cycles": lsu_cycles,
            "lsu_cycle_for_vector": lsu_cycle_for_vector,
            "lsu_vector_read_cycle": lsu_vector_read_cycle,
            "lsu_vector_write_cycle": lsu_vector_write_cycle,
            "lsu_cycle_for_tile": lsu_cycle_for_tile,
            "lsu_tile_read_cycle": lsu_tile_read_cycle,
            "lsu_tile_write_cycle": lsu_tile_write_cycle,
        },
        "latencies": {
            "effective_load_latency": effective_load_latency,
            "effective_store_latency": effective_store_latency,
            "chaining_penalty": chaining_penalty,
        },
        "total_cycles": total_cycles,
        "bottleneck": bottleneck,
        "mac_utilization": (
            (matrix_cycles / total_cycles) * 100.0 if total_cycles else 0.0
        ),
    }

    # -------------------------------------------------------------------------
    # 6. Output Reporting
    # -------------------------------------------------------------------------
    lsu_pct = (100.0 * lsu_cycles / total_cycles) if total_cycles else 0.0
    lsu_str = f"lsu_cycle: {lsu_cycle_for_vector} + {lsu_cycle_for_tile} ({lsu_pct:.2f}% of total)"

    if dm == 0:
        print("=" * 100)
        print(
            f"ZVT result: VLEN={params.VLEN}, TE={params.TE}, CE={params.CE}, "
            f"SEW={params.SEW}, TEW={params.TEW}, "
            f"M={gemm.M}, N={gemm.N}, K={gemm.K}, store={strategy}, enable_branch={params.enable_branch}"
        )
        print(f"Total cycles: {total_cycles} | Bottleneck: {', '.join(bottleneck)}")
        print(f"MAC Busy Utilization: {result['mac_utilization']:.2f}%")
        print(f"LSU breakdown: {lsu_str}")

        formula = ", ".join(f"{k}={v}" for k, v in resource_cycles.items())
        print(f"Limiting formula: max({formula}) = {total_cycles}")
        print("=" * 100)

    elif dm >= 1:
        print("=" * 100)
        print(f"ZVT Microarchitectural Evaluation (enable_branch={params.enable_branch})")
        print("=" * 100)

        print("Inputs:")
        for k, v in result["inputs"].items():
            print(f"  {k}: {v}")

        print("\nDerived architecture:")
        for k, v in result["derived_arch"].items():
            print(f"  {k}: {v}")

        print("\nTile & Block configuration:")
        for k, v in result["tiles"].items():
            print(f"  {k}: {v}")

        print("\nDynamic instruction counts:")
        for k, v in result["instructions"].items():
            print(f"  {k}: {v}")

        print("\nThroughput & Issue Analysis:")
        print(f"  vtfmm HW execution time : {step_cycle} cycles/inst")
        print(f"  Avg issue interval      : {issue_interval:.2f} cycles/inst (due to other insts)")
        print(f"  Effective MAC throughput: {effective_step_cycle:.2f} cycles/inst")
        if starvation_factor < 1.0:
            print(f"  => FMA is STARVED by Frontend! Starvation Factor = {starvation_factor:.4f} ({(1-starvation_factor)*100:.1f}% throughput lost)")
        else:
            print(f"  => FMA is COMPUTE BOUND. Frontend is fast enough.")
        print("\nUnit busy cycles:")
        
        for k, v in resource_cycles.items():
            if k == "lsu":
                print(f"  {lsu_str}")
                print(f"  - load to vector = {lsu_vector_read_cycle}, store from vector = {lsu_vector_write_cycle}")
                print(f"  - load to tile = {lsu_tile_read_cycle}, store from tile = {lsu_tile_write_cycle}")
            else:
                util = 100.0 * v / total_cycles if total_cycles else 0.0
                print(f"  {k}: {v} cycles ({util:.2f}% of total)")

        print(f"\nMAC Busy Utilization: {result['mac_utilization']:.2f}%")
        print("\nBottleneck:")
        print(f"  {', '.join(bottleneck)}")
        print("=" * 100)

    return result


def evaluate_sweep(
    VLEN_list: Iterable[int],
    TE_list: Iterable[int],
    CE_list: Iterable[int],
    SEW_list: Iterable[int],
    TEW_list: Iterable[int],
    gemm_list: Iterable[GemmShape],
    store_strategy: Union[str, Iterable[str]] = "vtse",
    debug_mode: int = 0,
    enable_branch: bool = True,
    **overrides: Any,
) -> List[Dict[str, Any]]:
    if isinstance(store_strategy, str):
        strategies = ["vtse", "vtmv_vse"] if store_strategy.lower() in {"both", "all"} else [store_strategy.lower()]
    else:
        strategies = [str(s).lower() for s in store_strategy]

    overrides = dict(overrides)
    overrides.pop("debug_mode", None)
    overrides.pop("store_strategy", None)
    overrides.pop("enable_branch", None)
    for field in ("VLEN", "TE", "CE", "SEW", "TEW"):
        overrides.pop(field, None)

    results: List[Dict[str, Any]] = []

    for VLEN, TE, CE, SEW, TEW in product(
        VLEN_list, TE_list, CE_list, SEW_list, TEW_list
    ):
        for gemm in gemm_list:
            for strat in strategies:
                params = MicroArchParams(
                    VLEN=VLEN,
                    TE=TE,
                    CE=CE,
                    SEW=SEW,
                    TEW=TEW,
                    store_strategy=strat,
                    debug_mode=debug_mode,
                    enable_branch=bool(enable_branch),
                    **overrides,
                )
                try:
                    res = evaluate_gemm(gemm, params, debug_mode=debug_mode)
                    results.append(res)
                except ValueError as e:
                    if debug_mode:
                        print(f"Skipping invalid configuration: {e}")

    return results


if __name__ == "__main__":
    evaluate_sweep(
        VLEN_list=[512],
        TE_list=[32],
        CE_list=[16],
        SEW_list=[32],
        TEW_list=[32],
        gemm_list=[
            GemmShape(M=64, N=64, K=64),
        ],
        store_strategy="vtse",
        debug_mode=1,
        enable_branch=True,
    )