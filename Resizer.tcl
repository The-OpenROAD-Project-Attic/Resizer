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

# Defined by SWIG interface LefDef.i.
define_cmd_args "read_lef" {filename}

define_cmd_args "read_def" {filename}

define_cmd_args "write_def" {filename}

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
			    [-buffer_cell buffer_cell]}

proc resize { args } {
  parse_key_args "resize" args \
    keys {-buffer_cell} flags {-resize -repair_max_cap -repair_max_slew}

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

  check_argc_eq0 "resize" $args

  resize_cmd $resize $repair_max_cap $repair_max_slew $buffer_cell
}

# sta namespace end
}
