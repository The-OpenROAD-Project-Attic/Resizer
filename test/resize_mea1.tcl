# resize mea
source helpers.tcl
read_liberty nlc18.lib
read_lef nlc18.lef
read_def mea.def
source mea.sdc

puts "TNS before = [format %.2f [total_negative_slack]]"
puts "WNS before = [format %.2f [worst_negative_slack]]"

resize

puts "TNS before = [format %.2f [total_negative_slack]]"
puts "WNS before = [format %.2f [worst_negative_slack]]"

set def_file [make_result_file reg1_sized.def]
write_def $def_file
