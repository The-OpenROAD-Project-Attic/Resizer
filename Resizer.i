%module resizer

// OpenSTA, Static Timing Analyzer
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

%include "StaException.i"

%{

#include "Machine.hh"
#include "Error.hh"
#include "LefReader.hh"
#include "DefReader.hh"
#include "DefWriter.hh"
#include "LefDefNetwork.hh"
#include "Resizer.hh"

using namespace sta;

%}

////////////////////////////////////////////////////////////////
//
// C++ functions visible as TCL functions.
//
////////////////////////////////////////////////////////////////

%inline %{

LefDefNetwork *
lefDefNetwork()
{
  Sta *sta = Sta::sta();
  return dynamic_cast<LefDefNetwork*>(sta->network());
}

Resizer *
getResizer()
{
  return static_cast<Resizer*>(Sta::sta());
}

void
read_lef(const char *filename)
{
  LefDefNetwork *network = lefDefNetwork();
  readLef(filename, network);
}

bool
lef_exists()
{
  LefDefNetwork *network = lefDefNetwork();
  return network->lefLibrary() != nullptr;
}

void
read_def(const char *filename)
{
  LefDefNetwork *network = lefDefNetwork();
  readDef(filename, true, network);
}

void
write_def_cmd(const char *filename,
	      int units,
	      // Die area.
	      double die_lx,
	      double die_ly,
	      double die_ux,
	      double die_uy,
	      // Core area.
	      double core_lx,
	      double core_ly,
	      double core_ux,
	      double core_uy,
	      const char *site_name,
	      bool auto_place_pins,
	      bool sort)
{
  LefDefNetwork *network = lefDefNetwork();
  if (site_name[0] == '\0')
    site_name = nullptr;
  writeDef(filename, units,
	   die_lx, die_ly, die_ux, die_uy,
	   core_lx, core_ly, core_ux, core_uy,
	   site_name, auto_place_pins, sort, network);
}

void
set_wire_rc_cmd(float res,
		float cap,
		Corner *corner)
{
  Resizer *resizer = getResizer();
  resizer->setWireRC(res, cap, corner);
}

void
resize_cmd(bool resize,
	   bool repair_max_cap,
	   bool repair_max_slew,
	   LibertyCell *buffer_cell)
{
  Resizer *resizer = getResizer();
  resizer->resize(resize, repair_max_cap, repair_max_slew, buffer_cell);
}

void
resize_to_target_slew(Instance *inst)
{
  Resizer *resizer = getResizer();
  resizer->resizeToTargetSlew(inst, resizer->cmdCorner());
}

void
rebuffer_net(Net *net,
	     LibertyCell *buffer_cell)
{
  Resizer *resizer = getResizer();
  resizer->rebuffer(net, buffer_cell);
}

%} // inline
