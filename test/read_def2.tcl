# read def with hierarchical instance,net names
read_liberty liberty1.lib
read_lef liberty1.lef
read_def reg2.def
report_object_full_names [get_cells h1/r3]
report_object_full_names [get_nets h2/u2z]
