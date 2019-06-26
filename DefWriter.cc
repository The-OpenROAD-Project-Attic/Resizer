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
#include "Report.hh"
#include "StringUtil.hh"
#include "PortDirection.hh"
#include "LefDefNetwork.hh"
#include "NetworkCmp.hh"
#include "defiComponent.hpp"
#include "defiNet.hpp"
#include "DefWriter.hh"

namespace sta {

using std::abs;

static void
rewriteDef(const char *in_filename,
	   const char *filename,
	   bool sort,
	   LefDefNetwork *network);
static void
writeDefFresh(const char *filename,
	      int units,
	      double die_lx,
	      double die_ly,
	      double die_ux,
	      double die_uy,
	      double core_lx,
	      double core_ly,
	      double core_ux,
	      double core_uy,
	      const char *site_name,
	      bool auto_place_pins,
	      bool sort,
	      LefDefNetwork *network);
static void
writeDefComponents(bool sort,
		   FILE *out_stream,
		   LefDefNetwork *network);
static void
writeDefComponent(Instance *inst,
		  FILE *out_stream,
		  LefDefNetwork *network);
static void
writeDefPins(double core_lx,
	     double core_ly,
	     double core_ux,
	     double core_uy,
	     bool auto_place_pins,
	     FILE *out_stream,
	     LefDefNetwork *network);
static void
writeDefPin(Pin *pin,
	    bool is_placed,
	    double x,
	    double y,
	    const char *orient,	    
	    FILE *out_stream,
	    LefDefNetwork *network);
static const char *
staToDef(PortDirection *dir);
static void
writeDefNets(bool sort,
	     FILE *out_stream,
	     LefDefNetwork *network);
static void
writeDefNets(const Instance *inst,
	     bool sort,
	     FILE *out_stream,
	     LefDefNetwork *network);
static void
writeDefNet(Net *net,
	    bool sort,
	    FILE *out_stream,
	    LefDefNetwork *network);
static const char *
staToDef(const char *token,
	 Network *network);
static void
writeDefHeader(int units,
	       // Die area.
	       double die_lx,
	       double die_ly,
	       double die_ux,
	       double die_uy,
	       FILE *out_stream,
	       LefDefNetwork *network);
static void
writeDefRows(const char *site_name,
	     double core_lx,
	     double core_ly,
	     double core_ux,
	     double core_uy,
	     FILE *out_stream,
	     LefDefNetwork *network);

////////////////////////////////////////////////////////////////

void
writeDef(const char *filename,
	 int units,
	 // Die area.
	 double die_lx,
	 double die_ly,
	 double die_ux,
	 double die_uy,
	 double core_lx,
	 double core_ly,
	 double core_ux,
	 double core_uy,
	 const char *site_name,
	 bool auto_place_pins,
	 bool sort,
	 LefDefNetwork *network)
{
  const char *in_filename = network->defFilename();
  if (in_filename)
    rewriteDef(in_filename, filename, sort, network);
  else
    writeDefFresh(filename, units,
		  die_lx, die_ly, die_ux, die_uy,
		  core_lx, core_ly, core_ux, core_uy,
		  site_name, auto_place_pins, sort, network);
}

// Write a fresh DEF file from the network.
static void
writeDefFresh(const char *filename,
	      int units,
	      // Die area.
	      double die_lx,
	      double die_ly,
	      double die_ux,
	      double die_uy,
	      double core_lx,
	      double core_ly,
	      double core_ux,
	      double core_uy,
	      const char *site_name,
	      bool auto_place_pins,
	      bool sort,
	      LefDefNetwork *network)
{
  FILE *out_stream = fopen(filename, "w");
  if (out_stream) {
    network->setDefUnits(units);
    writeDefHeader(units, die_lx, die_ly, die_ux, die_uy,
		   out_stream, network);
    fprintf(out_stream, "\n");
    writeDefRows(site_name, core_lx, core_ly, core_ux, core_uy,
		 out_stream, network);
    fprintf(out_stream, "\n");
    writeDefComponents(sort, out_stream, network);
    fprintf(out_stream, "\n");
    writeDefPins(core_lx, core_ly, core_ux, core_uy,
		 auto_place_pins, out_stream, network);
    fprintf(out_stream, "\n");
    writeDefNets(sort, out_stream, network);
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
	   bool sort,
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
	  writeDefComponents(sort, out_stream, network);
	}
	else if (stringBeginEqual(buffer, "NETS ")) {
	  // Skip the nets.
	  do {
	    getline(&buffer, &buffer_size, in_stream);
	  } while (!stringBeginEqual(buffer, "END NETS")
		   && !feof(in_stream));
	  writeDefNets(sort, out_stream, network);
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
	       double die_lx,
	       double die_ly,
	       double die_ux,
	       double die_uy,
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
	  network->metersToDbu(die_lx),
	  network->metersToDbu(die_ly),
	  network->metersToDbu(die_ux),
	  network->metersToDbu(die_uy));
}

static void
writeDefRows(const char *site_name,
	     double core_lx,
	     double core_ly,
	     double core_ux,
	     double core_uy,
	     FILE *out_stream,
	     LefDefNetwork *network)
{
  if (site_name
      &&  core_lx >= 0.0 && core_lx >= 0.0 && core_ux >= 0.0 && core_uy >= 0.0) {
    lefiSite *site = network->findLefSite(site_name);
    if (site) {
      if (site->hasSize()) {
	// LEF site size is in microns. Convert to meters.
	double site_dx = site->sizeX() * 1e-6;
	double site_dy = site->sizeY() * 1e-6;
	int site_dx_dbu = network->metersToDbu(site_dx);
	int site_dy_dbu = network->metersToDbu(site_dy);
	double core_dx = abs(core_ux - core_lx);
	double core_dy = abs(core_uy - core_ly);
	int rows_x = core_dx / site_dx;
	int rows_y = core_dy / site_dy;

	int core_lx_dbu = network->metersToDbu(core_lx);
	int y = network->metersToDbu(core_ly);;
	for (int row = 0; row < rows_y; row++) {
	  const char *orient = (row % 2 == 0) ? "FS" : "N";
	  fprintf(out_stream, "ROW ROW_%d %s %d %d %s DO %d by 1 STEP %d 0 ;\n",
		  row,
		  site_name,
		  core_lx_dbu,
		  y,
		  orient,
		  rows_x,
		  site_dx_dbu);
	  y += site_dy_dbu;
	}
      }
      else
	network->report()->printWarn("Warning: LEF site %s does not have size.\n",
				   site_name);
    }
    else
      network->report()->printWarn("Warning: LEF site %s not found.\n",
				   site_name);
  }
}

static void
writeDefComponents(bool sort,
		   FILE *out_stream,
		   LefDefNetwork *network)
{
  fprintf(out_stream, "COMPONENTS %d ;\n",
	  network->leafInstanceCount());

  InstanceSeq insts;
  LeafInstanceIterator *leaf_iter = network->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    Instance *inst = leaf_iter->next();
    insts.push_back(inst);
  }
  delete leaf_iter;

  if (sort)
    sta::sort(insts, InstancePathNameLess(network));
  
  for (auto inst : insts)
    writeDefComponent(inst, out_stream, network);

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
writeDefPins(double core_lx,
	     double core_ly,
	     double core_ux,
	     double core_uy,
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
    double dx = abs(core_ux - core_lx);
    double dy = abs(core_uy - core_ly);
    double perimeter = dx * 2 + dy * 2;
    double location = 0.0;
    double pin_dist = perimeter / pin_count;

    fprintf(out_stream, "PINS %d ;\n", pin_count);
    InstancePinIterator *pin_iter2 = network->pinIterator(network->topInstance());
    while (pin_iter2->hasNext()) {
      Pin *pin = pin_iter2->next();
      double x, y;
      const char *orient;
      if (location < dx) {
	// bottom
	x = core_lx + location;
	y = core_ly;
	orient = "S";
      }
      else if (location < (dx + dy)) {
	// right
	x = core_ux;
	y = core_ly + (location - dx);
	orient = "E";
      }
      else if (location < (dx * 2 + dy)) {
	// top
	x = core_ux - (location - (dx + dy));
	y = core_uy;
	orient = "N";
      }
      else {
	// left
	x = core_lx;
	y = core_uy - (location - (dx * 2 + dy));
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
	    double x,
	    double y,
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
    fprintf(out_stream, " + FIXED ( %d %d ) %s",
	    network->metersToDbu(x),
	    network->metersToDbu(y),
	    orient);
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
writeDefNets(bool sort,
	     FILE *out_stream,
	     LefDefNetwork *network)
{
  fprintf(out_stream, "NETS %d ;\n",
	  network->netCount());
  writeDefNets(network->topInstance(), sort, out_stream, network);  
  fprintf(out_stream, "END NETS\n");
}

static void
writeDefNets(const Instance *inst,
	     bool sort,
	     FILE *out_stream,
	     LefDefNetwork *network)
{
  NetSeq nets;
  NetIterator *net_iter = network->netIterator(inst);
  while (net_iter->hasNext()) {
    Net *net = net_iter->next();
    if (!network->isGround(net) && !network->isPower(net))
      nets.push_back(net);
  }
  delete net_iter;

  if (sort)
    sta::sort(nets, NetPathNameLess(network));

  for (auto net : nets)
    writeDefNet(net, sort, out_stream, network);

  // Decend the hierarchy.
  InstanceChildIterator *child_iter = network->childIterator(inst);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    if (network->isHierarchical(child))
      writeDefNets(child, sort, out_stream, network);
  }
  delete child_iter;
}

static void
writeDefNet(Net *net,
	    bool sort,
	    FILE *out_stream,
	    LefDefNetwork *network)
{
  const char *sta_net_name = network->pathName(net);
  const char *def_net_name = staToDef(sta_net_name, network);
  fprintf(out_stream, "- %s", def_net_name);
  int column = strlen(def_net_name) + 2;
  int column_max = 80;

  Vector<const Pin*> pins;
  NetConnectedPinIterator *pin_iter = network->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    pins.push_back(pin);
  }
  delete pin_iter;

  if (sort)
    sta::sort(pins, PinPathNameLess(network));

  for (auto pin : pins) {
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
