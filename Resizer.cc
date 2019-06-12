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
#include "Report.hh"
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
#include "SearchPred.hh"
#include "Bfs.hh"
#include "Search.hh"
#include "LefDefNetwork.hh"
#include "SteinerTree.hh"
#include "Resizer.hh"

// Outstanding issues
//  Instance levelization and resizing to target slew only support single output gates
//  skinflute wants to read files which prevents having a stand-alone executable
//  multi-corner support?
//  tcl cmds to set liberty pin cap and limit for testing
//  unplaced nets should use wireload model parasitic instead of steiner parasitics
//  check lef/liberty cells match
//  check one lef, one def
//  test rebuffering on input ports

namespace sta {

using std::abs;
using std::min;
using std::string;

static Pin *
singleOutputPin(const Instance *inst,
		Network *network);

Resizer::Resizer() :
  Sta(),
  level_insts_valid_(false),
  tgt_slews_valid_(false),
  unique_net_index_(1),
  unique_buffer_index_(1)
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
Resizer::init(float wire_res_per_length,
	      float wire_cap_per_length,
	      Corner *corner)
{
  initCorner(corner);
  wire_res_per_length_ = wire_res_per_length;
  wire_cap_per_length_ = wire_cap_per_length;

  // Disable incremental timing.
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();

  ensureLevelized();
  ensureLevelInsts();
  resize_count_ = 0;
  rebuffer_count_ = 0;
}

void
Resizer::resize(float wire_res_per_length,
		float wire_cap_per_length,
		Corner *corner)
{
  init(wire_res_per_length, wire_cap_per_length, corner);

  // Find a target slew for the libraries and then
  // a target load for each cell that gives the target slew.
  ensureTargetLoads();

  if (wire_cap_per_length > 0.0)
    makeNetParasitics();

  resizeToTargetSlew();
  report_->print("Resized %d instances.\n", resize_count_);
}

void
Resizer::initCorner(Corner *corner)
{
  corner_ = corner;
  min_max_ = MinMax::max();
  dcalc_ap_ = corner->findDcalcAnalysisPt(min_max_);
  pvt_ = dcalc_ap_->operatingConditions();
  parasitics_ap_ = corner->findParasiticAnalysisPt(min_max_);
}

void
Resizer::ensureLevelInsts()
{
  if (!level_insts_valid_) {
    level_insts_.clear();
    LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
    while (leaf_iter->hasNext()) {
      Instance *leaf = leaf_iter->next();
      level_insts_.push_back(leaf);
    }
    sort(level_insts_, InstanceOutputLevelLess(network_, graph_));
    delete leaf_iter;
    level_insts_valid_ = true;
  }
}

void
Resizer::resizeToTargetSlew(Instance *inst,
			    Corner *corner)
{
  init(0.0, 0.0, corner);
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
	  resize_count_++;
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
  ensureBufferTargetSlews();

  target_load_map_ = new CellTargetLoadMap;
  LibertyLibraryIterator *lib_iter = network_->libertyLibraryIterator();
  while (lib_iter->hasNext()) {
    LibertyLibrary *lib = lib_iter->next();
    findTargetLoads(lib);
  }
  delete lib_iter;
}

void
Resizer::findTargetLoads(LibertyLibrary *library)
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
						 tgt_slews_[in_tr->index()]);
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

// Find target slew across all buffers in the libraries.
void
Resizer::ensureBufferTargetSlews()
{
  if (!tgt_slews_valid_) {
    findBufferTargetSlews();
    tgt_slews_valid_ = true;
  }
}

void
Resizer::findBufferTargetSlews()
{
  tgt_slews_[TransRiseFall::riseIndex()] = 0.0;
  tgt_slews_[TransRiseFall::fallIndex()] = 0.0;
  int counts[TransRiseFall::index_count]{0};
  
  LibertyLibraryIterator *lib_iter = network_->libertyLibraryIterator();
  while (lib_iter->hasNext()) {
    LibertyLibrary *lib = lib_iter->next();
    findBufferTargetSlews(lib, counts);
  }
  delete lib_iter;

  tgt_slews_[TransRiseFall::riseIndex()] /= counts[TransRiseFall::riseIndex()];
  tgt_slews_[TransRiseFall::fallIndex()] /= counts[TransRiseFall::fallIndex()];
}

void
Resizer::findBufferTargetSlews(LibertyLibrary *library,
			       // Return values.
			       int counts[])
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
	  tgt_slews_[out_tr->index()] += arc_slew;
	  counts[out_tr->index()]++;
	}
      }
    }
  }
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

////////////////////////////////////////////////////////////////

void
Resizer::makeNetParasitics(float wire_res_per_length,
			   float wire_cap_per_length,
			   Corner *corner)
{
  init(wire_res_per_length, wire_cap_per_length, corner);
  makeNetParasitics();
}

void
Resizer::makeNetParasitics()
{
  NetIterator *net_iter = network_->netIterator(network_->topInstance());
  while (net_iter->hasNext()) {
    Net *net = net_iter->next();
    makeNetParasitics(net);
  }
  delete net_iter;
}

void
Resizer::makeNetParasitics(const Net *net)
{
  LefDefNetwork *network = lefDefNetwork();
  SteinerTree *tree = makeSteinerTree(net, false, network);
  if (tree) {
    tree->findSteinerPtAliases();
    debugPrint1(debug_, "resizer_parasitics", 1, "net %s\n",
		sdc_network_->pathName(net));
    Parasitic *parasitic = parasitics_->makeParasiticNetwork(net, false,
							     parasitics_ap_);
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
      if (n1 != n2) {
	if (wire_length_dbu == 0)
	  // Use a small resistor to keep the connectivity intact.
	  parasitics_->makeResistor(nullptr, n1, n2, 1.0e-3, parasitics_ap_);
	else {
	  float wire_length = network->dbuToMeters(wire_length_dbu);
	  float wire_cap = wire_length * wire_cap_per_length_;
	  float wire_res = wire_length * wire_res_per_length_;
	  // Make pi model for the wire.
	  debugPrint5(debug_, "resizer_parasitics", 2,
		      " pi %s c2=%s rpi=%s c1=%s %s\n",
		      parasitics_->name(n1),
		      units_->capacitanceUnit()->asString(wire_cap / 2.0),
		      units_->resistanceUnit()->asString(wire_res),
		      units_->capacitanceUnit()->asString(wire_cap / 2.0),
		      parasitics_->name(n2));
	  parasitics_->incrCap(n1, wire_cap / 2.0, parasitics_ap_);
	  parasitics_->makeResistor(nullptr, n1, n2, wire_res, parasitics_ap_);
	  parasitics_->incrCap(n2, wire_cap / 2.0, parasitics_ap_);
	}
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
		 Pin *load_pin,
		 DefPt location,
		 RebufferOption *ref,
		 RebufferOption *ref2);
  ~RebufferOption();
  Type type() const { return type_; }
  float cap() const { return cap_; }
  Required required() const { return required_; }
  Required bufferRequired(LibertyCell *buffer_cell,
			  Resizer *resizer) const;
  DefPt location() const { return location_; }
  Pin *loadPin() const { return load_pin_; }
  RebufferOption *ref() const { return ref_; }
  RebufferOption *ref2() const { return ref2_; }

private:
  Type type_;
  float cap_;
  Required required_;
  Pin *load_pin_;
  DefPt location_;
  RebufferOption *ref_;
  RebufferOption *ref2_;
};

RebufferOption::RebufferOption(Type type,
			       float cap,
			       Required required,
			       Pin *load_pin,
			       DefPt location,
			       RebufferOption *ref,
			       RebufferOption *ref2) :
  type_(type),
  cap_(cap),
  required_(required),
  load_pin_(load_pin),
  location_(location),
  ref_(ref),
  ref2_(ref2)
{
}

RebufferOption::~RebufferOption()
{
}

Required
RebufferOption::bufferRequired(LibertyCell *buffer_cell,
			       Resizer *resizer) const
{
  return required_ - resizer->bufferDelay(buffer_cell, cap_);
}

////////////////////////////////////////////////////////////////

void
Resizer::rebuffer(bool repair_max_cap,
		  bool repair_max_slew,
		  LibertyCell *buffer_cell,
		  float wire_res_per_length,
		  float wire_cap_per_length,
		  Corner *corner)
{
  init(wire_res_per_length, wire_cap_per_length, corner);
  ensureBufferTargetSlews();
  rebuffer(repair_max_cap, repair_max_slew, buffer_cell);
  report_->print("Inserted %d buffers.\n", rebuffer_count_);
}

// EvalPred unless (no dynamic loop breaking)
//  latch D->Q edge
class SearchThru1 : public EvalPred
{
public:
  SearchThru1(const StaState *sta);
  virtual bool searchThru(Edge *edge);

private:
  DISALLOW_COPY_AND_ASSIGN(SearchThru1);
};

SearchThru1::SearchThru1(const StaState *sta) :
  EvalPred(sta)
{
}

bool
SearchThru1::searchThru(Edge *edge)
{
  const Graph *graph = sta_->graph();
  const Sdc *sdc = sta_->sdc();
  Search *search = sta_->search();
  return EvalPred::searchThru(edge)
    && edge->role() != TimingRole::latchDtoQ();
}

void
Resizer::rebuffer(bool repair_max_cap,
		  bool repair_max_slew,
		  LibertyCell *buffer_cell)
{
  SearchThru1 search_adj(this);
  BfsBkwdIterator bfs(BfsIndex::other, &search_adj, this);
  while (bfs.hasNext()) {
    Vertex *vertex = bfs.next();
    if (vertex->isDriver(network_)) {
      Pin *drvr_pin = vertex->pin();
      if ((repair_max_cap
	   && hasMaxCapViolation(drvr_pin))
	  || (repair_max_slew
	      && hasMaxSlewViolation(drvr_pin)))
	rebuffer(drvr_pin, buffer_cell);
    }
  }
}

bool
Resizer::hasMaxCapViolation(const Pin *drvr_pin)
{
  float load_cap = graph_delay_calc_->loadCap(drvr_pin, dcalc_ap_);
  LibertyPort *port = network_->libertyPort(drvr_pin);
  float cap_limit;
  bool exists;
  port->capacitanceLimit(MinMax::max(), cap_limit, exists);
  return exists && load_cap > cap_limit;
}

bool
Resizer::hasMaxSlewViolation(const Pin *drvr_pin)
{
  LibertyPort *port = network_->libertyPort(drvr_pin);
  float slew_limit;
  bool exists;
  port->slewLimit(MinMax::max(), slew_limit, exists);
  Vertex *vertex = graph_->pinDrvrVertex(drvr_pin);
  if (exists) {
    TransRiseFallIterator tr_iter;
    while (tr_iter.hasNext()) {
      TransRiseFall *tr = tr_iter.next();
      Slew slew = graph_->slew(vertex, tr, dcalc_ap_->index());
	if (slew > slew_limit)
	  return true;
    }
  }
  return false;
}

void
Resizer::rebuffer(Instance *inst,
		  LibertyCell *buffer_cell,
		  float wire_res_per_length,
		  float wire_cap_per_length,
		  Corner *corner)
{
  init(wire_res_per_length, wire_cap_per_length, corner);
  ensureBufferTargetSlews();
  LibertyCell *cell = network_->libertyCell(inst);
  if (cell) {
    Pin *output = singleOutputPin(inst, network_);
    if (output)
      rebuffer(output, buffer_cell);
  }
  report_->print("Inserted %d buffers.\n", rebuffer_count_);
}

void
Resizer::rebuffer(Instance *inst,
		  LibertyCell *buffer_cell)
{
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

void
Resizer::rebuffer(const Pin *drvr_pin,
		  LibertyCell *buffer_cell)
{
  Net *net = network_->net(drvr_pin);
  LibertyPort *drvr_port = network_->libertyPort(drvr_pin);
  if (drvr_port == nullptr) {
    // Top level port. Should use sdc external driver here.
    LibertyPort *input;
    buffer_cell->bufferPorts(input, drvr_port);
  }
  LefDefNetwork *network = lefDefNetwork();
  SteinerTree *tree = makeSteinerTree(net, true, network);
  SteinerPt drvr_pt = tree->drvrPt(network_);
  Required drvr_req = pinRequired(drvr_pin);
  // Make sure the driver is constrained.
  if (!fuzzyInf(drvr_req)) {
    debugPrint1(debug_, "rebuffer", 2, "driver %s\n",
		sdc_network_->pathName(drvr_pin));
    RebufferOptionSeq Z = rebufferBottomUp(tree, tree->left(drvr_pt),
					   drvr_pt,
					   1, buffer_cell);
    Required Tbest = -INF;
    RebufferOption *best = nullptr;
    for (auto p : Z) {
      Required Tb = p->required() - gateDelay(drvr_port, p->cap());
      if (fuzzyGreater(Tb, Tbest)) {
	Tbest = Tb;
	best = p;
      }
    }
    if (best)
      rebufferTopDown(best, net, 1, buffer_cell);
  }
}

// The routing tree is represented a binary tree with the sinks being the leaves
// of the tree, the junctions being the Steiner nodes and the root being the
// source of the net.
RebufferOptionSeq
Resizer::rebufferBottomUp(SteinerTree *tree,
			  SteinerPt k,
			  SteinerPt prev,
			  int level,
			  LibertyCell *buffer_cell)
{
  if (k != SteinerTree::null_pt) {
    Pin *pin = tree->pin(k);
    if (pin && network_->isLoad(pin)) {
      // Load capacitance and required time.
      RebufferOption *z = new RebufferOption(RebufferOption::Type::sink,
					     pinCapacitance(pin),
					     pinRequired(pin),
					     pin,
					     tree->location(k),
					     nullptr, nullptr);
      // %*s format indents level spaces.
      debugPrint5(debug_, "rebuffer", 3, "%*sload %s cap %s req %s\n",
		  level, "",
		  sdc_network_->pathName(pin),
		  units_->capacitanceUnit()->asString(z->cap()),
		  delayAsString(z->required(), this));
      RebufferOptionSeq Z;
      Z.push_back(z);
      return addWireAndBuffer(Z, tree, k, prev, level, buffer_cell);
    }
    else if (pin == nullptr) {
      // Steiner pt.
      RebufferOptionSeq Zl = rebufferBottomUp(tree, tree->left(k), k,
					      level + 1, buffer_cell);
      RebufferOptionSeq Zr = rebufferBottomUp(tree, tree->right(k), k,
					      level + 1, buffer_cell);
      RebufferOptionSeq Z2;
      // Combine the options from both branches.
      for (auto p : Zl) {
	for (auto q : Zr) {
	  RebufferOption *junc = new RebufferOption(RebufferOption::Type::junction,
						    p->cap() + q->cap(),
						    min(p->required(),
							q->required()),
						    nullptr,
						    tree->location(k),
						    p, q);
	  Z2.push_back(junc);
	}
      }
      // Prune the options. This is fanout^2.
      for (auto p : Z2) {
	if (p) {
	  Required Tp = p->bufferRequired(buffer_cell, this);
	  float Lp = p->cap();
	  int qi = 0;
	  for (auto q : Z2) {
	    if (q) {
	      Required Tq = q->bufferRequired(buffer_cell, this);
	      float Lq = q->cap();
	      if (fuzzyLess(Tq, Tp) && fuzzyGreater(Lq, Lp)) {
		// If q is strictly worse than p, remove solution q.
		Z2[qi] = nullptr;
	      }
	      qi++;
	    }
	  }
	}
      }
      // Save the survivors.
      RebufferOptionSeq Z;
      for (auto p : Z2) {
	if (p) {
	  debugPrint5(debug_, "rebuffer", 3, "%*sjunction %s cap %s req %s\n",
		      level, "",
		      tree->name(k, sdc_network_),
		      units_->capacitanceUnit()->asString(p->cap()),
		      delayAsString(p->required(), this));
	  Z.push_back(p);
	}
      }
      return addWireAndBuffer(Z, tree, k, prev, level, buffer_cell);
    }
  }
  return RebufferOptionSeq();
}

RebufferOptionSeq
Resizer::addWireAndBuffer(RebufferOptionSeq Z,
			  SteinerTree *tree,
			  SteinerPt k,
			  SteinerPt prev,
			  int level,
			  LibertyCell *buffer_cell)
{
  LefDefNetwork *network = lefDefNetwork();
  RebufferOptionSeq Z1;
  Required best = -INF;
  RebufferOption *best_ref = nullptr;
  DefPt k_loc = tree->location(k);
  DefPt prev_loc = tree->location(prev);
  DefDbu wire_length_dbu = abs(k_loc.x() - prev_loc.x())
    + abs(k_loc.y() - prev_loc.y());
  float wire_length = network->dbuToMeters(wire_length_dbu);
  float wire_cap = wire_length * wire_cap_per_length_;
  float wire_res = wire_length * wire_res_per_length_;
  float wire_delay = wire_res * wire_cap;
  for (auto p : Z) {
    RebufferOption *z = new RebufferOption(RebufferOption::Type::wire,
					   // account for wire load
					   p->cap() + wire_cap,
					   // account for wire delay
					   p->required() - wire_delay,
					   nullptr,
					   prev_loc,
					   p, nullptr);
    debugPrint7(debug_, "rebuffer", 3, "%*swire %s -> %s wl %d cap %s req %s\n",
		level, "",
		tree->name(prev, sdc_network_),
		tree->name(k, sdc_network_),
		wire_length_dbu,
		units_->capacitanceUnit()->asString(z->cap()),
		delayAsString(z->required(), this));
    Z1.push_back(z);
    // We could add options of different buffer drive strengths here
    // Which would have different delay Dbuf and input cap Lbuf
    // for simplicity we only consider one size of buffer.
    Required rt = z->bufferRequired(buffer_cell, this);
    if (fuzzyGreater(rt, best)) {
      best = rt;
      best_ref = p;
    }
  }
  if (best_ref) {
    RebufferOption *z = new RebufferOption(RebufferOption::Type::buffer,
					   bufferInputCapacitance(buffer_cell),
					   best,
					   nullptr,
					   // Locate buffer at opposite end of wire.
					   prev_loc,
					   best_ref, nullptr);
    debugPrint7(debug_, "rebuffer", 3, "%*sbuffer %s cap %s req %s -> cap %s req %s\n",
		level, "",
		tree->name(prev, sdc_network_),
		units_->capacitanceUnit()->asString(best_ref->cap()),
		delayAsString(best_ref->required(), this),
		units_->capacitanceUnit()->asString(z->cap()),
		delayAsString(z->required(), this));
    Z1.push_back(z);
  }
  return Z1;
}

void
Resizer::rebufferTopDown(RebufferOption *choice,
			 Net *net,
			 int level,
			 LibertyCell *buffer_cell)
{
  LefDefNetwork *network = lefDefNetwork();
  switch(choice->type()) {
  case RebufferOption::Type::buffer: {
    Instance *parent = network->topInstance();
    string net2_name = makeUniqueNetName();
    string buffer_name = makeUniqueBufferName();
    Net *net2 = network->makeNet(net2_name.c_str(), parent);
    Instance *buffer = network->makeInstance(buffer_cell,
					     buffer_name.c_str(),
					     parent);
    level_insts_valid_ = false;
    LibertyPort *input, *output;
    buffer_cell->bufferPorts(input, output);
    debugPrint5(debug_, "rebuffer", 3, "%*sinsert %s -> %s -> %s\n",
		level, "",
		sdc_network_->pathName(net),
		buffer_name.c_str(),
		net2_name.c_str());
    connectPin(buffer, input, net);
    connectPin(buffer, output, net2);
    network->setLocation(buffer, choice->location());
    rebufferTopDown(choice->ref(), net2, level + 1, buffer_cell);
    makeNetParasitics(net);
    makeNetParasitics(net2);
    rebuffer_count_++;
    break;
  }
  case RebufferOption::Type::wire:
    debugPrint2(debug_, "rebuffer", 3, "%*swire\n", level, "");
    rebufferTopDown(choice->ref(), net, level + 1, buffer_cell);
    break;
  case RebufferOption::Type::junction:
    debugPrint2(debug_, "rebuffer", 3, "%*sjunction\n", level, "");
    rebufferTopDown(choice->ref(), net, level + 1, buffer_cell);
    rebufferTopDown(choice->ref2(), net, level + 1, buffer_cell);
    break;
  case RebufferOption::Type::sink: {
    Pin *load_pin = choice->loadPin();
    Net *load_net = network_->net(load_pin);
    if (load_net != net) {
      Instance *load_inst = network->instance(load_pin);
      Port *load_port = network->port(load_pin);
      debugPrint4(debug_, "rebuffer", 3, "%*sconnect load %s to %s\n",
		  level, "",
		  sdc_network_->pathName(load_pin),
		  sdc_network_->pathName(net));
      disconnectPin(load_pin);
      connectPin(load_inst, load_port, net);
    }
  }
  }
}

string
Resizer::makeUniqueNetName()
{
  string node_name;
  Instance *top_inst = network_->topInstance();
  do 
    stringPrint(node_name, "net%d", unique_net_index_++);
  while (network_->findNet(top_inst, node_name.c_str()));
  return node_name;
}

string
Resizer::makeUniqueBufferName()
{
  string buffer_name;
  do 
    stringPrint(buffer_name, "buffer%d", unique_buffer_index_++);
  while (network_->findInstance(buffer_name.c_str()));
  return buffer_name;
}

float
Resizer::bufferInputCapacitance(LibertyCell *buffer_cell)
{
  LibertyPort *input, *output;
  buffer_cell->bufferPorts(input, output);
  return portCapacitance(input);
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
		     float load_cap)
{
  LibertyPort *input, *output;
  buffer_cell->bufferPorts(input, output);
  return gateDelay(output, load_cap);
}

float
Resizer::gateDelay(LibertyPort *out_port,
		   float load_cap)
{
  LibertyCell *cell = out_port->libertyCell();
  // Max rise/fall delays.
  ArcDelay max_delay = -INF;
  LibertyCellTimingArcSetIterator set_iter(cell);
  while (set_iter.hasNext()) {
    TimingArcSet *arc_set = set_iter.next();
    if (arc_set->to() == out_port) {
      TimingArcSetArcIterator arc_iter(arc_set);
      while (arc_iter.hasNext()) {
	TimingArc *arc = arc_iter.next();
	TransRiseFall *in_tr = arc->fromTrans()->asRiseFall();
	float in_slew = tgt_slews_[in_tr->index()];
	ArcDelay gate_delay;
	Slew drvr_slew;
	arc_delay_calc_->gateDelay(cell, arc, in_slew, load_cap,
				   nullptr, 0.0, pvt_, dcalc_ap_,
				   gate_delay,
				   drvr_slew);
	max_delay = max(max_delay, gate_delay);
      }
    }
  }
  return max_delay;
}

};
