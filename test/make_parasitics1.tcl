# make_parasitics
read_liberty liberty1.lib
read_lef liberty1.lef
read_def reg3.def
create_clock -period 10 clk
set_input_delay -clock clk 0 {in1 in2}

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
set wire_cap_per_meter [expr $m1_area_cap * $lambda * 4 + $m1_edge_cap * 2]
set wire_res_per_meter [expr $m1_res_sq / ($lambda * 4)]
sta::make_parasitics $wire_cap_per_meter $wire_res_per_meter

report_checks
