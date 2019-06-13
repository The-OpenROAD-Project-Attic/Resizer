# rebuffer 2 pin steiner with root branch other pt equal root
source "helpers.tcl"
read_liberty liberty1.lib
read_lef liberty1.lef
read_def reg3.def
create_clock clk -period 1

set buffer_cell [get_lib_cell liberty1/snl_bufx2]
# use 100x wire cap to tickle buffer insertion
set_wire_rc -resistance 1.7e+5 -capacitance 1.3e-8
sta::rebuffer_instance [get_cell u1] $buffer_cell

set def_file [make_result_file rebuffer4.def]
write_def $def_file
report_file $def_file
