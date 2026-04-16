# Copyright (c) 2025 ETH Zurich.
# Riccardo Fiorani Gallotta <riccardo.fiorani3@unibo.it>

if { ![info exists VCD_START] } {
    set VCD_START 0ns
}

if { ![info exists VCD_DURATION] } {
    set VCD_DURATION -1
}

run ${VCD_START}

exec echo "set VCD_FILE ${VCD_FILE}" > ${VCD_FILE}.var
exec echo "set VCD_START ${VCD_START}" >> ${VCD_FILE}.var
exec echo "set VCD_DURATION ${VCD_DURATION}" >> ${VCD_FILE}.var
exec echo "set NETLISTS ${VCD_NETS}" >> ${VCD_FILE}.var

vcd file ${VCD_FILE}

add_vcd

if {$VCD_DURATION == -1} {
    run -a
} else {
    run ${VCD_DURATION}
}
vcd flush
