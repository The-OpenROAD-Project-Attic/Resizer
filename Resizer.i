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
%include "StaTcl.i"
%include "NetworkEdit.i"
%include "DelayCalc.i"
%include "Parasitics.i"
%include "Verilog.i"

%{

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

void
read_lef(const char *filename)
{
  Sta *sta = Sta::sta();
  LefDefNetwork *network = dynamic_cast<LefDefNetwork*>(sta->network());
  readLef(filename, true, network);
}

void
read_def(const char *filename)
{
  Sta *sta = Sta::sta();
  LefDefNetwork *network = dynamic_cast<LefDefNetwork*>(sta->network());
  readDef(filename, true, network);
}

void
write_def(const char *filename)
{
  Resizer *resizer = static_cast<Resizer*>(Sta::sta());
  LefDefNetwork *network = static_cast<LefDefNetwork*>(resizer->network());
  writeDef(filename, network);
}

void
resize()
{
  Resizer *resizer = static_cast<Resizer*>(Sta::sta());
  resizer->resize(resizer->cmdCorner());
}

void
resize_to_target_slew(Instance *inst)
{
  Resizer *resizer = static_cast<Resizer*>(Sta::sta());
  resizer->resizeToTargetSlew(inst, resizer->cmdCorner());
}

%} // inline