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

#include "Machine.hh"
#include "Error.hh"
#include "PortDirection.hh"
#include "Liberty.hh"
#include "LefDefNetwork.hh"
#include "lefrReader.hpp"

// Use Cadence LEF parser to build ConcreteLibrary based objects.

namespace sta {

class LefReader;

static void
registerLefCallbacks();
static int
manufacturingGridCbk(lefrCallbackType_e,
		     double grid,
		     lefiUserData user);
static int
macroBeginCbk(lefrCallbackType_e,
	      const char *macro_name,
	      lefiUserData user);
static int
macroCbk(lefrCallbackType_e,
	 lefiMacro *macro,
	 lefiUserData user);
static int
macroEndCbk(lefrCallbackType_e,
	    const char *,
	    lefiUserData user);
static int
lefPinCbk(lefrCallbackType_e,
	  lefiPin *pin,
	  lefiUserData user);
static int
lefSiteCbk(lefrCallbackType_e,
	   lefiSite *site,
	   lefiUserData user);
static int
lefLayerCbk(lefrCallbackType_e,
	    lefiLayer *layer,
	    lefiUserData user);

// LEF parser callback routine state.
class LefReader
{
public:
  LefReader(const char *filename,
	    Library *lef_library,
	    LefDefNetwork *network);
  const char *filename() { return filename_; }
  LefDefNetwork *network() { return network_; }
  Library *lefLibrary() { return lef_library_; }
  Cell *lefMacro() { return lef_macro_; }
  void setLefMacro(Cell *lef_macro);

private:
  const char *filename_;
  Library *lef_library_;
  LefDefNetwork *network_;
  Cell *lef_macro_;
};

void
LefReader::setLefMacro(Cell *lef_macro)
{
  lef_macro_ = lef_macro;
}

void
readLef(const char *filename,
	LefDefNetwork *network)
{
  lefrInitSession();
  registerLefCallbacks();

  Library *lef_library = network->lefLibrary();
  if (lef_library == nullptr)
    lef_library = network->makeLefLibrary("LEF", filename);
  LefReader reader(filename, lef_library, network);
  FILE *stream = fopen(filename, "r");
  if (stream) {
    lefrRead(stream, filename, &reader);
    lefrClear();
    fclose(stream);
  }
  else
    throw FileNotReadable(filename);
}

static void
registerLefCallbacks()
{
  lefrSetManufacturingCbk(manufacturingGridCbk);
  lefrSetSiteCbk(lefSiteCbk);
  lefrSetLayerCbk(lefLayerCbk);
  lefrSetMacroBeginCbk(macroBeginCbk);
  lefrSetMacroEndCbk(macroEndCbk);
  lefrSetMacroCbk(macroCbk);
  lefrSetPinCbk(lefPinCbk);
}

LefReader::LefReader(const char *filename,
		     Library *lef_library,
		     LefDefNetwork *network) :
  filename_(filename),
  lef_library_(lef_library),
  network_(network),
  lef_macro_(nullptr)
{
}

#define getLefReader(user) (reinterpret_cast<LefReader *>(user))

static int
manufacturingGridCbk(lefrCallbackType_e,
		     double grid,
		     lefiUserData user)
{
  LefReader *reader = getLefReader(user);
  LefDefNetwork *network = reader->network();
  network->setManufacturingGrid(grid);
  return 0;
}

static int
macroBeginCbk(lefrCallbackType_e,
	      const char *macro_name,
	      lefiUserData user)
{
  LefReader *reader = getLefReader(user);
  LefDefNetwork *network = reader->network();
  Library *lef_lib = reader->lefLibrary();
  Cell *lef_macro = network->makeCell(lef_lib, macro_name, true,
				      reader->filename());
  reader->setLefMacro(lef_macro);
  return 0;
}

static int
macroEndCbk(lefrCallbackType_e,
	    const char *,
	    lefiUserData user)
{
  LefReader *reader = getLefReader(user);
  LefDefNetwork *network = reader->network();
  Cell *cell = reader->lefMacro();
  // Group bus bits into bus ports.
  network->groupBusPorts(cell);
  // Set corresponding liberty cell and ports for reference by Network.
  LibertyCell *lib_cell = network->findLibertyCell(network->name(cell));
  if (lib_cell) {
    ConcreteCell *ccell = reinterpret_cast<ConcreteCell*>(cell);
    ccell->setLibertyCell(lib_cell);
    CellPortIterator *port_iter = network->portIterator(cell);
    while (port_iter->hasNext()) {
      Port *port = port_iter->next();
      const char *port_name = network->name(port);
      LibertyPort *lib_port = lib_cell->findLibertyPort(port_name);
      if (lib_port) {
	ConcretePort *cport = reinterpret_cast<ConcretePort*>(port);
	cport->setLibertyPort(lib_port);

	if (network->isBus(port)) {
	  PortMemberIterator *member_iter = network->memberIterator(port);
	  while (member_iter->hasNext()) {
	    Port *member = member_iter->next();
	    const char *member_name = network->name(member);
	    LibertyPort *member_lport = lib_cell->findLibertyPort(member_name);
	    if (member_lport) {
	      ConcretePort *member_cport = reinterpret_cast<ConcretePort*>(member);
	      member_cport->setLibertyPort(member_lport);
	    }
	  }
	  delete member_iter;
	}
      }
    }
    delete port_iter;
  }
  reader->setLefMacro(nullptr);
  return 0;
}

static int
macroCbk(lefrCallbackType_e,
	 lefiMacro *lef_macro,
	 lefiUserData user)
{
  // Save lef macro data.
  LefReader *reader = getLefReader(user);
  LefDefNetwork *network = reader->network();
  Cell *cell = reader->lefMacro();
  network->setLefMacro(cell, new lefiMacro(*lef_macro));
  return 0;
}

static int
lefPinCbk(lefrCallbackType_e,
	  lefiPin *lpin,
	  lefiUserData user)
{
  LefReader *reader = getLefReader(user);
  LefDefNetwork *network = reader->network();
  PortDirection *dir = PortDirection::unknown();
  if (lpin->hasDirection()) {
    const char *ldir = lpin->direction();
    if (stringEq(ldir, "INPUT"))
      dir = PortDirection::input();
    else if (stringEq(ldir, "OUTPUT"))
      dir = PortDirection::output();
    else if (stringEq(ldir, "OUTPUT TRISTATE"))
      dir = PortDirection::tristate();
    else if (stringEq(ldir, "INOUT"))
      dir = PortDirection::bidirect();
  }
  if (lpin->hasUse()) {
    const char *use = lpin->use();
    if (stringEq(use, "POWER"))
      dir = PortDirection::power();
    else if (stringEq(use, "GROUND"))
      dir = PortDirection::ground();
  }
  Port *port = network->makePort(reader->lefMacro(), lpin->name());
  network->setDirection(port, dir);
  return 0;
}

static int
lefSiteCbk(lefrCallbackType_e,
	   lefiSite *site,
	   lefiUserData user)
{
  LefReader *reader = getLefReader(user);
  LefDefNetwork *network = reader->network();
  network->makeLefSite(site);
  return 0;
}

static int
lefLayerCbk(lefrCallbackType_e,
	    lefiLayer *layer,
	    lefiUserData user)
{
  LefReader *reader = getLefReader(user);
  LefDefNetwork *network = reader->network();
  network->makeLefLayer(layer);
  return 0;
}

} // namespace
