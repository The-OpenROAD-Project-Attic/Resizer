# read def with hierarchical instance,net names
source helpers.tcl
read_liberty liberty1.lib
read_lef liberty1.lef
read_def reg3.def
set def_file [make_result_file write_def3.def]
write_def $def_file
report_file $def_file
