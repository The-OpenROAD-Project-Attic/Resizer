# 
source helpers.tcl
read_liberty liberty1.lib
read_lef liberty1.lef
read_def reg3.def
create_clock -period 10 clk
set_input_delay -clock clk 0 {in1 in2}
resize
set def_file [make_result_file resize3.def]
write_def $def_file
report_file $def_file
