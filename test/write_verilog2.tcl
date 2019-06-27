# write_verilog reg1 with bus
source "helpers.tcl"
read_liberty liberty1.lib
read_lef liberty1.lef
read_def reg4.def

set verilog_file [make_result_file write_verilog2.v]
write_verilog  $verilog_file
report_file $verilog_file

read_verilog  $verilog_file
link_design reg1
