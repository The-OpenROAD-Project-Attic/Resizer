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

#ifndef STA_RESIZER_H
#define STA_RESIZER_H

#include "Sta.hh"

namespace sta {

typedef Map<LibertyCell*, float> CellTargetLoadMap;

class Resizer : public Sta
{
public:
  Resizer();
  // Resize all instances in the network.
  void resize(Corner *corner);
  // Resize a single instance to the target load.
  void resizeToTargetSlew(Instance *inst,
			  Corner *corner);

protected:
  virtual void makeNetwork();
  void resizeToTargetSlew1(Instance *inst,
			   Corner *corner);
  void instancesSortByLevel(InstanceSeq &insts);
  void ensureTargetLoads(Corner *corner);
  void findTargetLoads(Corner *corner);
  void findTargetLoads(LibertyLibrary *library,
		       float *tgt_slews,
		       const Pvt *pvt);
  float findTargetLoad(LibertyCell *cell,
		       TimingArc *arc,
		       Slew in_slew,
		       const Pvt *pvt);
  void findBufferTargetSlews(const Pvt *pvt,
			     const MinMax *min_max,
			     // Return values.
			     float *tgt_slews);
  void findBufferTargetSlews(LibertyLibrary *library,
			     const Pvt *pvt,
			     const MinMax *min_max,
			     // Return values.
			     float *slews,
			     int *counts);

private:
  CellTargetLoadMap *target_load_map_;
};

} // namespace
#endif