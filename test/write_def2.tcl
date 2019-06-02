# write_def with hierarchical instance,net names
source helpers.tcl
read_liberty liberty1.lib
read_lef liberty1.lef
read_def reg2.def
set def_file [make_result_file write_def2.def]
write_def $def_file
report_file $def_file
