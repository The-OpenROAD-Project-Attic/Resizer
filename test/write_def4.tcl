# write_def from hierarchical verilog
source helpers.tcl
read_liberty liberty1.lib
read_verilog write_def4.v
link_design top

set def_file [make_result_file write_def4.def]
# missing -site
set_design_size \
  -die {0 0 10000 10000} \
  -core {100 100 9000 9000}
write_def -units 100 \
  -auto_place_pins \
  -sort \
  $def_file

# check that we can read the def
read_lef liberty1.lef
read_def $def_file

report_file $def_file
