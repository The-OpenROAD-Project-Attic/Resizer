# resize to target_slew
read_liberty nlc18.lib
read_lef nlc18.lef
read_def reg1.def
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
set_load .3 u1z
sta::resize_to_target_slew [get_cell u1]
report_instance u1
