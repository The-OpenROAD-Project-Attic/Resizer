# verilog2def
source "helpers.tcl"
set def_file [make_result_file write_def3.def]
exec ../build/verilog2def \
  -lef liberty1.lef \
  -liberty liberty1.lib \
  -verilog reg1.v \
  -top_module top \
  -units 100 \
  -die_area "0 0 1000 1000" \
  -core_area "100 100 900 900" \
  -site site1 \
  -auto_place_pins \
  -def $def_file
report_file $def_file
