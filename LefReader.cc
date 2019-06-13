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
#include "LefDefNetwork.hh"
#include "lefrReader.hpp"

// Use Cadence LEF parser to build ConcreteLibrary based objects.

namespace sta {

class LefReader;

static void
registerLefCallbacks();
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

// LEF parser callback routine state.
class LefReader
{
public:
  LefReader(const char *filename,
	    bool save_lef_data,
	    Library *lef_library,
	    LefDefNetwork *network);
  const char *filename() { return filename_; }
  bool saveLefData() { return save_lef_data_; }
  LefDefNetwork *network() { return network_; }
  Library *lefLibrary() { return lef_library_; }
  Cell *lefMacro() { return lef_macro_; }
  void setLefMacro(Cell *lef_macro);

private:
  const char *filename_;
  bool save_lef_data_;
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
	bool save_lef_data,
	LefDefNetwork *network)
{
  lefrInitSession();
  registerLefCallbacks();

  Library *lef_library = network->lefLibrary();
  if (lef_library == nullptr)
    lef_library = network->makeLibrary("LEF", filename);
  LefReader reader(filename, save_lef_data, lef_library, network);
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
  lefrSetMacroBeginCbk(macroBeginCbk);
  lefrSetMacroEndCbk(macroEndCbk);
  lefrSetMacroCbk(macroCbk);
  lefrSetPinCbk(lefPinCbk);
}

LefReader::LefReader(const char *filename,
		     bool save_lef_data,
		     Library *lef_library,
		     LefDefNetwork *network) :
  filename_(filename),
  save_lef_data_(save_lef_data),
  lef_library_(lef_library),
  network_(network),
  lef_macro_(nullptr)
{
}

#define getLefReader(user) (reinterpret_cast<LefReader *>(user))
#define saveLefData(user) (getLefReader(user)->saveLefData())

static int
macroBeginCbk(lefrCallbackType_e,
	      const char *macro_name,
	      lefiUserData user)
{
  LefReader *reader = getLefReader(user);
  LefDefNetwork *network = reader->network();
  Library *lef_lib = reader->lefLibrary();
  Cell *lef_macro = network->makeCell(lef_lib, macro_name, true, reader->filename());
  reader->setLefMacro(lef_macro);
  return 0;
}

static int
macroEndCbk(lefrCallbackType_e,
	    const char *,
	    lefiUserData user)
{
  LefReader *reader = getLefReader(user);
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
  if (saveLefData(user)) {
    Cell *cell = reader->lefMacro();
    LefMacro *macro = reinterpret_cast<LefMacro*>(cell);
    macro->setLefMacro(lef_macro);
  }
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

} // namespace
