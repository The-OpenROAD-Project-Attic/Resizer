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
#include "Liberty.hh"
#include "SdcNetwork.hh"
#include "LefDefNetwork.hh"

namespace sta {

using std::round;

LefDefNetwork::LefDefNetwork() :
  ConcreteNetwork(),
  def_filename_(nullptr),
  lef_library_(nullptr)
{
}

LefDefNetwork::~LefDefNetwork()
{
  clear();
}

void
LefDefNetwork::clear()
{
  stringDelete(def_filename_);
  def_filename_ = nullptr;
  lef_library_ = nullptr;
  def_component_map_.deleteContents();
  lef_macro_map_.deleteContents();
  lef_size_map_.deleteContents();
  ConcreteNetwork::clear();
}

void
LefDefNetwork::initState(Report *report,
			 Debug *debug)
{
  report_ = report;
  debug_ = debug;
  sdc_network_ = new SdcNetwork(this);
}

void
LefDefNetwork::setDivider(char divider)
{
  divider_ = divider;
}

void
LefDefNetwork::setDefUnits(int def_units)
{
  def_units_ = def_units;
}

double
LefDefNetwork::dbuToMeters(int dbu) const
{
  return (dbu / def_units_) * 1e-6;
}

int
LefDefNetwork::metersToDbu(double dist) const
{
  return round((dist * 1e6) * def_units_);
}

void
LefDefNetwork::setDefFilename(const char *filename)
{
  def_filename_ = stringCopy(filename);
}

Library *
LefDefNetwork::makeLefLibrary(const char *name,
			      const char *filename)
{
  lef_library_ = ConcreteNetwork::makeLibrary(name, filename);
  return lef_library_;
}

Library *
LefDefNetwork::lefLibrary()
{
  return lef_library_;
}

void
LefDefNetwork::setLefMacro(Cell *cell,
			   lefiMacro *lef_macro)
{
  lef_macro_map_[cell] = lef_macro;
}

lefiMacro *
LefDefNetwork::lefMacro(Cell *cell)
{
  return lef_macro_map_.findKey(cell);
}

Cell *
LefDefNetwork::lefCell(LibertyCell *cell)
{
  return findCell(lef_library_, cell->name());
}

bool
LefDefNetwork::isLefCell(Cell *cell) const
{
  return library(cell) == lef_library_;
}

void
LefDefNetwork::initTopInstancePins()
{
  ConcreteInstance *ctop_inst = reinterpret_cast<ConcreteInstance*>(top_instance_);
  ctop_inst->initPins();
}

Instance *
LefDefNetwork::makeDefComponent(Cell *cell,
				const char *name,
				defiComponent *def_component)
{
  Instance *inst = makeInstance(cell, name, top_instance_);
  if (def_component)
    def_component_map_[inst] = def_component;
  return inst;
}

defiComponent *
LefDefNetwork::defComponent(Instance *inst) const
{
  return def_component_map_.findKey(inst);
}

void
LefDefNetwork::setLocation(Instance *instance,
			   DefPt location)
{
  defiComponent *def_component = defComponent(instance);
  if (def_component == nullptr) {
    def_component = new defiComponent(nullptr);
    def_component_map_[instance] = def_component;
  }
  def_component->setPlacementStatus(DEFI_COMPONENT_PLACED);
  def_component->setPlacementLocation(location.x(), location.y(), 0);
}

DefPt
LefDefNetwork::location(const Pin *pin) const
{
  Instance *inst = instance(pin);
  defiComponent *def_component = def_component_map_.findKey(inst);
  if (def_component
      && def_component->isPlaced()) {
    // Component location is good enough for now.
    return DefPt(def_component->placementX(),
		 def_component->placementY());
  }
  else if (isTopLevelPort(pin)) {
    Port *port = this->port(pin);
    DefPt location;
    bool exists;
    port_locations_.findKey(port, location, exists);
    if (exists)
      return location;
  }
  return DefPt(0, 0);
}

void
LefDefNetwork::setLocation(Port *port,
			   DefPt location)
{
  port_locations_[port] = location;
}

bool
LefDefNetwork::isPlaced(const Pin *pin) const
{
  Instance *inst = instance(pin);
  if (inst == top_instance_)
    return port_locations_.hasKey(port(pin));
  else {
    defiComponent *def_component = defComponent(inst);
    return def_component
      && def_component->isPlaced();
  }
}

////////////////////////////////////////////////////////////////

void
LefDefNetwork::makeLefSite(lefiSite *site)
{
  lefiSite *copy = new lefiSite(*site);
  lef_size_map_[copy->name()] = copy;
}

lefiSite *
LefDefNetwork::findLefSite(const char *name)
{
  return lef_size_map_.findKey(name);
}

////////////////////////////////////////////////////////////////

DefPt::DefPt(int x,
	     int y) :
  x_(x),
  y_(y)
{
}

DefPt::DefPt() :
  x_(0),
  y_(0)
{
}

} // namespace
