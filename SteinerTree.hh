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

#ifndef RESIZER_STEINER_TREE_H
#define RESIZER_STEINER_TREE_H

#include <string>
#include "Hash.hh"
#include "LefDefNetwork.hh"

#define FLUTE_2_2

#ifdef FLUTE_2_2
  #include "flute.h"
  typedef DBU FluteDbu;
  namespace sta { 
    using Flute::Tree;
    using Flute::Branch;
    using Flute::readLUT;
    using Flute::flute;
    using Flute::printtree;
  };
#endif

#ifdef FLUTE_3_1
  #define DTYPE sta::DefDbu
  #include "flute.h"
  typedef sta::DefDbu FluteDbu;
  // Remove flute turds.
  #undef max
  #undef min
  #undef abs
#endif

namespace sta {

using std::string;

class SteinerTree;
class LefDefNetwork;

typedef int SteinerPt;
typedef Vector<SteinerPt> SteinerPtSeq;

bool
readFluteInits(string dir);
SteinerTree *
makeSteinerTree(const Net *net,
		bool find_left_rights,
		LefDefNetwork *network);

class DefPtHash
{
public:
  Hash operator()(const DefPt &pt) const
  {
    Hash hash = hash_init_value;
    hashIncr(hash, pt.x());
    hashIncr(hash, pt.y());
    return hash;
  }
};

class DefPtEqual
{
public:
  bool operator()(const DefPt &pt1,
		  const DefPt &pt2) const
  {
    return pt1.x() == pt2.x()
      && pt1.y() == pt2.y();
  }
};

// Wrapper for Tree
class SteinerTree
{
public:
  SteinerTree() {}
  PinSeq &pins() { return pins_; }
  void setTree(Tree tree,
	       int pin_map[]);
  int pinCount() const { return pins_.size(); }
  int branchCount() const;
  void branch(int index,
	      // Return values.
	      DefPt &pt1,
	      Pin *&pin1,
	      int &steiner_pt1,
	      DefPt &pt2,
	      Pin *&pin2,
	      int &steiner_pt2,
	      int &wire_length);
  void report(const Network *network);
  // The steiner points can be in the same location as the pins.
  void findSteinerPtAliases();
  // Return a pin in the same location as the steiner pt if it exists.
  Pin *steinerPtAlias(SteinerPt pt);
  // Return the steiner pt connected to the driver pin.
  SteinerPt drvrPt(const Network *network) const;

  const char *name(SteinerPt pt,
		   const Network *network);
  SteinerPt adjacentPt(SteinerPt pt);
  Pin *pin(SteinerPt pt) const;
  bool isLoad(SteinerPt pt,
	      const Network *network);
  DefPt location(SteinerPt pt) const;
  SteinerPt left(SteinerPt pt);
  SteinerPt right(SteinerPt pt);
  void findLeftRights(const Network *network);
  static SteinerPt null_pt;

protected:
  void findLeftRights(SteinerPt from,
		      SteinerPt to,
		      SteinerPtSeq &adj1,
		      SteinerPtSeq &adj2);
  void findLeftRights(SteinerPt from,
		      SteinerPt to,
		      SteinerPt adj,
		      SteinerPtSeq &adj1,
		      SteinerPtSeq &adj2);
  void checkSteinerPt(SteinerPt pt) const;

  Tree tree_;
  PinSeq pins_;
  // steiner pt (tree vertex index) -> pin index
  SteinerPtSeq pin_map_;
  // location -> pin
  UnorderedMap<DefPt, Pin*, DefPtHash, DefPtEqual> steiner_pt_pin_alias_map_;
  SteinerPtSeq left_;
  SteinerPtSeq right_;
};

} // namespace
#endif
