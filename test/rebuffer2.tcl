# rebuffer 2 pin steiner with root branch other pt equal root
source "helpers.tcl"
read_liberty nlc18.lib
read_lef nlc18.lef
read_def reg3.def
create_clock clk -period 1

set wire_res_per_meter 1.7e+5
# use 100x wire cap to tickle buffer insertion
set wire_cap_per_meter 1.3e-8
set buffer_cell [get_lib_cell nlc18_worst/snl_bufx2]

sta::make_net_parasitics $wire_res_per_meter $wire_cap_per_meter
sta::rebuffer_instance [get_cell u1] $buffer_cell $wire_res_per_meter $wire_cap_per_meter
