# Makefrag for spatz instructions
include $(TESTS_DIR)/rv32uiv/Makefrag

rv32uiv_spatz_tests := $(addprefix rv32uiv-spatz-, $(rv32uiv_sc_tests))

spatz_tests := $(rv32uiv_spatz_tests)
