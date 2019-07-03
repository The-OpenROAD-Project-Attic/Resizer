# Resizer, LEF/DEF gate resizer
# Copyright (c) 2019, Parallax Software, Inc.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

namespace eval sta {

# Defined by SWIG interface Resizer.i.
define_cmd_args "set_dont_use" {cell dont_use}

# Defined by SWIG interface Resizer.i
define_cmd_args "design_area" {}

# Defined by SWIG interface Resizer.i
define_cmd_args "read_lef" {filename}

define_cmd_args "read_def" {filename}

define_cmd_args "set_design_size" {[-die {lx ly ux uy}]\
				     [-core {lx ly ux uy}]}

proc set_design_size { args } {
  parse_key_args "set_design_size" args keys {-die -core} flags {}

  set die_lx 0
  set die_ly 0
  set die_ux 0
  set die_uy 0
  if [info exists keys(-die)] {
    set die $keys(-die)
    if { [llength $die] != 4 } {
      sta_error "-die_area is a list of 4 coordinates."
    }
    lassign $die die_lx die_ly die_ux die_uy
    check_positive_float "-die" $die_lx
    check_positive_float "-die" $die_ly
    check_positive_float "-die" $die_ux
    check_positive_float "-die" $die_uy
  }

  set core_lx 0
  set core_ly 0
  set core_ux 0
  set core_uy 0
  if [info exists keys(-core)] {
    set core $keys(-core)
    if { [llength $core] != 4 } {
      sta_error "-core_area is a list of 4 coordinates."
    }
    lassign $core core_lx core_ly core_ux core_uy
    check_positive_float "-core" $core_lx
    check_positive_float "-core" $core_ly
    check_positive_float "-core" $core_ux
    check_positive_float "-core" $core_uy
  }
  set_design_size_cmd \
    [expr $die_lx * 1e-6] [expr $die_ly * 1e-6] \
    [expr $die_ux * 1e-6] [expr $die_uy * 1e-6] \
    [expr $core_lx * 1e-6] [expr $core_ly * 1e-6] \
    [expr $core_ux * 1e-6] [expr $core_uy * 1e-6]
}

define_cmd_args "write_def" {-units def_units\
			       [-die_area {lx ly ux uy}]\
			       [-core_area {lx ly ux uy}]\
			       [-site site_name]\
			       [-tracks tracks_file]\
			       [-auto_place_pins]\
			       [-sort]\
			       filename}

proc write_def { args } {
  parse_key_args "write_def" args \
    keys {-units -die_area -core_area -site -tracks} \
    flags {-auto_place_pins -sort}

  set units 1000
  if [info exists keys(-units)] {
    set units $keys(-units)
    check_positive_integer "-units" $units
  }

  set die_lx 0
  set die_ly 0
  set die_ux 0
  set die_uy 0
  set have_die 0
  if [info exists keys(-die_area)] {
    sta_warn "Warning: write_def -die_area deprecated. Use the set_design_size command."
    set die_area $keys(-die_area)
    if { [llength $die_area] != 4 } {
      sta_error "-die_area is a list of 4 coordinates."
    }
    lassign $die_area die_lx die_ly die_ux die_uy
    check_positive_float "-die_area" $die_lx
    check_positive_float "-die_area" $die_ly
    check_positive_float "-die_area" $die_ux
    check_positive_float "-die_area" $die_uy
    set have_die 1
  }

  set core_lx 0
  set core_ly 0
  set core_ux 0
  set core_uy 0
  set have_core 0
  if [info exists keys(-core_area)] {
    sta_warn "Warning: write_def -core_area deprecated. Use the set_design_size command."
    set core_area $keys(-core_area)
    if { [llength $core_area] != 4 } {
      sta_error "-core_area is a list of 4 coordinates."
    }
    lassign $core_area core_lx core_ly core_ux core_uy
    check_positive_float "-core_area" $core_lx
    check_positive_float "-core_area" $core_ly
    check_positive_float "-core_area" $core_ux
    check_positive_float "-core_area" $core_uy
    set have_core 1
  }
  if { $have_die || $have_core } {
    set_design_size_cmd \
      [expr $die_lx * 1e-6] [expr $die_ly * 1e-6] \
      [expr $die_ux * 1e-6] [expr $die_uy * 1e-6] \
      [expr $core_lx * 1e-6] [expr $core_ly * 1e-6] \
      [expr $core_ux * 1e-6] [expr $core_uy * 1e-6]
  }

  set site_name ""
  if [info exists keys(-site)] {
    set site_name $keys(-site)
  }

  set tracks_file ""
  if { [info exists keys(-tracks)] } {
    set tracks_file $keys(-tracks)
  }

  set auto_place_pins [info exists flags(-auto_place_pins)]

  set sort [info exists flags(-sort)]

  check_argc_eq1 "write_def" $args
  set filename $args

  # convert die coordinates to meters.
  write_def_cmd $filename $units \
    $site_name $tracks_file $auto_place_pins $sort
}

define_cmd_args "set_wire_rc" {[-resistance res ][-capacitance cap]\
				 [-corner corner_name]}

proc set_wire_rc { args } {
   parse_key_args "set_wire_rc" args \
    keys {-resistance -capacitance -corner} flags {}

  set wire_res 0.0
  if [info exists keys(-resistance)] {
    set res $keys(-resistance)
    check_positive_float "-resistance" $res
  }
  set wire_cap 0.0
  if [info exists keys(-capacitance)] {
    set cap $keys(-capacitance)
    check_positive_float "-capacitance" $cap
  }
  set corner [parse_corner keys]
  check_argc_eq0 "set_wire_rc" $args
  set_wire_rc_cmd $res $cap $corner
}

define_cmd_args "resize" {[-resize]\
			    [-repair_max_cap]\
			    [-repair_max_slew]\
			    [-resize_libraries resize_libs]\
			    [-buffer_cell buffer_cell]\
			    [-dont_use lib_cells]}

proc resize { args } {
  parse_key_args "resize" args \
    keys {-buffer_cell -resize_libraries -dont_use -max_utilization} \
    flags {-resize -repair_max_cap -repair_max_slew}

  set resize [info exists flags(-resize)]
  set repair_max_cap [info exists flags(-repair_max_cap)]
  set repair_max_slew [info exists flags(-repair_max_slew)]
  if { !($resize || $repair_max_cap || $repair_max_slew) } {
    set resize 1
    set repair_max_cap 1
    set repair_max_slew 1
  }
  set buffer_cell "NULL"
  if { [info exists keys(-buffer_cell)] } {
    set buffer_cell_name $keys(-buffer_cell)
    # check for -buffer_cell [get_lib_cell arg] return ""
    if { $buffer_cell_name != "" } {
      set buffer_cell [get_lib_cell_error "-buffer_cell" $buffer_cell_name]
      if { $buffer_cell != "NULL" } {
	if { ![get_property $buffer_cell is_buffer] } {
	  sta_error "Error: [get_name $buffer_cell] is not a buffer."
	}
      }
    }
  }
  if { $buffer_cell == "NULL" && ($repair_max_cap || $repair_max_slew) } {
    sta_error "Error: resize -buffer_cell required for buffer insertion."
  }

  if { [info exists keys(-resize_libraries)] } {
    set resize_libs [get_liberty_error "-resize_libraries" $keys(-resize_libraries)]
  } else {
    set resize_libs [get_libs *]
  }

  set dont_use {}
  if { [info exists keys(-dont_use)] } {
    set dont_use [get_lib_cells -quiet $keys(-dont_use)]
  }

  set max_util 1.0
  if { [info exists keys(-max_utilization)] } {
    set max_util $keys(-max_utilization)
    if {!([string is double $max_util] && $max_util >= 0.0 && $max_util <= 100)} {
      sta_error "-max_utilization must be between 0 and 100%."
    }
    max_util [expr $max_util / 100.0]
  }

  check_argc_eq0 "resize" $args

  resize_cmd $resize $repair_max_cap $repair_max_slew $buffer_cell \
    $resize_libs $dont_use $max_util
}

define_cmd_args "get_pin_net" {pin_name}

proc get_pin_net { pin_name } {
  set pin [get_pin_error "pin_name" $pin_name]
  return [$pin net]
}

# sta namespace end
}
