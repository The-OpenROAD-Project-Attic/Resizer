# resize mea
read_liberty nlc18.lib
read_lef nlc18.lef
read_def mea.def
source mea.sdc
report_checks
sta::resize
report_checks
