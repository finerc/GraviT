/* =======================================================================================
   This file is released as part of GraviT - scalable, platform independent ray tracing
   tacc.github.io/GraviT

   Copyright 2013-2015 Texas Advanced Computing Center, The University of Texas at Austin
   All rights reserved.

   Licensed under the BSD 3-Clause License, (the "License"); you may not use this file
   except in compliance with the License.
   A copy of the License is included with this software in the file LICENSE.
   If your copy does not contain the License, you may obtain a copy of the License at:

       http://opensource.org/licenses/BSD-3-Clause

   Unless required by applicable law or agreed to in writing, software distributed under
   the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
   KIND, either express or implied.
   See the License for the specific language governing permissions and limitations under
   limitations under the License.

   GraviT is funded in part by the US National Science Foundation under awards ACI-1339863,
   ACI-1339881 and ACI-1339840
   ======================================================================================= */
/*
 * File:   RayWeightedSpread.h
 * Author: jbarbosa
 *
 * Created on January 21, 2014, 4:16 PM
 */

#ifndef GVT_RENDER_SCHEDULE_HYBRID_RAY_WEIGHTED_SPREAD_H
#define GVT_RENDER_SCHEDULE_HYBRID_RAY_WEIGHTED_SPREAD_H

#include <gvt/render/schedule/hybrid/HybridScheduleBase.h>

namespace gvt {
namespace render {
namespace schedule {
namespace hybrid {
/// hybrid schedule that distributes requested data across available processes, sorted by number of pending rays
/** This schedule allocates requested domains that have high ray demand to availalbe processes,
 where a process is 'available' if none of its loaded data is currently requested by any ray.
 This takes the SpreadSchedule logic and sorts the homeless domains by number of pending rays
 before assigning to available processes.

This schedule has the following issues:
    - processes may remain idle if there are fewer requested domains than processes
    - a domain can be assigned to a process that does not have rays queued for it, incurring excess ray sends

    \sa LoadAnyOnceSchedule, GreedySchedule, SpreadSchedule
    */
struct RayWeightedSpreadSchedule : public HybridScheduleBase {

  RayWeightedSpreadSchedule(int *newMap, int &size, int *map_size_buf, int **map_recv_bufs, int *data_send_buf)
      : HybridScheduleBase(newMap, size, map_size_buf, map_recv_bufs, data_send_buf) {}

  virtual ~RayWeightedSpreadSchedule() {}

  virtual void operator()() {
    GVT_DEBUG(DBG_LOW, "in adaptive schedule");
    for (int i = 0; i < size; ++i) newMap[i] = -1;

    std::map<int, int> data2proc;
    std::map<int, int> data2size;
    std::map<int, int> size2data;
    for (int s = 0; s < size; ++s) {
      if (map_recv_bufs[s]) {
        // add currently loaded data
        data2proc[map_recv_bufs[s][0]] = s; // this will evict previous entries.
                                            // that's okay since we don't want
                                            // to dup data
        GVT_DEBUG(DBG_LOW, "    noting currently " << s << " -> " << map_recv_bufs[s][0]);

        // add ray counts
        for (int d = 1; d < map_size_buf[s]; d += 2) {
          data2size[map_recv_bufs[s][d]] += map_recv_bufs[s][d + 1];
          GVT_DEBUG(DBG_LOW, "        " << s << " has " << map_recv_bufs[s][d + 1] << " rays for data "
                                        << map_recv_bufs[s][d]);
        }
      }
    }

    // convert data2size into size2data,
    // use data id to pseudo-uniqueify, since only need ordering
    for (std::map<int, int>::iterator it = data2size.begin(); it != data2size.end(); ++it) {
      size2data[(it->second << 7) + it->first] = it->first;
    }

    // iterate over queued data, find which are already loaded somewhere
    // since size2data is sorted in increasing key order,
    // homeless data with most rays will end up at top of homeless list
    std::vector<int> homeless;
    for (std::map<int, int>::iterator d2sit = size2data.begin(); d2sit != size2data.end(); ++d2sit) {
      std::map<int, int>::iterator d2pit = data2proc.find(d2sit->second);
      if (d2pit != data2proc.end()) {
        newMap[d2pit->second] = d2pit->first;
        GVT_DEBUG(DBG_LOW, "    adding " << d2pit->second << " -> " << d2pit->first << " to map");
      } else {
        homeless.push_back(d2sit->second);
        GVT_DEBUG(DBG_LOW, "    noting " << d2sit->second << " is homeless");
      }
    }

    // iterate over newMap, fill as many procs as possible with homeless data
    // could be dupes in the homeless list, so keep track of what's added
    for (int i = 0; (i < size) & (!homeless.empty()); ++i) {
      if (newMap[i] < 0) {
        while (!homeless.empty() && data2proc.find(homeless.back()) != data2proc.end()) homeless.pop_back();
        if (!homeless.empty()) {
          newMap[i] = homeless.back();
          data2proc[newMap[i]] = i;
          homeless.pop_back();
        }
      }
    }

    GVT_DEBUG_CODE(DBG_LOW, std::cerr << "new map size is " << size << std::endl << std::flush;
                   for (int i = 0; i < size; ++i) std::cerr << "    " << i << " -> " << newMap[i] << std::endl << std::flush;);
  }
};
}
}
}
}

#endif /* GVT_RENDER_SCHEDULE_HYBRID_RAY_WEIGHTED_SPREAD_H */
