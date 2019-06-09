# resize reg1
source helpers.tcl
read_liberty nlc18.lib
read_lef nlc18.lef
read_def reg1.def

create_clock -name clk -period 1 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
# no placement, so add a load
set_load .2 u1z

report_checks
resize
report_checks

set def_file [make_result_file resize2.def]
write_def $def_file
report_file $def_file
