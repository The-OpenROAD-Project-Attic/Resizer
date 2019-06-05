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

#include <fstream>
#include "Machine.hh"
#include "Debug.hh"
#include "PortDirection.hh"
#include "TimingRole.hh"
#include "Units.hh"
#include "Liberty.hh"
#include "TimingArc.hh"
#include "TimingModel.hh"
#include "Corner.hh"
#include "DcalcAnalysisPt.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"
#include "Parasitics.hh"
#include "Search.hh"
#include "LefDefNetwork.hh"
#include "Resizer.hh"
#include "flute.h"

// Outstanding issues
//  Instance levelization and resizing to target slew only support single output gates
//  flute wants to read files which prevents having a stand-alone executable
//  DefRead uses printf for errors.
//  multi-corner support?

namespace sta {

using std::abs;

static Pin *
singleOutputPin(const Instance *inst,
		Network *network);
bool
fileExists(const std::string &filename);

Resizer::Resizer() :
  Sta()
{
}

void
Resizer::makeNetwork()
{
  network_ = new LefDefNetwork();
}

LefDefNetwork *
Resizer::lefDefNetwork()
{
  return dynamic_cast<LefDefNetwork*>(network_);
}

////////////////////////////////////////////////////////////////

class InstanceOutputLevelLess
{
public:
  InstanceOutputLevelLess(Network *network,
			  Graph *graph);
  bool operator()(const Instance *inst1,
		  const Instance *inst2) const;

protected:
  Level outputLevel(const Instance *inst) const;

  Network *network_;
  Graph *graph_;
};

InstanceOutputLevelLess::InstanceOutputLevelLess(Network *network,
						 Graph *graph) :
  network_(network),
  graph_(graph)
{
}

bool
InstanceOutputLevelLess::operator()(const Instance *inst1,
				    const Instance *inst2) const
{
  return outputLevel(inst1) < outputLevel(inst2);
}

Level
InstanceOutputLevelLess::outputLevel(const Instance *inst) const
{
  Pin *output = singleOutputPin(inst, network_);
  if (output) {
    Vertex *vertex = graph_->pinDrvrVertex(output);
    return vertex->level();
  }
  else
    return 0;
}

////////////////////////////////////////////////////////////////

void
Resizer::resize(float wire_res_per_length,
		float wire_cap_per_length,
		Corner *corner)
{
  initCorner(corner);

  // Disable incremental timing.
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();

  ensureLevelized();
  sortInstancesByLevel();

  // Find a target slew for the libraries and then
  // a target load for each cell that gives the target slew.
  ensureTargetLoads();

  if (wire_cap_per_length > 0.0)
    makeNetParasitics(wire_res_per_length, wire_cap_per_length);

  resizeToTargetSlew();
}

void
Resizer::initCorner(Corner *corner)
{
  corner_ = corner;
  min_max_ = MinMax::max();
  dcalc_ap_ = corner->findDcalcAnalysisPt(min_max_);
  pvt_ = dcalc_ap_->operatingConditions();
}

void
Resizer::sortInstancesByLevel()
{
  LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    Instance *leaf = leaf_iter->next();
    level_insts_.push_back(leaf);
  }
  sort(level_insts_, InstanceOutputLevelLess(network_, graph_));
  delete leaf_iter;
}

void
Resizer::resizeToTargetSlew(Instance *inst,
			    Corner *corner)
{
  initCorner(corner);
  ensureTargetLoads();
  resizeToTargetSlew1(inst);
}

void
Resizer::resizeToTargetSlew()
{
  // Resize by in reverse level order.
  for (int i = level_insts_.size() - 1; i >= 0; i--) {
    Instance *inst = level_insts_[i];
    resizeToTargetSlew1(inst);
  }
}

void
Resizer::resizeToTargetSlew1(Instance *inst)
{
  LibertyCell *cell = network_->libertyCell(inst);
  if (cell) {
    Pin *output = singleOutputPin(inst, network_);
    // Only resize single output gates for now.
    if (output) {
      // Includes net parasitic capacitance.
      float load_cap = graph_delay_calc_->loadCap(output, dcalc_ap_);
      LibertyCell *best_cell = nullptr;
      float best_ratio = 0.0;
      auto equiv_cells = cell->equivCells();
      if (equiv_cells) {
	for (auto target_cell : *equiv_cells) {
	  float target_load = (*target_load_map_)[target_cell];
	  float ratio = target_load / load_cap;
	  if (ratio > 1.0)
	    ratio = 1.0 / ratio;
	  if (ratio > best_ratio) {
	    best_ratio = ratio;
	    best_cell = target_cell;
	  }
	}
	if (best_cell && best_cell != cell) {
	  debugPrint3(debug_, "resizer", 2, "%s %s -> %s\n",
		      sdc_network_->pathName(inst),
		      cell->name(),
		      best_cell->name());
	  replaceCell(inst, best_cell);
	}
      }
    }
  }
}

static Pin *
singleOutputPin(const Instance *inst,
		Network *network)
{
  Pin *output = nullptr;
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network->direction(pin)->isOutput()) {
      if (output) {
// Already found one.
	delete pin_iter;
	return nullptr;
      }
      output = pin;
    }
  }
  delete pin_iter;
  return output;
}

////////////////////////////////////////////////////////////////

void
Resizer::ensureTargetLoads()
{
  if (target_load_map_ == nullptr)
    findTargetLoads();
}

// Find the target load for each library cell that gives the target slew.
void
Resizer::findTargetLoads()
{
  // Find target slew across all buffers in the libraries.
  float tgt_slews[TransRiseFall::index_count];
  findBufferTargetSlews(tgt_slews);

  target_load_map_ = new CellTargetLoadMap;
  LibertyLibraryIterator *lib_iter = network_->libertyLibraryIterator();
  while (lib_iter->hasNext()) {
    LibertyLibrary *lib = lib_iter->next();
    findTargetLoads(lib, tgt_slews);
  }
  delete lib_iter;
}

void
Resizer::findTargetLoads(LibertyLibrary *library,
			 float *tgt_slews)
{
  LibertyCellIterator cell_iter(library);
  while (cell_iter.hasNext()) {
    auto cell = cell_iter.next();
    LibertyCellTimingArcSetIterator arc_set_iter(cell);
    float target_load_sum = 0.0;
    int arc_count = 0;
    while (arc_set_iter.hasNext()) {
      auto arc_set = arc_set_iter.next();
      auto role = arc_set->role();
      if (!role->isTimingCheck()
	  && role != TimingRole::tristateDisable()
	  && role != TimingRole::tristateEnable()) {
	TimingArcSetArcIterator arc_iter(arc_set);
	while (arc_iter.hasNext()) {
	  TimingArc *arc = arc_iter.next();
	  TransRiseFall *in_tr = arc->fromTrans()->asRiseFall();
	  float arc_target_load = findTargetLoad(cell, arc,
						 tgt_slews[in_tr->index()]);
	  target_load_sum += arc_target_load;
	  arc_count++;
	}
      }
    }
    float target_load = (arc_count > 0) ? target_load_sum / arc_count : 0.0;
    (*target_load_map_)[cell] = target_load;
    debugPrint2(debug_, "resizer", 3, "%s target_load = %.2e\n",
		cell->name(),
		target_load);
  }
}

// Find the load capacitance that will cause the output slew
// to be equal to in_slew.
float
Resizer::findTargetLoad(LibertyCell *cell,
			TimingArc *arc,
			Slew in_slew)
{
  GateTimingModel *model = dynamic_cast<GateTimingModel*>(arc->model());
  if (model) {
    float cap_init = 1.0e-12;  // 1pF
    float cap_tol = cap_init * .001; // .1%
    float load_cap = cap_init;
    float cap_step = cap_init;
    while (cap_step > cap_tol) {
      ArcDelay arc_delay;
      Slew arc_slew;
      model->gateDelay(cell, pvt_, 0.0, load_cap, 0.0, false,
		       arc_delay, arc_slew);
      if (arc_slew > in_slew) {
	load_cap -= cap_step;
	cap_step /= 2.0;
      }
      load_cap += cap_step;
    }
    return load_cap;
  }
  return 0.0;
}

////////////////////////////////////////////////////////////////

void
Resizer::findBufferTargetSlews(// Return values.
			       float *tgt_slews)
{
  tgt_slews[TransRiseFall::riseIndex()] = 0.0;
  tgt_slews[TransRiseFall::fallIndex()] = 0.0;
  int counts[TransRiseFall::index_count]{0};
  
  LibertyLibraryIterator *lib_iter = network_->libertyLibraryIterator();
  while (lib_iter->hasNext()) {
    LibertyLibrary *lib = lib_iter->next();
    findBufferTargetSlews(lib, tgt_slews, counts);
  }
  delete lib_iter;

  tgt_slews[TransRiseFall::riseIndex()] /= counts[TransRiseFall::riseIndex()];
  tgt_slews[TransRiseFall::fallIndex()] /= counts[TransRiseFall::fallIndex()];
}

void
Resizer::findBufferTargetSlews(LibertyLibrary *library,
			       // Return values.
			       float *slews,
			       int *counts)
{
  for (auto buffer : *library->buffers()) {
    LibertyPort *input, *output;
    buffer->bufferPorts(input, output);
    auto arc_sets = buffer->timingArcSets(input, output);
    if (arc_sets) {
      for (auto arc_set : *arc_sets) {
	TimingArcSetArcIterator arc_iter(arc_set);
	while (arc_iter.hasNext()) {
	  TimingArc *arc = arc_iter.next();
	  GateTimingModel *model = dynamic_cast<GateTimingModel*>(arc->model());
	  TransRiseFall *in_tr = arc->fromTrans()->asRiseFall();
	  TransRiseFall *out_tr = arc->toTrans()->asRiseFall();
	  float in_cap = input->capacitance(in_tr, min_max_);
	  float load_cap = in_cap * 10.0; // "factor debatable"
	  ArcDelay arc_delay;
	  Slew arc_slew;
	  model->gateDelay(buffer, pvt_, 0.0, load_cap, 0.0, false,
			   arc_delay, arc_slew);
	  model->gateDelay(buffer, pvt_, arc_slew, load_cap, 0.0, false,
			   arc_delay, arc_slew);
	  slews[out_tr->index()] += arc_slew;
	  counts[out_tr->index()]++;
	}
      }
    }
  }
}

////////////////////////////////////////////////////////////////

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

// Wrapper for Flute::Tree
class SteinerTree
{
public:
  SteinerTree() {}
  ~SteinerTree();
  PinSeq &pins() { return pins_; }
  void setTree(Flute::Tree tree,
	       int pin_map[]);
  int branchCount();
  void branch(int index,
	      // Return values.
	      DefPt &pt1,
	      Pin *&pin1,
	      int &steiner_pt1,
	      DefPt &pt2,
	      Pin *&pin2,
	      int &steiner_pt2,
	      int &wire_length);
  void reportBranches(const Network *network);
  // The steiner tree is binary so the steiner points can be in the
  // same location as the pins.
  void findSteinerPtAliases();
  // Return a pin in the same location as the steiner pt if it exists.
  Pin *steinerPtAlias(int steiner_pt);

private:
  Flute::Tree tree_;
  PinSeq pins_;
  // tree vertex index -> pin index
  Vector<int> pin_map_;
  UnorderedMap<DefPt, Pin*, DefPtHash, DefPtEqual> steiner_pt_pin_alias_map_;
};

SteinerTree::~SteinerTree()
{
}

void
SteinerTree::setTree(Flute::Tree tree,
		     int pin_map[])
{
  tree_ = tree;

  // Invert the steiner vertex to pin index map.
  int pin_count = pins_.size();
  pin_map_.resize(pin_count);
  for (int i = 0; i < pin_count; i++)
    pin_map_[pin_map[i]] = i;
}

int
SteinerTree::branchCount()
{
  // branch[deg * 2 - 2 - 1] is the root that has branch.n == branch index.
  return tree_.deg * 2 - 3;
}

void
SteinerTree::branch(int index,
		    // Return values.
		    DefPt &pt1,
		    Pin *&pin1,
		    int &steiner_pt1,
		    DefPt &pt2,
		    Pin *&pin2,
		    int &steiner_pt2,
		    int &wire_length)
{
  Flute::Branch &branch_pt1 = tree_.branch[index];
  int index2 = branch_pt1.n;
  Flute::Branch &branch_pt2 = tree_.branch[index2];
  pt1 = DefPt(branch_pt1.x, branch_pt1.y);
  if (index < pins_.size() ){
    pin1 = pins_[pin_map_[index]];
    steiner_pt1 = 0;
  }
  else {
    pin1 = nullptr;
    steiner_pt1 = index;
  }

  pt2 = DefPt(branch_pt2.x, branch_pt2.y);
  if (index2 < pins_.size()) {
    pin2 = pins_[pin_map_[index2]];
    steiner_pt2 = 0;
  }
  else {
    pin2 = nullptr;
    steiner_pt2 = index2;
  }

  wire_length = abs(branch_pt1.x - branch_pt2.x)
    + abs(branch_pt1.y - branch_pt2.y);
}

void
SteinerTree::reportBranches(const Network *network)
{
  int branch_count = branchCount();
  for(int i = 0; i < branch_count; i++) {
    DefPt pt1, pt2;
    Pin *pin1, *pin2;
    int steiner_pt1, steiner_pt2;
    int wire_length;
    branch(i,
	   pt1, pin1, steiner_pt1,
	   pt2, pin2, steiner_pt2,
	   wire_length);
    printf(" branch %s (%d %d) - %s (%d %d) wire_length = %d\n",
	   pin1 ? network->pathName(pin1) : stringPrintTmp("S%d", steiner_pt1),
	   pt1.x(),
	   pt1.y(),
	   pin2 ? network->pathName(pin2) : stringPrintTmp("S%d", steiner_pt2),
	   pt2.x(),
	   pt2.y(),
	   wire_length);
  }
}

void
SteinerTree::findSteinerPtAliases()
{
  int pin_count = pins_.size();
  for(int i = 0; i < pin_count; i++) {
    Flute::Branch &branch_pt1 = tree_.branch[i];
    // location -> pin
    steiner_pt_pin_alias_map_[DefPt(branch_pt1.x, branch_pt1.y)] = pins_[pin_map_[i]];
  }
}

Pin *
SteinerTree::steinerPtAlias(int steiner_pt)
{
  Flute::Branch &branch_pt = tree_.branch[steiner_pt];
  return steiner_pt_pin_alias_map_[DefPt(branch_pt.x, branch_pt.y)];
}

////////////////////////////////////////////////////////////////

// Flute reads look up tables from local files. gag me.
void
Resizer::initFlute(const char *resizer_path)
{
  string resizer_dir = resizer_path;
  // One directory level up from /bin or /build to find /etc.
  auto last_slash = resizer_dir.find_last_of("/");
  if (last_slash != string::npos) {
    resizer_dir.erase(last_slash);
    last_slash = resizer_dir.find_last_of("/");
    if (last_slash != string::npos) {
      resizer_dir.erase(last_slash);
      if (readFluteInits(resizer_dir))
	return;
    }
    else {
      // try ./etc2
      resizer_dir = ".";
      if (readFluteInits(resizer_dir))
	return;
    }
  }
  // try ../etc
  resizer_dir = "..";
  if (readFluteInits(resizer_dir))
    return;
  printf("Error: could not find FluteLUT files POWV9.dat and PORT9.dat.\n");
  exit(EXIT_FAILURE);
}

bool
Resizer::readFluteInits(string dir)
{
  string flute_path1 = dir;
  string flute_path2 = dir;
  flute_path1 += "/etc/POWV9.dat";
  flute_path2 += "/etc/PORT9.dat";
  if (fileExists(flute_path1) && fileExists(flute_path2)) {
    Flute::readLUT(flute_path1.c_str(), flute_path2.c_str());
    return true;
  }
  else
    return false;
}

// c++17 std::filesystem::exists
bool
fileExists(const std::string &filename)
{
  std::ifstream stream(filename.c_str());
  return stream.good();
}


SteinerTree *
Resizer::makeSteinerTree(const Net *net)
{
  LefDefNetwork *network = lefDefNetwork();
  debugPrint1(debug_, "steiner", 1, "Net %s\n", network->pathName(net));

  SteinerTree *tree = new SteinerTree();
  PinSeq &pins = tree->pins();
  network->connectedPins(net, pins);
  int pin_count = pins.size();
  if (pin_count >= 2) {
    DBU x[pin_count];
    DBU y[pin_count];
    // map[pin_index] -> steiner tree vertex index
    int pin_map[pin_count];

    for (int j = 0; j < pin_count; j++) {
      Pin *pin = pins[j];
      DefPt loc = network->location(pin);
      x[j] = loc.x();
      y[j] = loc.y();
      debugPrint3(debug_, "steiner", 3, "%s (%d %d)\n",
		  network->pathName(pin), loc.x(), loc.y());
    }

    Flute::Tree stree = Flute::flute(pin_count, x, y, FLUTE_ACCURACY, pin_map);
    tree->setTree(stree, pin_map);
    if (debug_->check("steiner", 3)) {
      Flute::printtree(stree);
      printf("pin map\n");
      for (int i = 0; i < pin_count; i++)
	printf(" %d -> %d\n", i, pin_map[i]);
    }
    if (debug_->check("steiner", 2)) 
      tree->reportBranches(network);
    return tree;
  }
  else
    return nullptr;
}

////////////////////////////////////////////////////////////////

void
Resizer::makeNetParasitics(float wire_res_per_length,
			   float wire_cap_per_length)
{
  NetIterator *net_iter = network_->netIterator(network_->topInstance());
  while (net_iter->hasNext()) {
    Net *net = net_iter->next();
    makeNetParasitics(net, wire_res_per_length, wire_cap_per_length);
  }
  delete net_iter;

  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
}

void
Resizer::makeNetParasitics(const Net *net,
			   float wire_res_per_length,
			   float wire_cap_per_length)
{
  Corner *corner = cmd_corner_;
  const MinMax *min_max = MinMax::max();
  LefDefNetwork *network = lefDefNetwork();
  const ParasiticAnalysisPt *ap = corner->findParasiticAnalysisPt(min_max);
  SteinerTree *tree = makeSteinerTree(net);
  if (tree) {
    tree->findSteinerPtAliases();
    Parasitic *parasitic = parasitics_->makeParasiticNetwork(net, false, ap);
    int branch_count = tree->branchCount();
    for (int i = 0; i < branch_count; i++) {
      DefPt pt1, pt2;
      Pin *pin1, *pin2;
      int steiner_pt1, steiner_pt2;
      int wire_length_dbu;
      tree->branch(i,
		   pt1, pin1, steiner_pt1,
		   pt2, pin2, steiner_pt2,
		   wire_length_dbu);
      ParasiticNode *n1 = findParasiticNode(tree, parasitic, net, pin1, steiner_pt1);
      ParasiticNode *n2 = findParasiticNode(tree, parasitic, net, pin2, steiner_pt2);
      if (wire_length_dbu == 0)
	// Use a small resistor to keep the connectivity intact.
	parasitics_->makeResistor(nullptr, n1, n2, 1.0e-3, ap);
      else {
	float wire_length = network->dbuToMeters(wire_length_dbu);
	float wire_cap = wire_length * wire_cap_per_length;
	float wire_res = wire_length * wire_res_per_length;
	// Make pi model for the wire.
	debugPrint5(debug_, "resizer", 3, "pi %s c2=%s rpi=%s c1=%s %s\n",
		    parasitics_->name(n1),
		    units_->capacitanceUnit()->asString(wire_cap / 2.0),
		    units_->resistanceUnit()->asString(wire_res),
		    units_->capacitanceUnit()->asString(wire_cap / 2.0),
		    parasitics_->name(n2));
	parasitics_->incrCap(n1, wire_cap / 2.0, ap);
	parasitics_->makeResistor(nullptr, n1, n2, wire_res, ap);
	parasitics_->incrCap(n2, wire_cap / 2.0, ap);
      }
    }
    delete tree;
  }
}

ParasiticNode *
Resizer::findParasiticNode(SteinerTree *tree,
			   Parasitic *parasitic,
			   const Net *net,
			   const Pin *pin,
			   int steiner_pt)
{
  if (pin == nullptr)
    // If the steiner pt is on top of a pin, use the pin instead.
    pin = tree->steinerPtAlias(steiner_pt);
  if (pin)
    return parasitics_->ensureParasiticNode(parasitic, pin);
  else 
    return parasitics_->ensureParasiticNode(parasitic, net, steiner_pt);
}

////////////////////////////////////////////////////////////////

#if 0
class RebufferOption
{
public:
  RebufferOption();

private:
  enum {sink junction wire } Type;
  float cap_;
  Required required_;
  DefPt location_;
  RebufferOption ref;
  RebufferOption ref2;
};

void
Resizer::rebuffer(float cap_limit)
{
  for (int i = level_insts.size() - 1; i >= 0; i--) {
    Instance *inst = level_insts[i];
    LibertyCell *cell = network_->libertyCell(inst);
    if (cell) {
      Pin *output = singleOutputPin(inst, network_);
      if (output) {
	float load_cap = graph_delay_calc_->loadCap(output, dcalc_ap);
	if (load_cap > cap_limit)
	  rebuffer(output);
      }
    }
  }
}
 
void
Resizer::rebuffer(const Pin *output)
{
  Net *net = network_->net(output);
  SteinerTree *tree = makeSteinerTree(net);
  RebufferOption Z = bottom_up(Stree.root);
  Tbest = -infinity;
  for(auto p : Z) {
    Tb = p.req - Dbuf - Rbuf * p.cap;
    if (Tb > Tbest) {
      Tbest = Tb;
      best = p;
    }
  }
  top_down(best, net);
}

// The routing tree is represented a binary tree with the sinks being the leaves
// of the tree, the junctions being the Steiner nodes and the root being the
// source of the net.
Set<RebufferOption>
bottom_up(SteinerTree k)
{
  if (is_sink(k)) {
    z = new option;
    z.cap = input_cap(k);
    z.req = required_time(k);
    z.xy = k.xy;
    z.sink = k.pin;
    z.type = SINK;
    Z = { z } // capacitance of the sink and the required time
  } else {
    Zl = bottom_up(k.left);
    Zr = bottom_up(k.right);
    // Now combine the options from both branches
    Z = { };
    for p in Zl {
	for (q in Zr) {
	  z = new option;
	  z.cap = p.cap + q.cap;
	  z.req = min(p.req, q.req);
	  z.xy = k.xy;
	  z.ref = p;
	  z.ref2 = q;
	  z.type = JUNCTION;
	}
    }

    // Prune the options
    for p in Z {
	for q in Z {
	    if(Tp < Tq &amp;&amp; Lp &lt; Lq) { // if q strictly worse than p
	      delete q; // remove solution q
	    }
	}
    }
  }
  Z1 = add_wire_and_buffer(Z, k);
  return Z1;
}

define top_down(option choice, net) {
  switch(choice.type) {
  case BUFFER:
    net2 = new NET;
    buf = new BUFFER;
    connect(buf.in, net);
    connect(buf.out, net2);
    place(buf, choice.xy);
    top_down(choice.ref, net2);
    break;
  case WIRE:
    top_down(choice.ref, net);
    break;
  case JUNCTION:
    top_down(choice.ref, net);
    top_down(choice.ref2, net);
    break;
  case SINK:
    connect(choice.sink, net);
  }
}

RebufferOption
Resizer::addWireAndBuffer(RebufferOption Z,
			  SteinerTree k) 
{
  Z1 = { };
  best = -infinity;
  for (p in Z) {
    z = new option;
    z.req = p.req - rc_delay(k, k.left); // account for wire delay
    z.cap = p.cap + wireload(k, k.left); // account for wire load
    z.ref = p;
    z.xy = p.xy;
    z.type = WIRE;
    Z1 = Z1 U { z };
    // We could add options of different buffer drive strengths here
    // Which would have different delay Dbuf and input cap Lbuf
    // for simplicity we only consider one size of buffer
    rt = z.req - Dbuf - Rbuf * z.cap;
    if(rt = best) {
      best = rt;
      best_ref = p;
    }
  }
  z = new option;
  z.req = best;
  z.cap = Cbuf; // buffer input cap
  z.xy = best_ref.xy;
  z.ref = best_ref;
  z.type = BUFFER;
  Z1 = Z1 U { z };
  return Z1;
}

#endif

};
