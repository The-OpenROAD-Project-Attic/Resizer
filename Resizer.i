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
  readLef(filename, true, network);
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
write_def(const char *filename)
{
  LefDefNetwork *network = lefDefNetwork();
  writeDef(filename, network);
}

void
resize_cmd(float wire_res_per_length, // Ohms/Meter
	   float wire_cap_per_length, // Farads/Meter
	   Corner *corner)
{
  Resizer *resizer = getResizer();
  resizer->resize(wire_res_per_length, wire_cap_per_length, corner);
}

void
resize_to_target_slew(Instance *inst)
{
  Resizer *resizer = getResizer();
  resizer->resizeToTargetSlew(inst, resizer->cmdCorner());
}

void
make_net_parasitics(float wire_cap_per_length,
		    float wire_res_per_length)
{
  Resizer *resizer = getResizer();
  Corner *corner = resizer->cmdCorner();
  resizer->makeNetParasitics(wire_cap_per_length, wire_res_per_length,
			     corner);
}

void
rebuffer(LibertyCell *buffer_cell,
	 float wire_res_per_length,
	 float wire_cap_per_length)
{
  Resizer *resizer = getResizer();
  Corner *corner = resizer->cmdCorner();
  resizer->rebuffer(true, true,
		    buffer_cell, wire_res_per_length,
		    wire_cap_per_length, corner);
}

void
rebuffer_instance(Instance *inst,
		  LibertyCell *buffer_cell,
		  float wire_res_per_length,
		  float wire_cap_per_length)
{
  Resizer *resizer = getResizer();
  Corner *corner = resizer->cmdCorner();
  resizer->rebuffer(inst, buffer_cell, wire_res_per_length,
		    wire_cap_per_length, corner);
}

%} // inline
