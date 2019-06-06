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
#include "ArcDelayCalc.hh"
#include "GraphDelayCalc.hh"
#include "Parasitics.hh"
#include "PathVertex.hh"
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
  Pin *steinerPtAlias(SteinerPt pt);
  // Return the steiner pt connected to the driver pin.
  SteinerPt drvrPt(const Network *network) const;
  Pin *pin(SteinerPt k);
  bool isLoad(SteinerPt pt,
	      const Network *network);
  DefPt location(SteinerPt pt);
  SteinerPt left(SteinerPt pt);
  SteinerPt right(SteinerPt pt);

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
    pin1 = pin(index);
    steiner_pt1 = 0;
  }
  else {
    pin1 = nullptr;
    steiner_pt1 = index;
  }

  pt2 = DefPt(branch_pt2.x, branch_pt2.y);
  if (index2 < pins_.size()) {
    pin2 = pin(index2);
    steiner_pt2 = 0;
  }
  else {
    pin2 = nullptr;
    steiner_pt2 = index2;
  }

  wire_length = abs(branch_pt1.x - branch_pt2.x)
    + abs(branch_pt1.y - branch_pt2.y);
}

Pin *
SteinerTree::pin(SteinerPt k)
{
  if (k < pins_.size())
    return pins_[pin_map_[k]];
  else
    return nullptr;
}

void
SteinerTree::reportBranches(const Network *network)
{
  int branch_count = branchCount();
  for (int i = 0; i < branch_count; i++) {
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
SteinerTree::steinerPtAlias(SteinerPt pt)
{
  Flute::Branch &branch_pt = tree_.branch[pt];
  return steiner_pt_pin_alias_map_[DefPt(branch_pt.x, branch_pt.y)];
}

SteinerPt
SteinerTree::left(SteinerPt pt)
{
  // MIA
  return 0;
}

SteinerPt
SteinerTree::right(SteinerPt pt)
{
  // MIA
  return 0;
}

SteinerPt
SteinerTree::drvrPt(const Network *network) const
{
  int pin_count = pins_.size();
  for (int i = 0; i < pin_count; i++) {
    Pin *pin = pins_[pin_map_[i]];
    if (network->isDriver(pin)) {
      Flute::Branch &branch_pt = tree_.branch[i];
      return branch_pt.n;
    }
  }
  return -1;
}

bool
SteinerTree::isLoad(SteinerPt pt,
		    const Network *network)
{
  Pin *pin = this->pin(pt);
  return pin && network->isLoad(pin);
}

DefPt
SteinerTree::location(SteinerPt pt)
{
  Flute::Branch &branch_pt = tree_.branch[pt];
  return DefPt(branch_pt.x, branch_pt.y);
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
  connectedPins(net, pins);
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

void
Resizer::connectedPins(const Net *net,
		       PinSeq &pins)
{
  NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    pins.push_back(pin);
  }
  delete pin_iter;
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

class RebufferOption
{
public:
  enum Type { sink, junction, wire, buffer };

  RebufferOption(Type type,
		 float cap,
		 Required required,
		 const Pin *load_pin,
		 DefPt location,
		 RebufferOption *ref,
		 RebufferOption *ref2);
  ~RebufferOption();
  Type type() const { return type_; }
  float cap() const { return cap_; }
  Required required() const { return required_; }
  Required bufferRequired(LibertyCell *buffer_cell,
			  const Pin *drvr_pin,
			  Resizer *resizer) const;
  DefPt location() const { return location_; }
  const Pin *loadPin() const { return load_pin_; }
  RebufferOption *ref() const { return ref_; }
  RebufferOption *ref2() const { return ref2_; }

private:
  Type type_;
  float cap_;
  Required required_;
  const Pin *load_pin_;
  DefPt location_;
  RebufferOption *ref_;
  RebufferOption *ref2_;
};

RebufferOption::RebufferOption(Type type,
			       float cap,
			       Required required,
			       const Pin *load_pin,
			       DefPt location,
			       RebufferOption *ref,
			       RebufferOption *ref2) :
  type_(type),
  cap_(cap),
  required_(required),
  load_pin_(load_pin),
  location_(location),
  ref_(ref),
  ref2_(ref)
{
}

RebufferOption::~RebufferOption()
{
}

Required
RebufferOption::bufferRequired(LibertyCell *buffer_cell,
			       const Pin *drvr_pin,
			       Resizer *resizer) const
{
  return required_ - resizer->bufferDelay(buffer_cell, drvr_pin, cap_);
}

////////////////////////////////////////////////////////////////

void
Resizer::rebuffer(float cap_limit,
		  LibertyCell *buffer_cell)
{
  for (int i = level_insts_.size() - 1; i >= 0; i--) {
    Instance *inst = level_insts_[i];
    LibertyCell *cell = network_->libertyCell(inst);
    if (cell) {
      Pin *output = singleOutputPin(inst, network_);
      if (output) {
	float load_cap = graph_delay_calc_->loadCap(output, dcalc_ap_);
	LibertyPort *port = network_->libertyPort(output);
	float cap_limit;
	bool exists;
	port->capacitanceLimit(min_max_, cap_limit, exists);
	if (exists && load_cap > cap_limit)
	  rebuffer(output, buffer_cell);
      }
    }
  }
}
 
void
Resizer::rebuffer(const Pin *drvr_pin,
		  LibertyCell *buffer_cell)
{
  Net *net = network_->net(drvr_pin);
  SteinerTree *tree = makeSteinerTree(net);
  SteinerPt drvr_pt = tree->drvrPt(network_);
  if (drvr_pt >= 0) {
    RebufferOptionSeq Z = rebufferBottomUp(tree, drvr_pt,
					   drvr_pin, buffer_cell);
    Required Tbest = -INF;
    RebufferOption *best = nullptr;
    for (auto p : Z) {
      Required Tb = p->bufferRequired(buffer_cell, drvr_pin, this);
      if (Tb > Tbest) {
	Tbest = Tb;
	best = p;
      }
    }
    rebufferTopDown(best, net, buffer_cell);
  }
}

// The routing tree is represented a binary tree with the sinks being the leaves
// of the tree, the junctions being the Steiner nodes and the root being the
// source of the net.
RebufferOptionSeq
Resizer::rebufferBottomUp(SteinerTree *tree,
			  SteinerPt k,
			  const Pin *drvr_pin,
			  LibertyCell *buffer_cell)
{
  RebufferOptionSeq Z;
  if (tree->isLoad(k, network_)) {
    const Pin *load_pin = tree->pin(k);
    // Load capacitance and required time.
    RebufferOption *z = new RebufferOption(RebufferOption::Type::sink,
					   pinCapacitance(load_pin),
					   pinRequired(load_pin),
					   load_pin,
					   tree->location(k),
					   nullptr, nullptr);
    Z.push_back(z);
  }
  else {
    // Steiner pt.
    RebufferOptionSeq Zl = rebufferBottomUp(tree, tree->left(k),
					    drvr_pin, buffer_cell);
    RebufferOptionSeq Zr = rebufferBottomUp(tree, tree->right(k),
					    drvr_pin, buffer_cell);
    RebufferOptionSeq Z2;
    // Combine the options from both branches.
    for (auto p : Zl) {
      for (auto q : Zr) {
	RebufferOption *junc = new RebufferOption(RebufferOption::Type::junction,
						  p->cap() + q->cap(),
						  min(p->required(), q->required()),
						  nullptr,
						  tree->location(k),
						  p, q);
	Z2.push_back(junc);
      }
    }

    // Prune the options.
    for (auto p : Z2) {
      int qi = 0;
      for (auto q : Z2) {
	if (p && q) {
	  Required Tp = p->bufferRequired(buffer_cell, drvr_pin, this);
	  Required Tq = q->bufferRequired(buffer_cell, drvr_pin, this);
	  float Lp = p->cap();
	  float Lq = q->cap();
	  if (Tp < Tq && Lp < Lq) {
	    // If q is strictly worse than p, remove solution q.
	    Z2[qi] = nullptr;
	  }
	  qi++;
	}
      }
      for (auto p : Z2) {
	if (p)
	  Z.push_back(p);
      }
    }
  }
  return addWireAndBuffer(Z, tree, k, drvr_pin, buffer_cell);
}

RebufferOptionSeq
Resizer::addWireAndBuffer(RebufferOptionSeq Z,
			  SteinerTree *tree,
			  SteinerPt k,
			  const Pin *drvr_pin,
			  LibertyCell *buffer_cell)
{
  RebufferOptionSeq Z1;
  Required best = -INF;
  RebufferOption *best_ref;
  for (auto p : Z) {
    float wire_delay = 0.0; // rc_delay(k, k.left)
    float wire_cap = 0.0; // wireload(k, k.left)
    RebufferOption *z = new RebufferOption(RebufferOption::Type::wire,
					   // account for wire delay
					   p->required() - wire_delay,
					   // account for wire load
					   p->cap() + wire_cap,
					   nullptr,
					   p->location(),
					   p, nullptr);
    Z1.push_back(z);
    // We could add options of different buffer drive strengths here
    // Which would have different delay Dbuf and input cap Lbuf
    // for simplicity we only consider one size of buffer.
    Required rt = z->bufferRequired(buffer_cell, drvr_pin, this);
    if (rt == best) {
      best = rt;
      best_ref = p;
    }
  }
  RebufferOption *z = new RebufferOption(RebufferOption::Type::buffer,
					 best,
					 bufferInputCapacitance(buffer_cell),
					 nullptr,
					 best_ref->location(),
					 best_ref, nullptr);
  Z1.push_back(z);
  return Z1;
}

void
Resizer::rebufferTopDown(RebufferOption *choice,
			 Net *net,
			 LibertyCell *buffer_cell)
{
  LefDefNetwork *network = lefDefNetwork();
  switch(choice->type()) {
  case RebufferOption::Type::buffer: {
    Instance *parent = network->topInstance();
    const char *net_name = makeUniqueNetName();
    const char *buffer_name = makeUniqueInstanceName();
    Net *net2 = network->makeNet(net_name, parent);
    Instance *buffer = network->makeInstance(buffer_cell, buffer_name, parent);
    LibertyPort *input, *output;
    buffer_cell->bufferPorts(input, output);
    connectPin(buffer, input, net);
    connectPin(buffer, output, net2);
    network->setLocation(buffer, choice->location());
    rebufferTopDown(choice->ref(), net2, buffer_cell);
    break;
  }
  case RebufferOption::Type::wire:
    rebufferTopDown(choice->ref(), net, buffer_cell);
    break;
  case RebufferOption::Type::junction:
    rebufferTopDown(choice->ref(), net, buffer_cell);
    rebufferTopDown(choice->ref2(), net, buffer_cell);
    break;
  case RebufferOption::Type::sink: {
    const Pin *load_pin = choice->loadPin();
    Instance *load_inst = network->instance(load_pin);
    Port *load_port = network->port(load_pin);
    connectPin(load_inst, load_port, net);
  }
  }
}

const char *
Resizer::makeUniqueNetName()
{
  // MIA
  return nullptr;
}

const char *
Resizer::makeUniqueInstanceName()
{
  // MIA
  return nullptr;
}

float
Resizer::bufferInputCapacitance(LibertyCell *buffer_cell)
{
  LibertyPort *input, *output;
  buffer_cell->bufferPorts(input, output);
  return portCapacitance(output);
}

float
Resizer::pinCapacitance(const Pin *pin)
{
  LibertyPort *port = network_->libertyPort(pin);
  return portCapacitance(port);
}

float
Resizer::portCapacitance(const LibertyPort *port)
{
  float cap1 = port->capacitance(TransRiseFall::rise(), min_max_);
  float cap2 = port->capacitance(TransRiseFall::fall(), min_max_);
  return max(cap1, cap2);
}

Required
Resizer::pinRequired(const Pin *pin)
{
  Vertex *vertex = graph_->pinLoadVertex(pin);
  return vertexRequired(vertex, min_max_);
}

Required
Resizer::vertexRequired(Vertex *vertex,
			const MinMax *min_max)
{
  findRequired(vertex);
  const MinMax *req_min_max = min_max->opposite();
  Required required = req_min_max->initValue();
  VertexPathIterator path_iter(vertex, this);
  while (path_iter.hasNext()) {
    const Path *path = path_iter.next();
    if (path->minMax(this) == min_max) {
      const Required path_required = path->required(this);
      if (fuzzyGreater(path_required, required, req_min_max))
	required = path_required;
    }
  }
  return required;
}

float
Resizer::bufferDelay(LibertyCell *buffer_cell,
		     const Pin *in_pin,
		     float load_cap)
{
  // Average rise/fall delays.
  ArcDelay arc_delays = 0.0;
  int arc_count = 0;
  LibertyCellTimingArcSetIterator set_iter(buffer_cell);
  TimingArcSet *arc_set = set_iter.next();
  TimingArcSetArcIterator arc_iter(arc_set);
  while (arc_iter.hasNext()) {
    TimingArc *arc = arc_iter.next();
    TransRiseFall *in_tr = arc->fromTrans()->asRiseFall();
    Vertex *in_vertex = graph_->pinLoadVertex(in_pin);
    float in_slew = graph_->slew(in_vertex, in_tr, dcalc_ap_->index());
    ArcDelay gate_delay;
    Slew drvr_slew;
    arc_delay_calc_->gateDelay(buffer_cell, arc, in_slew, load_cap,
			       nullptr, 0.0, pvt_, dcalc_ap_,
			       gate_delay,
			       drvr_slew);
    arc_delays += gate_delay;
    arc_count++;
  }
  if (arc_count > 0)
    return arc_delays / arc_count;
  else
    return 0.0;
}

};
