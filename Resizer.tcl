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

define_cmd_args "resize" {[-wire_res_per_length res]\
			    [-wire_cap_per_length cap]\
			    [-corner corner_name]}

proc resize { args } {
  parse_key_args "resize" args \
    keys {-wire_res_per_length -wire_cap_per_length -corner} flags {}

  set wire_res_per_length 0.0
  if [info exists keys(-wire_res_per_length)] {
    set wire_res_per_length $keys(-wire_res_per_length)
    check_positive_float "-wire_res_per_length" $wire_res_per_length
  }
  set wire_cap_per_length 0.0
  if [info exists keys(-wire_cap_per_length)] {
    set wire_cap_per_length $keys(-wire_cap_per_length)
    check_positive_float "-wire_cap_per_length" $wire_cap_per_length
  }
  set corner [parse_corner keys]

  resize_cmd $wire_res_per_length $wire_cap_per_length $corner
}

# sta namespace end
}
