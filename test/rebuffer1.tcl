# rebuffer fanout 4 regs in a line driven from one end
source "helpers.tcl"
read_liberty liberty1.lib
read_lef liberty1.lef
read_def rebuffer1.def
create_clock clk -period 1

set buffer_cell [get_lib_cell liberty1/snl_bufx2]
# use 100x wire cap to tickle buffer insertion
set_wire_rc -resistance 1.7e+5 -capacitance 1.3e-8

report_checks -fields {input_pin capacitance}

#      s5  s6  s7
#  r1  r2  r3  r4  r5
# 1,1 1,2 1,3 1,4 1,5
sta::rebuffer_net [get_pin_net r1/Q] $buffer_cell
report_checks -fields {input_pin capacitance}

set def_file [make_result_file rebuffer1.def]
write_def -sort $def_file
report_file $def_file
