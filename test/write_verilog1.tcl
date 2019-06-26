# write_verilog
source "helpers.tcl"
source -echo read_reg1.tcl

set verilog_file [make_result_file write_verilog1.v]
write_verilog  $verilog_file
report_file $verilog_file

read_verilog  $verilog_file
link_design reg1
