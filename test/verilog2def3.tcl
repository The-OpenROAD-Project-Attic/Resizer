# verilog2def -utilization -aspect_ratio -core_space
source "helpers.tcl"
set def_file [make_result_file verilog2def3.def]
exec ../build/verilog2def \
  -lef liberty1.lef \
  -liberty liberty1.lib \
  -verilog reg1.v \
  -top_module top \
  -units 100 \
  -utilization 30 \
  -aspect_ratio 0.5 \
  -core_space 2 \
  -site site1 \
  -tracks write_def6.tracks \
  -auto_place_pins \
  -def $def_file
report_file $def_file
