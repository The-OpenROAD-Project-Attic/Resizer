# rebuffer fanout 4 reg
source "helpers.tcl"
read_liberty liberty1.lib
read_lef liberty1.lef
read_def reg4.def
create_clock clk -period 1

set wire_res_per_meter 1.7e+5
# use 100x wire cap to tickle buffer insertion
set wire_cap_per_meter 1.3e-8
set buffer_cell [get_lib_cell liberty1/snl_bufx2]

sta::make_net_parasitics $wire_res_per_meter $wire_cap_per_meter
report_checks

sta::rebuffer_instance [get_cell r1] $buffer_cell $wire_res_per_meter $wire_cap_per_meter
report_checks

set def_file [make_result_file rebuffer1.def]
write_def $def_file
report_file $def_file
