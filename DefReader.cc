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
#include "defrReader.hpp"

// Use Cadence DEF parser to build ConcreteNetwork based objects.

namespace sta {

static void
registerDefCallbacks();
static int
defDividerCbk(defrCallbackType_e,
	      const char *divider,
	      defiUserData user);
static int
defComponentCbk(defrCallbackType_e,
		defiComponent *def_component,
		defiUserData user);
static int
defNetCbk(defrCallbackType_e,
	  defiNet *def_net,
	  defiUserData user);
static int
defPinCbk(defrCallbackType_e,
	  defiPin *def_pin,
	  defiUserData user);
static int
defPinEndCbk(defrCallbackType_e,
	     void *,
	     defiUserData user);
static const char *
defToSta(const char *token,
	 Network *network);

////////////////////////////////////////////////////////////////

// DEF parser callback routine state.
class DefReader
{
public:
  DefReader(bool save_def_data,
	    LefDefNetwork *network);
  bool saveDefData() { return save_def_data_; }
  LefDefNetwork *network() { return network_; }

private:
  bool save_def_data_;
  LefDefNetwork *network_;
};

void
readDef(const char *filename,
	bool save_def_data,
	LefDefNetwork *network)
{
  network->setFilename(filename);
  // Make top_instance to act as parent to components.
  // Note that top ports are not known yet because PINS section has not been parsed.
  Library *lef_library = network->lefLibrary();
  Cell *top_cell = network->makeCell(lef_library, "top", false, filename);
  Instance *top_instance = network->makeInstance(top_cell, "", nullptr);
  network->setTopInstance(top_instance);

  defrInitSession();
  registerDefCallbacks();
  DefReader reader(save_def_data, network);
  FILE *stream = fopen(filename, "r");
  if (stream) {
    bool case_sensitive = true;
    defrRead(stream, filename, &reader, case_sensitive);
    defrClear();
    fclose(stream);
  }
  else
    throw FileNotReadable(filename);
}

static void
registerDefCallbacks()
{
  defrSetDividerCbk(defDividerCbk);
  defrSetComponentCbk(defComponentCbk);
  defrSetNetCbk(defNetCbk);
  defrSetPinCbk(defPinCbk);
  defrSetPinEndCbk(defPinEndCbk);
}

DefReader::DefReader(bool save_def_data,
		     LefDefNetwork *network) :
  save_def_data_(save_def_data),
  network_(network)
{
}

#define getDefReader(user) (reinterpret_cast<DefReader *>(user))
#define getNetwork(user) (getDefReader(user)->network())
#define saveDefData(user) (getDefReader(user)->saveDefData())

static int
defDividerCbk(defrCallbackType_e,
	      const char *divider,
	      defiUserData user)
{
  LefDefNetwork *network = getNetwork(user);
  network->setDivider(divider[0]);
  return 0;
}

static int
defComponentCbk(defrCallbackType_e,
		defiComponent *def_component,
		defiUserData user)
{
  LefDefNetwork *network = getNetwork(user);
  Library *lef_lib = network->lefLibrary();
  const char *def_name = def_component->id();
  const char *sta_name = defToSta(def_name, network);
  const char *macro_name = def_component->name();
  Cell *cell = network->findCell(lef_lib, macro_name);
  if (cell)
    network->makeDefComponent(cell, sta_name,
			      saveDefData(user) ? def_component : nullptr);
  else
    printf("Error: component %s macro %s not found.\n", def_name, macro_name);
  return 0;
}

static int
defPinCbk(defrCallbackType_e,
	  defiPin *def_pin,
	  defiUserData user)
{
  LefDefNetwork *network = getNetwork(user);
  const char *pin_name = def_pin->pinName();
  Cell *top_cell = network->cell(network->topInstance());
  Port *port = network->makePort(top_cell, pin_name);

  PortDirection *dir = PortDirection::unknown();
  if (def_pin->hasDirection()) {
    const char *def_dir = def_pin->direction();
    if (stringEq(def_dir, "INPUT"))
      dir = PortDirection::input();
    else if (stringEq(def_dir, "OUTPUT"))
      dir = PortDirection::output();
    else if (stringEq(def_dir, "INOUT"))
      dir = PortDirection::bidirect();
  }
  network->setDirection(port, dir);
  return 0;
}

// Finished PINS section so all of the top instance ports are defined.
// Now top_instance::initPins() can be called.
static int
defPinEndCbk(defrCallbackType_e,
	     void *,
	     defiUserData user)
{
  LefDefNetwork *network = getNetwork(user);
  network->initTopInstancePins();
  return 0;
}

static int
defNetCbk(defrCallbackType_e,
	  defiNet *def_net,
	  defiUserData user)
{
  LefDefNetwork *network = getNetwork(user);
  const char *def_net_name = def_net->name();
  const char *sta_net_name = defToSta(def_net_name, network);
  Net *net = network->makeNet(sta_net_name, saveDefData(user) ? def_net : nullptr);
  for (int i = 0; i < def_net->numConnections(); i++) {
    const char *def_inst_name = def_net->instance(i);
    const char *sta_inst_name = defToSta(def_inst_name, network);
    const char *pin_name = def_net->pin(i);
    if (stringEq(def_inst_name, "PIN")) {
      Instance *top_inst = network->topInstance();
      Pin *pin = network->findPin(top_inst, pin_name);
      if (pin == nullptr) {
	Cell *cell = network->cell(top_inst);
	Port *port = network->findPort(cell, pin_name);
	if (port)
	  pin = network->makePin(top_inst, port, nullptr);
	else
	  printf("Error: net %s connection to PIN %s not found\n",
		 def_net_name,
		 pin_name);
      }
      if (pin)
	network->makeTerm(pin, net);
    }
    else {
      Instance *inst = network->findInstance(sta_inst_name);
      if (inst) {
	Cell *cell = network->cell(inst);
	Port *port = network->findPort(cell, pin_name);
	if (port)
	  network->connect(inst, port, net);
	else
	  printf("Error: net %s connection to component %s/%s pin %s not found.\n",
		 def_net_name,
		 def_inst_name,
		 network->name(cell),
		 pin_name);
      }
      else
	printf("Error: net %s connection component %s not found.\n",
	       def_net_name,
	       def_inst_name);
    }
  }
  return 0;
}

// Escape path dividers in token.
static const char *
defToSta(const char *token,
	 Network *network)
{
  char path_escape = network->pathEscape();
  char path_divider = network->pathDivider();
  char *escaped = makeTmpString(strlen(token) + 1);
  char *e = escaped;
  for (const char *s = token; *s ; s++) {
    char ch = *s;

    if (ch == path_divider) {
      // Insert escape for divider.
      *e++ = path_escape;
      *e++ = path_divider;
    }
    else
      // Just the normal noises.
      *e++ = ch;
  }
  *e = '\0';
  return escaped;
}

} // namespace
