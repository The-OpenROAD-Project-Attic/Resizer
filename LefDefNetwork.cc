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

#include "Machine.hh"
#include "Liberty.hh"
#include "LefDefNetwork.hh"

namespace sta {

LefDefNetwork::LefDefNetwork() :
  ConcreteNetwork(),
  filename_(nullptr),
  lef_library_(nullptr)
{
}

LefDefNetwork::~LefDefNetwork()
{
  stringDelete(filename_);
}

void
LefDefNetwork::clear()
{
  stringDelete(filename_);
  filename_ = nullptr;
  lef_library_ = nullptr;
  ConcreteNetwork::clear();
}

void
LefDefNetwork::setDivider(char divider)
{
  divider_ = divider_;
}

void
LefDefNetwork::setFilename(const char *filename)
{
  filename_ = stringCopy(filename);
}

Library *
LefDefNetwork::makeLibrary(const char *name,
			   const char *filename)
{
  lef_library_ = new LefLibrary(name, filename);
  addLibrary(lef_library_);
  return lefToSta(lef_library_);
}

Library *
LefDefNetwork::lefLibrary()
{
  return lefToSta(lef_library_);
}

Cell *
LefDefNetwork::makeCell(Library *library,
			const char *name,
			bool is_leaf,
			const char *filename)
{
  LefLibrary *lef_lib = staToLef(library);
  LefMacro *macro = new LefMacro(lef_lib, name, is_leaf, filename);
  lef_lib->addCell(macro);

  // Find corresponding liberty cell.
  // This assumes liberty libraries are read before LEF.
  LibertyCell *lib_cell = findLibertyCell(name);
  macro->setLibertyCell(lib_cell);
  return lefToSta(macro);
}

LibertyCell *
LefDefNetwork::libertyCell(Cell *cell) const
{
  LefMacro *macro = staToLef(cell);
  return macro->libertyCell();
}

LibertyPort *
LefDefNetwork::libertyPort(Port *port) const
{
  Cell *cell = this->cell(port);
  LibertyCell *liberty_cell = libertyCell(cell);
  if (liberty_cell) {
    const char *port_name = this->name(port);
    return liberty_cell->findLibertyPort(port_name);
  }
  else
    return nullptr;
}

void
LefDefNetwork::initTopInstancePins()
{
  staToDef(top_instance_)->initPins();
}

Instance *
LefDefNetwork::makeInstance(Cell *cell,
			    const char *name,
			    Instance *)
{
  DefComponent *component = makeDefComponent(cell, name, nullptr);
  return defToSta(component);
}

Instance *
LefDefNetwork::makeInstance(LibertyCell *cell,
			    const char *name,
			    Instance *parent)
{
  // Keep it all in the family.
  ConcreteCell *ccell = lef_library_->findCell(cell->name());
  Cell *macro_cell = reinterpret_cast<Cell*>(ccell);
  return makeInstance(macro_cell, name, parent);
}

DefComponent *
LefDefNetwork::makeDefComponent(Cell *cell,
				const char *name,
				defiComponent *def_component)
{
  LefMacro *macro = staToLef(cell);
  DefComponent *top = staToDef(top_instance_);
  DefComponent *component = new DefComponent(macro, name, top, def_component);
  if (top_instance_)
    top->addChild(component);
  return component;
}

void
LefDefNetwork::replaceCell(Instance *inst,
			   LibertyCell *cell)
{
  // Keep it all in the family.
  ConcreteCell *ccell = lef_library_->findCell(cell->name());
  replaceCellIntenal(inst, ccell);
}

Instance *
LefDefNetwork::findInstance(const char *name) const
{
  DefComponent *top = staToDef(top_instance_);
  return top->findChild(name);
}

Net *
LefDefNetwork::makeNet(const char *name,
		       defiNet *def_net)
{
  DefComponent *top = staToDef(top_instance_);
  DefNet *net = new DefNet(name, top, def_net);
  top->addNet(net);
  return defToSta(net);
}

////////////////////////////////////////////////////////////////

Library *
LefDefNetwork::lefToSta(LefLibrary *lib) const
{
  return reinterpret_cast<Library*>(lib);
}

LefLibrary *
LefDefNetwork::staToLef(Library *lib) const
{
  return reinterpret_cast<LefLibrary*>(lib);
}

Cell *
LefDefNetwork::lefToSta(LefMacro *macro) const
{
  return reinterpret_cast<Cell*>(macro);
}

LefMacro *
LefDefNetwork::staToLef(Cell *cell) const
{
  return reinterpret_cast<LefMacro*>(cell);
}

DefComponent *
LefDefNetwork::staToDef(Instance *inst) const
{
  return reinterpret_cast<DefComponent*>(inst);
}

Instance *
LefDefNetwork::defToSta(DefComponent *component) const
{
  return reinterpret_cast<Instance*>(component);
}

Net *
LefDefNetwork::defToSta(DefNet *net) const
{
  return reinterpret_cast<Net*>(net);
}

////////////////////////////////////////////////////////////////

LefMacro::LefMacro(ConcreteLibrary *library,
		   const char *name,
		   bool is_leaf,
		   const char *filename) :
  ConcreteCell(library, name, is_leaf, filename),
  liberty_cell_(nullptr)
{
}

LefPin *
LefMacro::makeLefPin(const char *name)
{
  LefPin *pin = new LefPin(this, name);
  addPort(pin);
  return pin;
}

void
LefMacro::setLefMacro(lefiMacro *lef_macro)
{
  if (lef_macro)
    lef_macro_ = new lefiMacro(*lef_macro);
}

void
LefMacro::setLibertyCell(LibertyCell *cell)
{
  liberty_cell_ = cell;
}

////////////////////////////////////////////////////////////////

LefPin::LefPin(LefMacro *macro,
	       const char *name) :
  ConcretePort(macro, name, false, -1, -1, false, nullptr)
{
}

////////////////////////////////////////////////////////////////

DefComponent::DefComponent(ConcreteCell *cell,
			   const char *name,
			   ConcreteInstance *parent,
			   defiComponent *component) :
  ConcreteInstance(cell, name, parent),
  def_component_(component ? new defiComponent(*component) : nullptr)
{
}

LefMacro *
DefComponent::lefMacro()
{
  return dynamic_cast<LefMacro *>(cell_);
}

////////////////////////////////////////////////////////////////

DefNet::DefNet(const char *name,
	       ConcreteInstance *top_instance,
	       defiNet *def_net) :
  ConcreteNet(name, top_instance),
  def_net_(nullptr)
{
  if (def_net)
    def_net_ = new defiNet(*def_net);
}

};
