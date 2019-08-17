# write_def reg1.v (no lef)
source helpers.tcl
read_liberty liberty1.lib
read_lef liberty1.lef
read_verilog reg1.v
link_design top

set utilization .3
# height / width
set aspect_ratio 0.5
set core_space 2

set design_area [sta::area_sta_ui [sta::design_area]]
set core_area [expr $design_area / $utilization]
set core_width [expr sqrt($core_area / $aspect_ratio)]
set core_height [expr $core_width * $aspect_ratio]
set core_lx $core_space
set core_ly $core_space
set core_ux [expr $core_space + $core_width]
set core_uy [expr $core_space + $core_height]

set die_lx 0
set die_ly 0
set die_ux [expr $core_width + $core_space * 2]
set die_uy [expr $core_height + $core_space * 2]

set def_file [make_result_file write_def7.def]
set_design_size \
  -die "$die_lx $die_ly $die_ux $die_uy" \
  -core "$core_lx $core_ly $core_ux $core_uy"
write_def -units 100 \
  -site site1 \
  -auto_place_pins \
  -sort \
  $def_file
report_file $def_file
