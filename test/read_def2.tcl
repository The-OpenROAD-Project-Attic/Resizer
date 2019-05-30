# def buffer upsize, insertion
source -echo read_def1.tcl
# buffer upsize
replace_cell u1 snl_bufx2
report_checks -fields input_pin
# buffer insertion
insert_buffer b1 snl_bufx1 u2z r3/D b1z
report_checks -fields input_pin
