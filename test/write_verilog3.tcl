# write_verilog from lef macro bus pins
source "helpers.tcl"
read_liberty liberty1.lib
read_liberty bus1.lib
read_lef bus1.lef
read_def bus1.def

set verilog_file [make_result_file write_verilog2.v]
write_verilog  $verilog_file
report_file $verilog_file

read_verilog  $verilog_file
link_design top
