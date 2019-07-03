# resize reg1 resize/rebuffer design area
read_liberty nlc18.lib
read_lef nlc18.lef
read_def rebuffer2.def
create_clock clk -period 1

set buffer_cell [get_lib_cell nlc18/snl_bufx2]
# use 100x wire cap to tickle buffer insertion
set_wire_rc -resistance 1.7e+5 -capacitance 1.3e-8

puts "design area [format %.0f [expr [design_area] *1e12]] u^2"

resize -resize
puts "design area [format %.0f [expr [design_area] *1e12]] u^2"

resize -repair_max_cap -repair_max_slew -buffer_cell $buffer_cell
puts "design area [format %.0f [expr [design_area] *1e12]] u^2"
