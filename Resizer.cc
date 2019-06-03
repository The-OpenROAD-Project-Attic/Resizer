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
#include "Debug.hh"
#include "PortDirection.hh"
#include "LefDefNetwork.hh"
#include "TimingRole.hh"
#include "Liberty.hh"
#include "TimingArc.hh"
#include "TimingModel.hh"
#include "Corner.hh"
#include "DcalcAnalysisPt.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"
#include "Parasitics.hh"
#include "Search.hh"
#include "Resizer.hh"
#include "flute.h"

// Outstanding issues
//  Instance levelization and resizing to target slew only support single output gates
//  flute wants to read files which prevents having a stand-alone executable
//  DefRead uses printf for errors.

namespace sta {

using std::abs;

static Pin *
singleOutputPin(const Instance *inst,
		Network *network);

Resizer::Resizer() :
  Sta(),
  flute_inited_(false)
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
Resizer::resize(Corner *corner)
{
  // Disable incremental timing.
  search_->arrivalsInvalid();

  ensureLevelized();
  InstanceSeq insts;
  instancesSortByLevel(insts);

  ensureTargetLoads(corner);
  // Resize by in reverse level order.
  for (int i = insts.size() - 1; i >= 0; i--) {
    Instance *inst = insts[i];
    resizeToTargetSlew1(inst, corner);
  }
}

void
Resizer::instancesSortByLevel(InstanceSeq &insts)
{
  LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    Instance *leaf = leaf_iter->next();
    insts.push_back(leaf);
  }
  sort(insts, InstanceOutputLevelLess(network_, graph_));
  delete leaf_iter;
}

void
Resizer::resizeToTargetSlew(Instance *inst,
			    Corner *corner)
{
  ensureTargetLoads(corner);
  resizeToTargetSlew1(inst, corner);
}

void
Resizer::resizeToTargetSlew1(Instance *inst,
			     Corner *corner)
{
  const MinMax *min_max = MinMax::max();
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
  LibertyCell *cell = network_->libertyCell(inst);
  if (cell) {
    Pin *output = singleOutputPin(inst, network_);
    // Only resize single output gates for now.
    if (output) {
      // Includes net parasitic capacitance.
      float load_cap = graph_delay_calc_->loadCap(output, dcalc_ap);
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
Resizer::ensureTargetLoads(Corner *corner)
{
  if (target_load_map_ == nullptr)
    findTargetLoads(corner);
}

void
Resizer::findTargetLoads(Corner *corner)
{
  const MinMax *min_max = MinMax::max();
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
  const Pvt *pvt = dcalc_ap->operatingConditions();

  // Find target slew across all buffers in the libraries.
  float tgt_slews[TransRiseFall::index_count];
  findBufferTargetSlews(pvt, min_max, tgt_slews);

  target_load_map_ = new CellTargetLoadMap;
  LibertyLibraryIterator *lib_iter = network_->libertyLibraryIterator();
  while (lib_iter->hasNext()) {
    LibertyLibrary *lib = lib_iter->next();
    findTargetLoads(lib, tgt_slews, pvt);
  }
  delete lib_iter;
}

void
Resizer::findTargetLoads(LibertyLibrary *library,
			 float *tgt_slews,
			 const Pvt *pvt)
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
						 tgt_slews[in_tr->index()],
						 pvt);
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
			Slew in_slew,
			const Pvt *pvt)
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
      model->gateDelay(cell, pvt, 0.0, load_cap, 0.0, false,
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
Resizer::findBufferTargetSlews(const Pvt *pvt,
			       const MinMax *min_max,
			       // Return values.
			       float *tgt_slews)
{
  tgt_slews[TransRiseFall::riseIndex()] = 0.0;
  tgt_slews[TransRiseFall::fallIndex()] = 0.0;
  int counts[TransRiseFall::index_count]{0};
  
  LibertyLibraryIterator *lib_iter = network_->libertyLibraryIterator();
  while (lib_iter->hasNext()) {
    LibertyLibrary *lib = lib_iter->next();
    findBufferTargetSlews(lib, pvt, min_max, tgt_slews, counts);
  }
  delete lib_iter;

  tgt_slews[TransRiseFall::riseIndex()] /= counts[TransRiseFall::riseIndex()];
  tgt_slews[TransRiseFall::fallIndex()] /= counts[TransRiseFall::fallIndex()];
}

void
Resizer::findBufferTargetSlews(LibertyLibrary *library,
			       const Pvt *pvt,
			       const MinMax *min_max,
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
	  float in_cap = input->capacitance(in_tr, min_max);
	  float load_cap = in_cap * 10.0; // "factor debatable"
	  ArcDelay arc_delay;
	  Slew arc_slew;
	  model->gateDelay(buffer, pvt, 0.0, load_cap, 0.0, false,
			   arc_delay, arc_slew);
	  model->gateDelay(buffer, pvt, arc_slew, load_cap, 0.0, false,
			   arc_delay, arc_slew);
	  slews[out_tr->index()] += arc_slew;
	  counts[out_tr->index()]++;
	}
      }
    }
  }
}

////////////////////////////////////////////////////////////////

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

private:
  Flute::Tree tree_;
  PinSeq pins_;
  // tree vertex index -> pin index
  Vector<int> pin_map_;
};

SteinerTree::~SteinerTree()
{
}

void
SteinerTree::setTree(Flute::Tree tree,
		     int pin_map[])
{
  tree_ = tree;

  int pin_count = pins_.size();
  // Invert the steiner vertex to pin index map.
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
  if (index2 < pins_.size() ){
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

////////////////////////////////////////////////////////////////

void
Resizer::ensureFluteInited()
{
  if (!flute_inited_) {
    // Flute reads look up tables from local files. gag me.
    Flute::readLUT("/Users/cherry/sta/flute/POWV9.dat",
		   "/Users/cherry/sta/flute/PORT9.dat");
    flute_inited_ = true;  
  }
}

SteinerTree *
Resizer::makeSteinerTree(const Net *net)
{
  LefDefNetwork *network = lefDefNetwork();
  debugPrint1(debug_, "steiner", 1, "Net %s\n", network->pathName(net));
  ensureFluteInited();

  SteinerTree *tree = new SteinerTree();
  PinSeq &pins = tree->pins();
  network->connectedPins(net, pins);
  int pin_count = pins.size();
  if (pin_count > 0) {
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
Resizer::makeNetParasitics(float wire_cap_per_length, // Farads/Meter
			   float wire_res_per_length) // Ohms/Meter
{
  NetIterator *net_iter = network_->netIterator(network_->topInstance());
  while (net_iter->hasNext()) {
    Net *net = net_iter->next();
    makeNetParasitics(net, wire_cap_per_length, wire_res_per_length);
  }
  delete net_iter;

  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
}

void
Resizer::makeNetParasitics(const Net *net,
			   float wire_cap_per_length,
			   float wire_res_per_length)
{
  Corner *corner = cmd_corner_;
  const MinMax *min_max = MinMax::max();
  LefDefNetwork *network = lefDefNetwork();
  const ParasiticAnalysisPt *ap = corner->findParasiticAnalysisPt(min_max);
  SteinerTree *tree = makeSteinerTree(net);
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
    ParasiticNode *n1 = pin1
      ? parasitics_->ensureParasiticNode(parasitic, pin1)
      : parasitics_->ensureParasiticNode(parasitic, net, steiner_pt1);
    ParasiticNode *n2 = pin2
      ? parasitics_->ensureParasiticNode(parasitic, pin2)
      : parasitics_->ensureParasiticNode(parasitic, net, steiner_pt2);
    float wire_length = network->dbuToMeters(wire_length_dbu);
    float wire_cap = wire_length * wire_cap_per_length;
    float wire_res = wire_length * wire_res_per_length;
    // Make pi model for the wire.
    parasitics_->incrCap(n1, wire_cap / 2.0, ap);
    parasitics_->makeResistor(nullptr, n1, n2, wire_res, ap);
    parasitics_->incrCap(n1, wire_cap / 2.0, ap);
  }
  delete tree;
}

};
