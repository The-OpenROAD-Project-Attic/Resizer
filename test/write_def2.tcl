# write_def after buffer insertion
source helpers.tcl
source -echo read_def1.tcl
insert_buffer b1 snl_bufx1 u2z r3/D b1z
set def_file [make_result_file write_def2.def]
write_def $def_file
report_file $def_file
