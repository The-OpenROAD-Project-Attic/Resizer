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

#ifndef RESIZER_LEF_DEF_NETWORK_H
#define RESIZER_LEF_DEF_NETWORK_H

#include "UnorderedMap.hh"
#include "ConcreteLibrary.hh"
#include "ConcreteNetwork.hh"
#include "lefiMacro.hpp"
#include "defiComponent.hpp"
#include "defiNet.hpp"

namespace sta {

class DefPt;

// Database location type used by DEF parser.
typedef int DefDbu;

class DefPt
{
public:
  DefPt();
  DefPt(DefDbu x,
	DefDbu y);
  DefDbu x() const { return x_; }
  DefDbu y() const { return y_; }

private:
  DefDbu x_;
  DefDbu y_;
};

// No need to specializing ConcreteLibrary at this point.
typedef UnorderedMap<Cell*, LibertyCell*> LibertyCellMap;
typedef UnorderedMap<Port*, DefPt> DefPortLocations;
typedef UnorderedMap<Instance*, defiComponent *> InstanceDefComponentMap;
typedef UnorderedMap<Cell*, lefiMacro*> CellLefMacroMap;
typedef Map<const char*, lefiSite*, CharPtrLess> LefSiteMap;

class LefDefNetwork : public ConcreteNetwork
{
public:
  LefDefNetwork();
  ~LefDefNetwork();
  void initState(Report *report,
		 Debug *debug);
  virtual void clear();
  void setDivider(char divider);
  const char *defFilename() { return def_filename_; }
  void setDefFilename(const char *filename);
  // dbu/micron
  float defUnits() const { return def_units_; }
  void setDefUnits(int def_units);
  double dbuToMeters(int dbu) const;
  int metersToDbu(double dist) const;
  Library *makeLefLibrary(const char *name,
			  const char *filename);
  Library *lefLibrary();
  Library *lefLibrary(Cell *cell);
  lefiSite *findLefSite(const char *name);
  void makeLefSite(lefiSite *site);

  lefiMacro *lefMacro(Cell *cell);
  void setLefMacro(Cell *cell,
		   lefiMacro *lef_macro);
  Cell *lefCell(LibertyCell *cell);
  bool isLefCell(Cell *cell) const;

  void initTopInstancePins();
  Instance *makeDefComponent(Cell *cell,
			     const char *name,
			     defiComponent *def_component);
  defiComponent *defComponent(Instance *inst) const;
  // In DBUs.
  DefPt location(const Pin *pin) const;
  void setLocation(Instance *instance,
		   DefPt location);
  // Set top level pin/port location.
  void setLocation(Port *port,
		   DefPt location);
  bool isPlaced(const Pin *pin) const;

  void connectedPins(const Net *net,
		     PinSeq &pins);
  using ConcreteNetwork::connect;

protected:
  const char *def_filename_;
  Library *lef_library_;
  int def_units_;		// dbu/micron
  DefPortLocations port_locations_;
  InstanceDefComponentMap def_component_map_;
  CellLefMacroMap lef_macro_map_;
  LefSiteMap lef_size_map_;
};

} // namespace
#endif
