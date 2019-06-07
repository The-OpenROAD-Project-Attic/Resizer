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

#ifndef STA_RESIZER_H
#define STA_RESIZER_H

#include "Sta.hh"

namespace sta {

class LefDefNetwork;
class SteinerTree;
class RebufferOption;

typedef Map<LibertyCell*, float> CellTargetLoadMap;
typedef Vector<RebufferOption*> RebufferOptionSeq;
typedef int SteinerPt;

class Resizer : public Sta
{
public:
  Resizer();
  void initFlute(const char *resizer_path);
  LefDefNetwork *lefDefNetwork();

  // Resize all instances in the network.
  void resize(float wire_res_per_length, // ohms/meter
	      float wire_cap_per_length, // farads/meter
	      Corner *corner);
  // Resize a single instance to the target load.
  void resizeToTargetSlew(Instance *inst,
			  Corner *corner);
  SteinerTree *makeSteinerTree(const Net *net,
			       bool find_left_rights);
  void makeNetParasitics(float wire_res_per_length,
			 float wire_cap_per_length);

  void rebuffer(float cap_limit,
		LibertyCell *buffer_cell);
  float bufferDelay(LibertyCell *buffer_cell,
		    const Pin *in_pin,
		    float load_cap);

protected:
  virtual void makeNetwork();
  void initCorner(Corner *corner);
  void sortInstancesByLevel();
  void resizeToTargetSlew();
  void resizeToTargetSlew1(Instance *inst);
  void ensureTargetLoads();
  void findTargetLoads();
  void findTargetLoads(LibertyLibrary *library,
		       float *tgt_slews);
  float findTargetLoad(LibertyCell *cell,
		       TimingArc *arc,
		       Slew in_slew);
  void findBufferTargetSlews(// Return values.
			     float *tgt_slews);
  void findBufferTargetSlews(LibertyLibrary *library,
			     // Return values.
			     float *slews,
			     int *counts);
  void makeNetParasitics(const Net *net,
			 float wire_res_per_length,
			 float wire_cap_per_length);
  ParasiticNode *findParasiticNode(SteinerTree *tree,
				   Parasitic *parasitic,
				   const Net *net,
				   const Pin *pin,
				   int steiner_pt);
  bool readFluteInits(string dir);
  void rebuffer(const Pin *drvr_pin,
		LibertyCell *buffer_cell);
  RebufferOptionSeq rebufferBottomUp(SteinerTree *tree,
				     SteinerPt k,
				     const Pin *drvr_pin,
				     LibertyCell *buffer_cell);
  void rebufferTopDown(RebufferOption *choice,
		       Net *net,
		       LibertyCell *buffer_cell);
  RebufferOptionSeq
  addWireAndBuffer(RebufferOptionSeq Z,
		   SteinerTree *tree,
		   SteinerPt k,
		   const Pin *drvr_pin,
		   LibertyCell *buffer_cell);
  void connectedPins(const Net *net,
		     PinSeq &pins);
  float portCapacitance(const LibertyPort *port);
  float pinCapacitance(const Pin *pin);
  float bufferInputCapacitance(LibertyCell *buffer_cell);
  Required pinRequired(const Pin *pin);
  Required vertexRequired(Vertex *vertex,
			  const MinMax *min_max);
  string makeUniqueNetName();
  string makeUniqueBufferName();

private:
  Corner *corner_;
  const MinMax *min_max_;
  const DcalcAnalysisPt *dcalc_ap_;
  const Pvt *pvt_;
  CellTargetLoadMap *target_load_map_;
  InstanceSeq level_insts_;
  int unique_net_index_;
  int unique_buffer_index_;
};

} // namespace
#endif
