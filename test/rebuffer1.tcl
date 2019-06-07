# rebuffer
read_liberty nlc18.lib
read_lef nlc18.lef
read_def reg4.def
create_clock clk -period 1

# use huge wire cap to tickle buffer insertion
set wire_res_per_meter 1.7e+5
set wire_cap_per_meter 1.3e-8

sta::make_net_parasitics $wire_res_per_meter $wire_cap_per_meter
report_checks

sta::rebuffer_instance [get_cell r1] [get_lib_cell nlc18_worst/snl_bufx2] $wire_res_per_meter $wire_cap_per_meter
report_checks
