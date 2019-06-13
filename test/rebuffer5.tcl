source "helpers.tcl"
read_liberty liberty1.lib
read_lef liberty1.lef
read_def reg3.def
create_clock clk -period 1

set buffer_cell [get_lib_cell liberty1/snl_bufx2]
# use 10x wire cap to tickle buffer insertion
set_wire_rc -resistance 1.7e+5 -capacitance 1.3e-9

report_check_types -max_transition -all_violators

resize -repair_max_slew -buffer_cell $buffer_cell

report_check_types -max_transition -all_violators
report_checks -fields {input_pin transition_time capacitance}

set def_file [make_result_file rebuffer5.def]
write_def $def_file
report_file $def_file
