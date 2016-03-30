/* =======================================================================================
   This file is released as part of GraviT - scalable, platform independent ray
   tracing
   tacc.github.io/GraviT

   Copyright 2013-2015 Texas Advanced Computing Center, The University of Texas at Austin
   All rights reserved.

   Licensed under the BSD 3-Clause License, (the "License"); you may not use this file
   except in compliance with the License.
   A copy of the License is included with this software in the file LICENSE.
   If your copy does not contain the License, you may obtain a copy of the
   License at:

       http://opensource.org/licenses/BSD-3-Clause

   Unless required by applicable law or agreed to in writing, software distributed under
   the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
   KIND, either express or implied.
   See the License for the specific language governing permissions and limitations under
   limitations under the License.

   GraviT is funded in part by the US National Science Foundation under awards
   ACI-1339863,
   ACI-1339881 and ACI-1339840
   =======================================================================================
   */
#include "HeterogeneousMeshAdapter.h"
#include <atomic>
#include <cmath>
#include <future>
#include <mutex>
#include <thread>

#include <tbb/task_group.h>

using namespace gvt::render::actor;
using namespace gvt::render::adapter::heterogeneous::data;
//using namespace gvt::render::data::primitives;

HeterogeneousMeshAdapter::HeterogeneousMeshAdapter(std::map<int, gvt::render::data::primitives::Mesh *> &meshRef,
		std::map<int, glm::mat4 *> &instM,
        std::map<int, glm::mat4 *> &instMinv, std::map<int, glm::mat3 *> &instMinvN,
        std::vector<gvt::render::data::scene::Light *> &lights, std::vector<size_t> instances,
        bool unique) : Adapter(meshRef, instM, instMinv, instMinvN, lights, instances, unique) {

  _embree = new gvt::render::adapter::embree::data::EmbreeMeshAdapter(meshRef, instM, instMinv, instMinvN, lights, instances, unique);
  _optix = new gvt::render::adapter::optix::data::OptixMeshAdapter(meshRef, instM, instMinv, instMinvN, lights, instances, unique);
}

HeterogeneousMeshAdapter::~HeterogeneousMeshAdapter() {
  delete _embree;
  delete _optix;
}

void HeterogeneousMeshAdapter::trace(gvt::render::actor::RayVector &rayList, gvt::render::actor::RayVector &moved_rays,
                                     size_t begin, size_t end) {
#ifdef GVT_USE_DEBUG
  boost::timer::auto_cpu_timer t_functor("HeterogeneousMeshAdapter: trace time: %w\n");
#endif

  gvt::render::actor::RayVector mOptix;
  std::mutex _lock_rays;

  {
    const size_t size = rayList.size();
    //const size_t work = 4096;
    //size_t current = 0;

    //std::atomic<size_t> cput(0), gput(0);
    tbb::task_group g;


    size_t split = size * 0.25;

    g.run([&]() {
//      while (current < size) {
//        if (_lock_rays.try_lock()) {
//          size_t start = current;
//          current += work;
//          size_t end = current;
//          _lock_rays.unlock();
//
//          if (start >= size) continue;
//          if (end >= size) end = size;
//
//          gput++;
          _optix->trace(rayList, mOptix,   0, split);
 //       }
 //     }
    });

    g.run([&]() {
//      while (current < size) {
//        if (_lock_rays.try_lock()) {
//          size_t start = current;
//          current += work;
//          size_t end = current;
//          _lock_rays.unlock();
//
//          if (start >= size) continue;
//          if (end >= size) end = size;
//
//          cput++;
            _embree->trace(rayList, moved_rays,  split, end);

//        }
 //     }
    });

    g.wait();
    moved_rays.reserve(moved_rays.size() + mOptix.size());
    moved_rays.insert(moved_rays.end(), std::make_move_iterator(mOptix.begin()), std::make_move_iterator(mOptix.end()));

  }

  // rayList.clear();
}
