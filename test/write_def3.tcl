# write def from verilog
source helpers.tcl
read_liberty liberty1.lib
read_verilog reg1.v
link_design top

set def_file [make_result_file write_def3.def]
write_def -units 2000 -die_area {0 0 10000 10000} -auto_place_pins $def_file

# check that we can read and time the def
read_lef liberty1.lef
read_def results/write_def3.def
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
report_checks

report_file $def_file
