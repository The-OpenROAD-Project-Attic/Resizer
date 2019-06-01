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

#ifndef STA_LEF_DEF_NETWORK_H
#define STA_LEF_DEF_NETWORK_H

#include "UnorderedMap.hh"
#include "ConcreteLibrary.hh"
#include "ConcreteNetwork.hh"
#include "lefiMacro.hpp"
#include "defiComponent.hpp"
#include "defiNet.hpp"

namespace sta {

class LefMacro;
class DefComponent;
class DefNet;
class LefPin;

// No need to specializing ConcreteLibrary at this point.
typedef ConcreteLibrary LefLibrary;

typedef UnorderedMap<Cell*, LibertyCell*> LibertyCellMap;

class LefDefNetwork : public ConcreteNetwork
{
public:
  LefDefNetwork();
  ~LefDefNetwork();
  virtual void clear();
  void setDivider(char divider);
  const char *filename() { return filename_; }
  void setFilename(const char *filename);

  virtual Library *makeLibrary(const char *name,
			       const char *filename);
  virtual Cell *makeCell(Library *library,
			 const char *name,
			 bool is_leaf,
			 const char *filename);
  virtual LibertyCell *libertyCell(Cell *cell) const;
  virtual LibertyPort *libertyPort(Port *port) const;

  Library *lefLibrary();
  LefLibrary *lefLib() { return lef_library_; }
  void initTopInstancePins();
  virtual Instance *makeInstance(Cell *cell,
				 const char *name,
				 Instance *parent);
  virtual Instance *makeInstance(LibertyCell *cell,
				 const char *name,
				 Instance *parent);
  DefComponent *makeDefComponent(Cell *cell,
				 const char *name,
				 defiComponent *def_component);
  virtual void replaceCell(Instance *inst,
			   LibertyCell *cell);
  // DEF instances all have top_instance as the parent.
  Instance *findInstance(const char *name) const;
  Net *makeNet(const char *name,
	       defiNet *def_net);
  int pinCount(Net *net);

protected:
  Library *lefToSta(LefLibrary *lib) const;
  LefLibrary *staToLef(Library *lib) const;
  Cell *lefToSta(LefMacro *macro) const;
  LefMacro *staToLef(Cell *cell) const;
  DefComponent *staToDef(Instance *inst) const;
  Instance *defToSta(DefComponent *component) const;
  Net *defToSta(DefNet *net) const;

  const char *filename_;
  LefLibrary *lef_library_;

  using NetworkEdit::makeNet;
  using NetworkEdit::makeInstance;
};

////////////////////////////////////////////////////////////////

class LefMacro : public ConcreteCell
{
public:
  LefPin *makeLefPin(const char *name);
  void setLefMacro(lefiMacro *lef_macro);
  LibertyCell *libertyCell() { return liberty_cell_; }

protected:
  LefMacro(ConcreteLibrary *library,
	   const char *name,
	   bool is_leaf,
	   const char *filename);
  void setLibertyCell(LibertyCell *cell);

  lefiMacro *lef_macro_;
  LibertyCell *liberty_cell_;

  friend class LefDefNetwork;
};

class LefPin : public ConcretePort
{
public:

protected:
  LefPin(LefMacro *macro,
	 const char *name);

  friend class LefMacro;
};

class DefComponent : public ConcreteInstance
{
public:
  LefMacro *lefMacro();
  defiComponent *defComponent() { return def_component_; }

protected:
  DefComponent(ConcreteCell *cell,
	       const char *name,
	       ConcreteInstance *top_instance,
	       defiComponent *def_component);

private:
  defiComponent *def_component_;

  friend class LefDefNetwork;
};

class DefNet : public ConcreteNet
{
public:

protected:
  DefNet(const char *name,
	 ConcreteInstance *top_instance,
	 defiNet *def_net);
  defiNet *defNet(){ return def_net_; }

private:
  defiNet *def_net_;

  friend class LefDefNetwork;
};

} // namespace
#endif
