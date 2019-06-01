# def buffer upsize, insertion
source helpers.tcl
source -echo read_def1.tcl
# buffer upsize
replace_cell u1 snl_bufx2
report_checks -fields input_pin

# buffer insertion
insert_buffer b1 snl_bufx1 u2z r3/D b1z
report_checks -fields input_pin

set def_file [make_result_file write_def2.def]
write_def $def_file
report_file $def_file
