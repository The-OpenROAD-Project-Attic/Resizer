# make_parasitics
read_liberty liberty1.lib
read_lef liberty1.lef
read_def reg3.def
create_clock -period 10 clk
set_input_delay -clock clk 0 in1

report_checks

# Meters.
set lambda .12e-6
# Ohm/Square.
set m1_res_sq .08
# Farads/meter^2 (picofarads/micron^2).
set m1_area_cap 39e-6
# Farads/meter.
set m1_edge_cap 57e-12
# 4 lambda wide wire
# res/cap are per meter of wire length
set wire_cap [expr $m1_area_cap * $lambda * 4 + $m1_edge_cap * 2]
set wire_res [expr $m1_res_sq / ($lambda * 4)]
set_wire_rc -resistance $wire_res -capacitance $wire_cap

report_checks
