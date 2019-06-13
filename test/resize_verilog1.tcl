read_liberty liberty1.lib
read_verilog reg1.v
link_design top
sta::report_network
#resize -buffer_cell [get_lib_cell liberty1/snl_bufx2]
