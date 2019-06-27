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

class DefWriter
{
public:
  DefWriter(const char *filename,
	    bool sort,
	    LefDefNetwork *network);
  void rewriteDef(const char *in_filename);
  void writeDefFresh(int units,
		     double die_lx,
		     double die_ly,
		     double die_ux,
		     double die_uy,
		     double core_lx,
		     double core_ly,
		     double core_ux,
		     double core_uy,
		     const char *site_name,
		     bool auto_place_pins);

protected:
  void writeDefHeader(int units,
		      // Die area.
		      double die_lx,
		      double die_ly,
		      double die_ux,
		      double die_uy);
  void writeDefRows(const char *site_name,
		    double core_lx,
		    double core_ly,
		    double core_ux,
		    double core_uy);
  void writeDefComponents();
  void writeDefComponent(Instance *inst);
  void writeDefPins(double core_lx,
		    double core_ly,
		    double core_ux,
		    double core_uy,
		    bool auto_place_pins);
  void writeDefPin(Pin *pin,
		   bool is_placed,
		   double x,
		   double y,
		   const char *orient);
  const char *staToDef(PortDirection *dir);
  void writeDefNets();
  void writeDefNets(const Instance *inst);
  void writeDefNet(Net *net);
  const char *staToDef(const char *token);

  const char *filename_;
  bool sort_;
  LefDefNetwork *network_;
  FILE *out_stream_;
};

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
  DefWriter writer(filename, sort, network);
  const char *in_filename = network->defFilename();
  if (in_filename)
    writer.rewriteDef(in_filename);
  else
    writer.writeDefFresh(units,
			 die_lx, die_ly, die_ux, die_uy,
			 core_lx, core_ly, core_ux, core_uy,
			 site_name, auto_place_pins);
}

DefWriter::DefWriter(const char *filename,
		     bool sort,
		     LefDefNetwork *network) :
  filename_(filename),
  sort_(sort),
  network_(network)
{
}

// Write a fresh DEF file from the network.
void
DefWriter::writeDefFresh(int units,
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
			 bool auto_place_pins)
{
  out_stream_ = fopen(filename_, "w");
  if (out_stream_) {
    network_->setDefUnits(units);
    writeDefHeader(units, die_lx, die_ly, die_ux, die_uy);
    fprintf(out_stream_, "\n");
    writeDefRows(site_name, core_lx, core_ly, core_ux, core_uy);
    fprintf(out_stream_, "\n");
    writeDefComponents();
    fprintf(out_stream_, "\n");
    writeDefPins(core_lx, core_ly, core_ux, core_uy, auto_place_pins);
    fprintf(out_stream_, "\n");
    writeDefNets();
    fprintf(out_stream_, "\nEND DESIGN\n");
    fclose(out_stream_);
  }
  else
    throw FileNotWritable(filename_);
}

// The network came from a DEF file.
// Preserve everything but the COMPONENT and NET sections by copying them
// and replacing those sections.
void
DefWriter::rewriteDef(const char *in_filename)
{
  FILE *in_stream = fopen(in_filename, "r");
  if (in_stream) {
    out_stream_ = fopen(filename_, "w");
    if (out_stream_) {
      size_t buffer_size = 128;
      char *buffer = new char[buffer_size];
      while (getline(&buffer, &buffer_size, in_stream) >= 0) {
	if (stringBeginEqual(buffer, "COMPONENTS ")) {
	  // Skip the components.
	  do {
	    getline(&buffer, &buffer_size, in_stream);
	  } while (!stringBeginEqual(buffer, "END COMPONENTS")
		   && !feof(in_stream));
	  writeDefComponents();
	}
	else if (stringBeginEqual(buffer, "NETS ")) {
	  // Skip the nets.
	  do {
	    getline(&buffer, &buffer_size, in_stream);
	  } while (!stringBeginEqual(buffer, "END NETS")
		   && !feof(in_stream));
	  writeDefNets();
	}
	else
	  fputs(buffer, out_stream_);
      }
      delete [] buffer;
      fclose(out_stream_);
    }
    else
      throw FileNotWritable(filename_);
    fclose(in_stream);
  }
  else
    throw FileNotReadable(in_filename);
}

void
DefWriter::writeDefHeader(int units,
			  // Die area.
			  double die_lx,
			  double die_ly,
			  double die_ux,
			  double die_uy)
{
  fprintf(out_stream_, "VERSION 5.5 ;\n");
  fprintf(out_stream_, "NAMESCASESENSITIVE ON ;\n");
  fprintf(out_stream_, "DIVIDERCHAR \"%c\" ;\n", network_->pathDivider());
  fprintf(out_stream_, "BUSBITCHARS \"[]\" ;\n");
  fprintf(out_stream_, "DESIGN %s ;\n",
	  network_->name(network_->cell(network_->topInstance())));
  fprintf(out_stream_, "UNITS DISTANCE MICRONS %d ;\n", units);
  fprintf(out_stream_, "DIEAREA ( %d %d ) ( %d %d ) ;\n",
	  network_->metersToDbu(die_lx),
	  network_->metersToDbu(die_ly),
	  network_->metersToDbu(die_ux),
	  network_->metersToDbu(die_uy));
}

void
DefWriter::writeDefRows(const char *site_name,
			double core_lx,
			double core_ly,
			double core_ux,
			double core_uy)
{
  if (site_name
      &&  core_lx >= 0.0 && core_lx >= 0.0 && core_ux >= 0.0 && core_uy >= 0.0) {
    lefiSite *site = network_->findLefSite(site_name);
    if (site) {
      if (site->hasSize()) {
	// LEF site size is in microns. Convert to meters.
	double site_dx = site->sizeX() * 1e-6;
	double site_dy = site->sizeY() * 1e-6;
	int site_dx_dbu = network_->metersToDbu(site_dx);
	int site_dy_dbu = network_->metersToDbu(site_dy);
	double core_dx = abs(core_ux - core_lx);
	double core_dy = abs(core_uy - core_ly);
	int rows_x = core_dx / site_dx;
	int rows_y = core_dy / site_dy;

	int core_lx_dbu = network_->metersToDbu(core_lx);
	int y = network_->metersToDbu(core_ly);;
	for (int row = 0; row < rows_y; row++) {
	  const char *orient = (row % 2 == 0) ? "FS" : "N";
	  fprintf(out_stream_, "ROW ROW_%d %s %d %d %s DO %d by 1 STEP %d 0 ;\n",
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
	network_->report()->printWarn("Warning: LEF site %s does not have size.\n",
				   site_name);
    }
    else
      network_->report()->printWarn("Warning: LEF site %s not found.\n",
				   site_name);
  }
}

void
DefWriter::writeDefComponents()
{
  fprintf(out_stream_, "COMPONENTS %d ;\n",
	  network_->leafInstanceCount());

  InstanceSeq insts;
  LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    Instance *inst = leaf_iter->next();
    insts.push_back(inst);
  }
  delete leaf_iter;

  if (sort_)
    sort(insts, InstancePathNameLess(network_));
  
  for (auto inst : insts)
    writeDefComponent(inst);

  fprintf(out_stream_, "END COMPONENTS\n");
}

void
DefWriter::writeDefComponent(Instance *inst)
{
  defiComponent *def_component = network_->defComponent(inst);
  fprintf(out_stream_, "- %s %s",
	  staToDef(network_->pathName(inst)),
	  network_->name(network_->cell(inst)));
  if (def_component) {
    if (def_component->hasEEQ())
      fprintf(out_stream_, "\n+ EEQMASTER %s ", def_component->EEQ());
    if (def_component->hasGenerate())
      fprintf(out_stream_, "\n+ GENERATE %s %s",
	      def_component->generateName(),
	      def_component->macroName());
    if (def_component->hasSource())
      fprintf(out_stream_, "\n+ SOURCE %s", def_component->source());
    if (def_component->hasForeignName())
      fprintf(out_stream_, "\n+ FOREIGN %s ( %d %d ) %s",
	      def_component->foreignName(),
	      def_component->foreignX(),
	      def_component->foreignY(),
	      def_component->foreignOri());
    int status = def_component->placementStatus();
    if (status) {
      if (status == DEFI_COMPONENT_UNPLACED)
	fprintf(out_stream_, "\n+ UNPLACED");
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
	fprintf(out_stream_, "\n+ %s ( %d %d ) %s",
		status_key,
		def_component->placementX(),
		def_component->placementY(),
		def_component->placementOrientStr());
      }
    }
    if (def_component->hasWeight())
      fprintf(out_stream_, "\n+ WEIGHT %d",
	      def_component->weight());
    if (def_component->hasRegionName())
      fprintf(out_stream_, "\n+ REGION %s",
	      def_component->regionName());
    if (def_component->hasRegionBounds()) {
      int size, *xl, *yl, *xh, *yh;
      def_component->regionBounds(&size, &xl, &yl, &xh, &yh);
      fprintf(out_stream_, "\n+ REGION ( %d %d ) ( %d %d )",
	      xl[0], yl[0], xh[0], yh[0]);
    }
  }
  fprintf(out_stream_, " ;\n");
}
  
void
DefWriter::writeDefPins(double core_lx,
			double core_ly,
			double core_ux,
			double core_uy,
			bool auto_place_pins)
{
  int pin_count = 0;
  InstancePinIterator *pin_iter1 = network_->pinIterator(network_->topInstance());
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

    fprintf(out_stream_, "PINS %d ;\n", pin_count);
    InstancePinIterator *pin_iter2 = network_->pinIterator(network_->topInstance());
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
      writeDefPin(pin, auto_place_pins, x, y, orient);
      location += pin_dist;
    }
    delete pin_iter2;
  
    fprintf(out_stream_, "END PINS\n");
  }
}

void
DefWriter::writeDefPin(Pin *pin,
		       bool is_placed,
		       double x,
		       double y,
		       const char *orient)
{
  fprintf(out_stream_, "- %s",
	  network_->pathName(pin));
  Net *net = network_->net(network_->term(pin));
  if (net)
    fprintf(out_stream_, " + NET %s",
	    network_->pathName(net));
  PortDirection *dir = network_->direction(network_->port(pin));
  fprintf(out_stream_, " + DIRECTION %s",
	  staToDef(dir));
  if (is_placed)
    fprintf(out_stream_, " + FIXED ( %d %d ) %s",
	    network_->metersToDbu(x),
	    network_->metersToDbu(y),
	    orient);
  fprintf(out_stream_, " ;\n");
}

const char *
DefWriter::staToDef(PortDirection *dir)
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

void
DefWriter::writeDefNets()
{
  fprintf(out_stream_, "NETS %d ;\n",
	  network_->netCount());
  writeDefNets(network_->topInstance());  
  fprintf(out_stream_, "END NETS\n");
}

void
DefWriter::writeDefNets(const Instance *inst)
{
  NetSeq nets;
  NetIterator *net_iter = network_->netIterator(inst);
  while (net_iter->hasNext()) {
    Net *net = net_iter->next();
    if (!network_->isGround(net) && !network_->isPower(net))
      nets.push_back(net);
  }
  delete net_iter;

  if (sort_)
    sort(nets, NetPathNameLess(network_));

  for (auto net : nets)
    writeDefNet(net);

  // Decend the hierarchy.
  InstanceChildIterator *child_iter = network_->childIterator(inst);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    if (network_->isHierarchical(child))
      writeDefNets(child);
  }
  delete child_iter;
}

void
DefWriter::writeDefNet(Net *net)
{
  const char *sta_net_name = network_->pathName(net);
  const char *def_net_name = staToDef(sta_net_name);
  fprintf(out_stream_, "- %s", def_net_name);
  int column = strlen(def_net_name) + 2;
  int column_max = 80;

  Vector<const Pin*> pins;
  NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    pins.push_back(pin);
  }
  delete pin_iter;

  if (sort_)
    sort(pins, PinPathNameLess(network_));

  for (auto pin : pins) {
    int width = 0;
    if (network_->isTopLevelPort(pin)) {
      const char *port_name = network_->portName(pin);
      fprintf(out_stream_, " ( PIN %s )",
	      port_name);
      width = strlen(port_name) + 9;
    }
    else if (network_->isLeaf(pin)) {
      const char *sta_component_name = network_->pathName(network_->instance(pin));
      const char *def_component_name = staToDef(sta_component_name);
      const char *port_name = network_->portName(pin);
      fprintf(out_stream_, " ( %s %s )",
	      def_component_name,
	      port_name);
      width = strlen(def_component_name) + strlen(port_name) + 6;
    }
    if ((column + width) > column_max) {
      fprintf(out_stream_, "\n ");
      column = 0;
    }
    column += width;
  }
  fprintf(out_stream_, " ;\n");
}

// Remove path divider escapes in token.
const char *
DefWriter::staToDef(const char *token)
{
  char path_escape = network_->pathEscape();
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
