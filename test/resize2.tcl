# resize reg1
source helpers.tcl
read_liberty nlc18.lib
read_lef nlc18.lef
read_def reg1.def
resize
set def_file [make_result_file reg1_sized.def]
write_def $def_file
report_file $def_file
