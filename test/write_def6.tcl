# write_def -tracks reg1.v
source "helpers.tcl"
read_liberty liberty1.lib
read_lef liberty1.lef
read_verilog reg1.v
link_design top

set def_file [make_result_file write_def6.def]
write_def -units 100 \
  -die_area {0 0 1000 1000} \
  -core_area {100 100 900 900} \
  -site site1 \
  -tracks write_def6.tracks \
  -auto_place_pins \
  -sort \
  $def_file
report_file $def_file
