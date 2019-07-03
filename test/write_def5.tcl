# write_def -site reg1.v
source "helpers.tcl"
read_liberty liberty1.lib
read_lef liberty1.lef
read_verilog reg1.v
link_design top

set def_file [make_result_file write_def5.def]
set_design_size \
  -die {0 0 1000 1000} \
  -core {100 100 900 900}
write_def -units 100 \
  -site site1 \
  -auto_place_pins \
  -sort \
  $def_file
report_file $def_file
