// Resizer, LEF/DEF gate resizer
// Copyright (c) 2019, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef RESIZER_DEF_WRITER_H
#define RESIZER_DEF_WRITER_H

namespace sta {

class LefDefNetwork;

void
writeDef(const char *filename,
	 // These args are only for writing DEF from Verilog.
	 int units,
	 // Die area (in meters).
	 double die_lx,
	 double die_ly,
	 double die_ux,
	 double die_uy,
	 // Core area (in meters).
	 double core_lx,
	 double core_ly,
	 double core_ux,
	 double core_uy,
	 // LEF site name to use for ROWS.
	 const char *site_name,
	 // Routing track info filename.
	 const char *tracks_file,
	 // Place pins around the die area boundary.
	 bool auto_place_pins,
	 bool sort,
	 LefDefNetwork *network);

} // namespace
#endif
