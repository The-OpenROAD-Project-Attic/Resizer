# write def from verilog
source helpers.tcl
read_liberty liberty1.lib
read_verilog write_def4.v
link_design top

set def_file [make_result_file write_def4.def]
write_def -units 100 -die_area {0 0 10000 10000} -auto_place_pins -sort $def_file

# check that we can read the def
read_lef liberty1.lef
read_def $def_file

report_file $def_file
