# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Returns the triplicated (TMR) / duplicated (DMR) flip-flops of the Spatz
# cluster, whatever naming convention the block uses (TMRG `_qA/_qB/_qC`,
# `foo_q_rep[0..2]`, `gen_tmr_part[0..2]`, `i_ipu_a/i_ipu_b`, ...).
#
#   source redundant_regs.tcl         ;# loads redundant_regs_spec.tcl as well
#   foreach grp [get_tmr_regs] {
#     lassign $grp grp_name all_regs  ;# all_regs: the 3 copies of ONE flop
#     COMMAND -name $grp_name -rule TMR_RULE -registers $all_regs ...
#   }
#   foreach grp [get_dmr_regs] { ... -rule DMR_RULE ... }
#
# The spec is produced from the RTL by find_redundant_regs.py (cached on
# Bender.lock): re-run `find_redundant_regs.py` after a dependency update.
#
# Netlist naming: both `name_reg[3]` and `name_reg_3_` style bit suffixes, and
# `gen_x[1].` / `gen_x_1_.` style generate scopes are handled. If the netlist
# root differs from the one used by the RTL hierarchy, pass -root.

namespace eval ::redundancy {
  variable specs {}
  variable indexed 0
  variable all_names {}
  variable by_var
  array set by_var {}
  variable cells
  array set cells {}
}

set ::redundancy::spec_file [file join [file dirname [info script]] redundant_regs_spec.tcl]

proc ::redundancy::load_spec {} {
  variable spec_file
  if {![file exists $spec_file]} {
    puts "ERROR: $spec_file not found, run find_redundant_regs.py first."
    return -code error "missing redundancy spec"
  }
  source $spec_file
}

# One pass over every sequential cell; the lookups below are then hash/regexp
# only (no repeated get_cells -hier).
proc ::redundancy::build_index {} {
  variable indexed
  variable all_names
  variable by_var
  variable cells
  if {$indexed} { return }
  set all_names {}
  array unset by_var
  array unset cells
  foreach_in_collection c [get_cells -hier -quiet -filter "is_sequential == true"] {
    set n [get_attribute $c full_name]
    lappend all_names $n
    set cells($n) $c
    set leaf [lindex [split $n /] end]
    regsub {_reg.*$} $leaf {} v
    lappend by_var($v) $n
  }
  set indexed 1
  puts "INFO: indexed [llength $all_names] sequential cells."
}

# {name idx rest} for a bit suffix: `[2]rest` or `_2_rest`
proc ::redundancy::split_idx {s} {
  if {[regexp {^\[([0-9]+)\](.*)$} $s -> i r]} { return [list $i $r] }
  if {[regexp {^_([0-9]+)_(.*)$} $s -> i r]} { return [list $i $r] }
  return {}
}

# Collect copies for one spec into the dict groups: key -> {copy name ...}
proc ::redundancy::match_spec {spec root} {
  variable all_names
  variable by_var
  lassign $spec kind style sc tmpl copies lit
  set res {}
  switch -- $style {
    var-letter {
      foreach cp $copies {
        set var [string map [list @ $cp] $tmpl]
        if {![info exists by_var($var)]} continue
        foreach n $by_var($var) {
          if {[regexp "^${root}(${sc})\[./\]${var}_reg(.*)\$" $n -> scope rest]} {
            dict lappend res "${scope}/${tmpl}_reg${rest}" [list $cp $n]
          }
        }
      }
    }
    var-array {
      if {![info exists by_var($tmpl)]} { return $res }
      foreach n $by_var($tmpl) {
        if {[regexp "^${root}(${sc})\[./\]${tmpl}_reg(.*)\$" $n -> scope rest]} {
          set ir [split_idx $rest]
          if {$ir eq {}} continue
          lassign $ir i r
          if {[lsearch -exact $copies $i] >= 0} { dict lappend res "${scope}/${tmpl}_reg\[@\]${r}" [list $i $n] }
        }
      }
    }
    inst-index - inst-letter - dmr-inst {
      if {$style eq "inst-index"} {
        set tok {(?:\[([0-9]+)\]|_([0-9]+)_)}
        set re "^${root}(${sc})${tok}(\[./\].*)\$"
      } else {
        set re "^${root}(${sc})(\[A-Za-z\])(\[./\].*)\$"
      }
      foreach n [cands $lit] {
        if {![regexp $re $n -> pre a b c]} continue
        if {$style eq "inst-index"} { set cp [expr {$a ne "" ? $a : $b}]; set rest $c } else { set cp $a; set rest $b }
        if {[lsearch -exact $copies $cp] >= 0} { dict lappend res "${pre}@${rest}" [list $cp $n] }
      }
    }
  }
  return $res
}

# names containing a literal, cached (the inst-* specs reuse few literals)
proc ::redundancy::cands {lit} {
  variable all_names
  variable cand_cache
  if {![info exists cand_cache($lit)]} {
    set l {}
    foreach n $all_names { if {[string first $lit $n] >= 0} { lappend l $n } }
    set cand_cache($lit) $l
  }
  return $cand_cache($lit)
}

proc ::redundancy::collect {want root} {
  variable specs
  variable cells
  variable cand_cache
  array unset cand_cache
  if {$specs eq {}} { load_spec }
  build_index
  set out {}
  set seen [dict create]
  set incomplete 0
  array set bystyle {}
  foreach spec $specs {
    if {[lindex $spec 0] ne $want} continue
    set ncopy [llength [lindex $spec 4]]
    dict for {key members} [match_spec $spec $root] {
      # every flop: all copies must be there, exactly once
      set got {}
      foreach m $members { lappend got [lindex $m 0] }
      set got [lsort -unique $got]
      if {[llength $members] != $ncopy || [llength $got] != $ncopy} {
        incr incomplete
        continue
      }
      set names {}
      foreach m $members { lappend names [lindex $m 1] }
      set dup 0
      foreach n $names { if {[dict exists $seen $n]} { set dup 1 } }
      if {$dup} continue                      ;# already part of another group
      foreach n $names { dict set seen $n 1 }
      set coll $cells([lindex $names 0])
      foreach n [lrange $names 1 end] { set coll [add_to_collection $coll $cells($n)] }
      regsub -all {[^A-Za-z0-9_]} "sr_[string map {@ {}} $key]" _ gname
      lappend out [list $gname $coll]
      set st [lindex $spec 1]
      if {![info exists bystyle($st)]} { set bystyle($st) 0 }
      incr bystyle($st)
    }
  }
  puts "INFO: [string toupper $want]: [llength $out] redundant flops groups, $incomplete incomplete (skipped)."
  foreach st [lsort [array names bystyle]] { puts "INFO:   $st: $bystyle($st)" }
  return $out
}

proc get_tmr_regs {{root ""}} { return [::redundancy::collect tmr $root] }
proc get_dmr_regs {{root ""}} { return [::redundancy::collect dmr $root] }

::redundancy::load_spec
