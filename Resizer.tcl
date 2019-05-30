# OpenSTA, Static Timing Analyzer
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

proc read_def { filename } {
  if { ![lef_exists] } {
    sta_error "use LEF file has not been read.\n"
  }
  if { ![file readable $filename] } {
    sta_error "$filename is not readable.\n"
  }
  read_def_cmd $filename
}

define_cmd_args "write_def" {filename}

proc write_def { filename } {
  if { ![file writable $filename] } {
    sta_error "$filename is not writeable.\n"
  }
  write_def_cmd $filename
}

# Defined by SWIG interface LefDef.i.
define_cmd_args "resize" {}

# sta namespace end
}
