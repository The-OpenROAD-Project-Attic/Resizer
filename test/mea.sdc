create_clock -name sclk -period 20 SCLK
set_input_delay -clock sclk 0 [delete_from_list [all_inputs] "SCLK"]
set_output_delay -clock sclk 0 [all_outputs]
