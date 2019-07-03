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

#ifndef RESIZER_RESIZER_H
#define RESIZER_RESIZER_H

#include "Sta.hh"
#include "SteinerTree.hh"

namespace sta {

class LefDefNetwork;
class RebufferOption;

typedef Map<LibertyCell*, float> CellTargetLoadMap;
typedef Vector<RebufferOption*> RebufferOptionSeq;

class Resizer : public Sta
{
public:
  Resizer();
  LefDefNetwork *lefDefNetwork();
  const LefDefNetwork *lefDefNetwork() const;
  void initFlute(const char *resizer_path);

  // Set the resistance and capacitance used for parasitics.
  // Make net wire parasitics based on DEF locations.
  void setWireRC(float wire_res, // ohms/meter
		 float wire_cap, // farads/meter
		 Corner *corner);
  // Resize all instances in the network
  // and insert buffers to fix max cap/slew violations.
  void resize(bool resize,
	      bool repair_max_cap,
	      bool repair_max_slew,
	      LibertyCell *buffer_cell,
	      LibertyLibrarySeq *resize_libs,
	      LibertyCellSeq *dont_use);

  // The functions below are for testing phases of the resizer.
  // Resize a single instance to the target load.
  void resizeToTargetSlew(Instance *inst,
			  LibertyLibrarySeq *resize_libs,
			  Corner *corner);
  // Rebuffer net.
  // Assumes buffer_cell->isBuffer() is true.
  void rebuffer(Net *net,
		LibertyCell *buffer_cell,
		LibertyLibrarySeq *resize_libs);
  Slew targetSlew(const TransRiseFall *tr);
  float targetLoadCap(LibertyCell *cell);
  // Area of the design in meter^2.
  double designArea();

protected:
  virtual void makeNetwork();
  virtual void makeCmdNetwork();
  void init();
  void ensureCorner();
  void initCorner(Corner *corner);
  void ensureClkNets();
  void findClkNets();
  bool isClock(Net *net);
  void ensureLevelDrvrVerticies();
  void resizeToTargetSlew(LibertyLibrarySeq *resize_libs);
  void makeEquivCells(LibertyLibrarySeq *resize_libs);
  void resizeToTargetSlew1(Instance *inst);
  void findTargetLoads(LibertyLibrarySeq *resize_libs);
  void findTargetLoads(LibertyLibrary *library,
		       Slew slews[]);
  void findTargetLoad(LibertyCell *cell,
		      Slew slews[]);
  float findTargetLoad(LibertyCell *cell,
		       TimingArc *arc,
		       Slew in_slew,
		       Slew out_slew);
  void findBufferTargetSlews(LibertyLibrarySeq *resize_libs);
  void findBufferTargetSlews(LibertyLibrary *library,
			     // Return values.
			     Slew slews[],
			     int counts[]);
  void makeNetParasitics();
  void makeNetParasitics(const Net *net);
  ParasiticNode *findParasiticNode(SteinerTree *tree,
				   Parasitic *parasitic,
				   const Net *net,
				   const Pin *pin,
				   int steiner_pt);

  // Assumes buffer_cell->isBuffer() is true.
  void rebuffer(bool repair_max_cap,
		bool repair_max_slew,
		LibertyCell *buffer_cell);
  void rebuffer(const Pin *drvr_pin,
		LibertyCell *buffer_cell);
  bool hasMaxCapViolation(const Pin *drvr_pin);
  bool hasMaxSlewViolation(const Pin *drvr_pin);
  void slewLimit(const Pin *pin,
		 const MinMax *min_max,
		 // Return values.
		 float &limit,
		 bool &exists) const;
			
  RebufferOptionSeq rebufferBottomUp(SteinerTree *tree,
				     SteinerPt k,
				     SteinerPt prev,
				     int level,
				     LibertyCell *buffer_cell);
  void rebufferTopDown(RebufferOption *choice,
		       Net *net,
		       int level,
		       LibertyCell *buffer_cell);
  RebufferOptionSeq
  addWireAndBuffer(RebufferOptionSeq Z,
		   SteinerTree *tree,
		   SteinerPt k,
		   SteinerPt prev,
		   int level,
		   LibertyCell *buffer_cell);
  float portCapacitance(const LibertyPort *port);
  float pinCapacitance(const Pin *pin);
  float bufferInputCapacitance(LibertyCell *buffer_cell);
  Required pinRequired(const Pin *pin);
  Required vertexRequired(Vertex *vertex,
			  const MinMax *min_max);
  float gateDelay(LibertyPort *out_port,
		  float load_cap);
  float bufferDelay(LibertyCell *buffer_cell,
		    float load_cap);
  string makeUniqueNetName();
  string makeUniqueBufferName();
  bool dontUse(LibertyCell *cell);
  void findDesignArea();

  friend class RebufferOption;
  using Sta::makeEquivCells;

  float wire_res_;
  float wire_cap_;
  Corner *corner_;
  LibertyCellSet dont_use_;

  const MinMax *min_max_;
  const DcalcAnalysisPt *dcalc_ap_;
  const Pvt *pvt_;
  const ParasiticAnalysisPt *parasitics_ap_;
  NetSet clk_nets_;
  bool clk_nets__valid_;
  CellTargetLoadMap *target_load_map_;
  VertexSeq level_drvr_verticies_;
  bool level_drvr_verticies_valid_;
  Slew tgt_slews_[TransRiseFall::index_count];
  int unique_net_index_;
  int unique_buffer_index_;
  int resize_count_;
  int inserted_buffer_count_;
  int rebuffer_net_count_;
  double design_area_;
};

} // namespace
#endif
