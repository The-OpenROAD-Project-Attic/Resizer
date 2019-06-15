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

#include <stdio.h>
#include "Machine.hh"
#include "Error.hh"
#include "StringUtil.hh"
#include "PortDirection.hh"
#include "LefDefNetwork.hh"
#include "defiComponent.hpp"
#include "defiNet.hpp"
#include "DefWriter.hh"

namespace sta {

using std::abs;

static void
rewriteDef(const char *in_filename,
	   const char *filename,
	   LefDefNetwork *network);
static void
writeDefFresh(const char *filename,
	      int units,
	      // Die area.
	      int die_lx,
	      int die_ly,
	      int die_ux,
	      int die_uy,
	      bool auto_place_pins,
	      LefDefNetwork *network);
static void
writeDefComponents(FILE *out_stream,
		   LefDefNetwork *network);
static void
writeDefComponent(Instance *inst,
		  FILE *out_stream,
		  LefDefNetwork *network);
static void
writeDefPins(int die_lx,
	     int die_ly,
	     int die_ux,
	     int die_uy,
	     bool auto_place_pins,
	     FILE *out_stream,
	     LefDefNetwork *network);
static void
writeDefPin(Pin *pin,
	    bool is_placed,
	    int x,
	    int y,
	    const char *orient,	    
	    FILE *out_stream,
	    LefDefNetwork *network);
static const char *
staToDef(PortDirection *dir);
static void
writeDefNets(FILE *out_stream,
	     LefDefNetwork *network);
static void
writeDefNets(const Instance *inst,
	     FILE *out_stream,
	     LefDefNetwork *network);
static void
writeDefNet(Net *net,
	    FILE *out_stream,
	    LefDefNetwork *network);
static const char *
staToDef(const char *token,
	 Network *network);
static void
writeDefHeader(int units,
	       // Die area.
	       int die_lx,
	       int die_ly,
	       int die_ux,
	       int die_uy,
	       FILE *out_stream,
	       LefDefNetwork *network);

////////////////////////////////////////////////////////////////

void
writeDef(const char *filename,
	 int units,
	 // Die area.
	 int die_lx,
	 int die_ly,
	 int die_ux,
	 int die_uy,
	 bool auto_place_pins,
	 LefDefNetwork *network)
{
  const char *in_filename = network->defFilename();
  if (in_filename)
    rewriteDef(in_filename, filename, network);
  else
    writeDefFresh(filename, units, die_lx, die_ly, die_ux, die_uy,
		  auto_place_pins, network);
}

// Write a fresh DEF file from the network.
static void
writeDefFresh(const char *filename,
	      int units,
	      // Die area.
	      int die_lx,
	      int die_ly,
	      int die_ux,
	      int die_uy,
	      bool auto_place_pins,
	      LefDefNetwork *network)
{
  FILE *out_stream = fopen(filename, "w");
  if (out_stream) {
    writeDefHeader(units, die_lx, die_ly, die_ux, die_uy,
		   out_stream, network);
    writeDefComponents(out_stream, network);
    fprintf(out_stream, "\n");
    writeDefPins(die_lx, die_ly, die_ux, die_uy, auto_place_pins,
		 out_stream, network);
    fprintf(out_stream, "\n");
    writeDefNets(out_stream, network);
    fprintf(out_stream, "\nEND DESIGN\n");
    fclose(out_stream);
  }
  else
    throw FileNotWritable(filename);
}

// The network came from a DEF file.
// Preserve everything but the COMPONENT and NET sections by copying them
// and replacing those sections.
static void
rewriteDef(const char *in_filename,
	   const char *filename,
	   LefDefNetwork *network)
{
  FILE *in_stream = fopen(in_filename, "r");
  if (in_stream) {
    FILE *out_stream = fopen(filename, "w");
    if (out_stream) {
      size_t buffer_size = 128;
      char *buffer = new char[buffer_size];
      while (getline(&buffer, &buffer_size, in_stream) >= 0) {
	
	if (stringBeginEqual(buffer, "COMPONENTS ")) {
	  // Skip the components.
	  do {
	    getline(&buffer, &buffer_size, in_stream);
	  } while (!stringBeginEqual(buffer, "END COMPONENTS")
		   && !feof(in_stream));
	  writeDefComponents(out_stream, network);
	}
	else if (stringBeginEqual(buffer, "NETS ")) {
	  // Skip the nets.
	  do {
	    getline(&buffer, &buffer_size, in_stream);
	  } while (!stringBeginEqual(buffer, "END NETS")
		   && !feof(in_stream));
	  writeDefNets(out_stream, network);
	}
	else
	  fputs(buffer, out_stream);
      }
      delete [] buffer;
      fclose(out_stream);
    }
    else
      throw FileNotWritable(filename);
    fclose(in_stream);
  }
  else
    throw FileNotReadable(in_filename);
}

static void
writeDefHeader(int units,
	       // Die area.
	       int die_lx,
	       int die_ly,
	       int die_ux,
	       int die_uy,
	       FILE *out_stream,
	       LefDefNetwork *network)
{
  fprintf(out_stream, "VERSION 5.5 ;\n");
  fprintf(out_stream, "NAMESCASESENSITIVE ON ;\n");
  fprintf(out_stream, "DIVIDERCHAR \"%c\" ;\n", network->pathDivider());
  fprintf(out_stream, "BUSBITCHARS \"[]\" ;\n");
  fprintf(out_stream, "DESIGN %s ;\n",
	  network->name(network->cell(network->topInstance())));
  fprintf(out_stream, "UNITS DISTANCE MICRONS %d ;\n", units);
  fprintf(out_stream, "DIEAREA ( %d %d ) ( %d %d ) ;\n",
	  die_lx, die_ly, die_ux, die_uy);
  fprintf(out_stream, "\n");
}

static void
writeDefComponents(FILE *out_stream,
		   LefDefNetwork *network)
{
  fprintf(out_stream, "COMPONENTS %d ;\n",
	  network->leafInstanceCount());

  LeafInstanceIterator *leaf_iter = network->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    Instance *inst = leaf_iter->next();
    writeDefComponent(inst, out_stream, network);
  }
  delete leaf_iter;

  fprintf(out_stream, "END COMPONENTS\n");
}

static void
writeDefComponent(Instance *inst,
		  FILE *out_stream,
		  LefDefNetwork *network)
{
  defiComponent *def_component = network->defComponent(inst);
  fprintf(out_stream, "- %s %s",
	  staToDef(network->pathName(inst), network),
	  network->name(network->cell(inst)));
  if (def_component) {
    if (def_component->hasEEQ())
      fprintf(out_stream, "\n+ EEQMASTER %s ", def_component->EEQ());
    if (def_component->hasGenerate())
      fprintf(out_stream, "\n+ GENERATE %s %s",
	      def_component->generateName(),
	      def_component->macroName());
    if (def_component->hasSource())
      fprintf(out_stream, "\n+ SOURCE %s", def_component->source());
    if (def_component->hasForeignName())
      fprintf(out_stream, "\n+ FOREIGN %s ( %d %d ) %s",
	      def_component->foreignName(),
	      def_component->foreignX(),
	      def_component->foreignY(),
	      def_component->foreignOri());
    int status = def_component->placementStatus();
    if (status) {
      if (status == DEFI_COMPONENT_UNPLACED)
	fprintf(out_stream, "\n+ UNPLACED");
      else {
	const char *status_key;
	switch (status) {
	case DEFI_COMPONENT_PLACED:
	  status_key = "PLACED";
	  break;
	case DEFI_COMPONENT_FIXED:
	  status_key = "FIXED";
	  break;
	case DEFI_COMPONENT_COVER:
	  status_key = "COVER";
	  break;
	}
	fprintf(out_stream, "\n+ %s ( %d %d ) %s",
		status_key,
		def_component->placementX(),
		def_component->placementY(),
		def_component->placementOrientStr());
      }
    }
    if (def_component->hasWeight())
      fprintf(out_stream, "\n+ WEIGHT %d",
	      def_component->weight());
    if (def_component->hasRegionName())
      fprintf(out_stream, "\n+ REGION %s",
	      def_component->regionName());
    if (def_component->hasRegionBounds()) {
      int size, *xl, *yl, *xh, *yh;
      def_component->regionBounds(&size, &xl, &yl, &xh, &yh);
      fprintf(out_stream, "\n+ REGION ( %d %d ) ( %d %d )",
	      xl[0], yl[0], xh[0], yh[0]);
    }
  }
  fprintf(out_stream, " ;\n");
}
  
static void
writeDefPins(int die_lx,
	     int die_ly,
	     int die_ux,
	     int die_uy,
	     bool auto_place_pins,
	     FILE *out_stream,
	     LefDefNetwork *network)
{
  int pin_count = 0;
  InstancePinIterator *pin_iter1 = network->pinIterator(network->topInstance());
  while (pin_iter1->hasNext()) {
    pin_iter1->next();
    pin_count++;
  }
  delete pin_iter1;

  if (pin_count > 0) {
    double dx = abs(die_ux - die_lx);
    double dy = abs(die_uy - die_ly);
    double die_perimeter = dx * 2 + dy * 2;
    double location = 0.0;
    double pin_dist = die_perimeter / pin_count;

    fprintf(out_stream, "PINS %d ;\n", pin_count);
    InstancePinIterator *pin_iter2 = network->pinIterator(network->topInstance());
    while (pin_iter2->hasNext()) {
      Pin *pin = pin_iter2->next();
      int x, y;
      const char *orient;
      if (location < dx) {
	// bottom
	x = die_lx + location;
	y = die_ly;
	orient = "S";
      }
      else if (location < (dx + dy)) {
	// right
	x = die_ux;
	y = die_ly + (location - dx);
	orient = "E";
      }
      else if (location < (dx * 2 + dy)) {
	// top
	x = die_ux - (location - (dx + dy));
	y = die_uy;
	orient = "N";
      }
      else {
	// left
	x = die_lx;
	y = die_uy - (location - (dx * 2 + dy));
	orient = "W";
      }
      writeDefPin(pin, auto_place_pins, x, y, orient, out_stream, network);
      location += pin_dist;
    }
    delete pin_iter2;
  
    fprintf(out_stream, "END PINS\n");
  }
}

static void
writeDefPin(Pin *pin,
	    bool is_placed,
	    int x,
	    int y,
	    const char *orient,	    
	    FILE *out_stream,
	    LefDefNetwork *network)
{
  fprintf(out_stream, "- %s",
	  network->pathName(pin));
  Net *net = network->net(network->term(pin));
  if (net)
    fprintf(out_stream, " + NET %s",
	    network->pathName(net));
  PortDirection *dir = network->direction(network->port(pin));
  fprintf(out_stream, " + DIRECTION %s",
	  staToDef(dir));
  if (is_placed)
    fprintf(out_stream, " + FIXED ( %d %d ) %s", x, y, orient);
  fprintf(out_stream, " ;\n");
}

static const char *
staToDef(PortDirection *dir)
{
  if (dir == PortDirection::input())
    return "INPUT";
  else if (dir == PortDirection::output())
    return "OUTPUT";
  else if (dir == PortDirection::bidirect())
    return "INOUT";
  else if (dir == PortDirection::tristate())
    return "OUTPUT TRISTATE";
  else
    return "INOUT";
}

static void
writeDefNets(FILE *out_stream,
	     LefDefNetwork *network)
{
  fprintf(out_stream, "NETS %d ;\n",
	  network->netCount());
  writeDefNets(network->topInstance(), out_stream, network);  
  fprintf(out_stream, "END NETS\n");
}

static void
writeDefNets(const Instance *inst,
	     FILE *out_stream,
	     LefDefNetwork *network)
{
  NetIterator *net_iter = network->netIterator(inst);
  while (net_iter->hasNext()) {
    Net *net = net_iter->next();
    if (!network->isGround(net) && !network->isPower(net))
      writeDefNet(net, out_stream, network);
  }
  delete net_iter;

  // Decend the hierarchy.
  InstanceChildIterator *child_iter = network->childIterator(inst);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    if (network->isHierarchical(child))
      writeDefNets(child, out_stream, network);
  }
  delete child_iter;
}

static void
writeDefNet(Net *net,
	    FILE *out_stream,
	    LefDefNetwork *network)
{
  const char *sta_net_name = network->pathName(net);
  const char *def_net_name = staToDef(sta_net_name, network);
  fprintf(out_stream, "- %s", def_net_name);
  int column = strlen(def_net_name) + 2;
  int column_max = 80;

  NetConnectedPinIterator *pin_iter = network->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    int width = 0;
    if (network->isTopLevelPort(pin)) {
      const char *port_name = network->portName(pin);
      fprintf(out_stream, " ( PIN %s )",
	      port_name);
      width = strlen(port_name) + 9;
    }
    else if (network->isLeaf(pin)) {
      const char *sta_component_name = network->pathName(network->instance(pin));
      const char *def_component_name = staToDef(sta_component_name, network);
      const char *port_name = network->portName(pin);
      fprintf(out_stream, " ( %s %s )",
	      def_component_name,
	      port_name);
      width = strlen(def_component_name) + strlen(port_name) + 6;
    }
    if ((column + width) > column_max) {
      fprintf(out_stream, "\n ");
      column = 0;
    }
    column += width;
  }
  delete pin_iter;
  fprintf(out_stream, " ;\n");
}

// Remove path divider escapes in token.
static const char *
staToDef(const char *token,
	 Network *network)
{
  char path_escape = network->pathEscape();
  char *unescaped = makeTmpString(strlen(token) + 1);
  char *u = unescaped;
  for (const char *s = token; *s ; s++) {
    char ch = *s;

    if (ch != path_escape)
      *u++ = ch;
  }
  *u = '\0';
  return unescaped;
}

} // namespace
