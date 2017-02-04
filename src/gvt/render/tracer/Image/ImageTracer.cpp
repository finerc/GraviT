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

#include <algorithm>

#include "ImageTracer.h"
#include <gvt/core/comm/communicator.h>

namespace gvt {
namespace render {
ImageTracer::ImageTracer() : gvt::render::RayTracer() {
  queue_mutex = new std::mutex[meshRef.size()];
  for (auto &m : meshRef) {
    queue[m.first] = gvt::render::actor::RayVector();
  }
}
ImageTracer::~ImageTracer() {
  if (queue_mutex != nullptr) delete[] queue_mutex;
  queue.clear();
}

void ImageTracer::resetBVH() {
  RayTracer::resetBVH();
  if (queue_mutex != nullptr) delete[] queue_mutex;
  for (auto &m : meshRef) {
    queue[m.first] = gvt::render::actor::RayVector();
  }
}

void ImageTracer::operator()() {

  img->reset();

  cam->AllocateCameraRays();
  cam->generateRays();
  processRaysAndDrop(cam->rays);
  gvt::render::actor::RayVector returned_rays;
  do {
    int target = -1;
    int amount = 0;
    for (auto &q : queue) {
      if (q.second.size() > amount) {
        amount = q.second.size();
        target = q.first;
      }
    }
    if (target != -1) {
      returned_rays.reserve(queue[target].size() * 10);
      calladapter(target, queue[target], returned_rays);
      queue[target].clear();
      processRays(returned_rays, target);
    }
  } while (hasWork());
  img->composite();
}

void ImageTracer::processRaysAndDrop(gvt::render::actor::RayVector &rays) {

  gvt::comm::communicator &comm = gvt::comm::communicator::instance();
  const unsigned ray_chunk = rays.size() / comm.lastid();
  const unsigned ray_start = ray_chunk * comm.id();
  const unsigned ray_end = ray_chunk * (comm.id() + 1);

  const int chunksize =
      MAX(4096, ray_chunk / (gvt::core::CoreContext::instance()->getRootNode()["threads"].value().toInteger() * 4));
  gvt::render::data::accel::BVH &acc = *bvh.get();
  static tbb::simple_partitioner ap;
  tbb::parallel_for(tbb::blocked_range<gvt::render::actor::RayVector::iterator>(rays.begin() + ray_start,
                                                                                rays.begin() + ray_end, chunksize),
                    [&](tbb::blocked_range<gvt::render::actor::RayVector::iterator> raysit) {

                      gvt::core::Vector<gvt::render::data::accel::BVH::hit> hits =
                          acc.intersect<GVT_SIMD_WIDTH>(raysit.begin(), raysit.end(), -1);

                      gvt::core::Map<int, gvt::render::actor::RayVector> local_queue;
                      for (size_t i = 0; i < hits.size(); i++) {
                        gvt::render::actor::Ray &r = *(raysit.begin() + i);
                        if (hits[i].next != -1) {
                          r.origin = r.origin + r.direction * (hits[i].t * 0.95f);
                          local_queue[hits[i].next].push_back(r);
                        }
                      }
                      for (auto &q : local_queue) {
                        queue_mutex[q.first].lock();
                        queue[q.first].insert(queue[q.first].end(),
                                              std::make_move_iterator(local_queue[q.first].begin()),
                                              std::make_move_iterator(local_queue[q.first].end()));
                        queue_mutex[q.first].unlock();
                      }
                    },
                    ap);

  rays.clear();
}

void ImageTracer::processRays(gvt::render::actor::RayVector &rays, const int src, const int dst) {

  const int chunksize =
      MAX(4096, rays.size() / (gvt::core::CoreContext::instance()->getRootNode()["threads"].value().toInteger() * 4));
  gvt::render::data::accel::BVH &acc = *bvh.get();
  static tbb::simple_partitioner ap;
  tbb::parallel_for(tbb::blocked_range<gvt::render::actor::RayVector::iterator>(rays.begin(), rays.end(), chunksize),
                    [&](tbb::blocked_range<gvt::render::actor::RayVector::iterator> raysit) {

                      gvt::core::Vector<gvt::render::data::accel::BVH::hit> hits =
                          acc.intersect<GVT_SIMD_WIDTH>(raysit.begin(), raysit.end(), src);

                      gvt::core::Map<int, gvt::render::actor::RayVector> local_queue;
                      for (size_t i = 0; i < hits.size(); i++) {
                        gvt::render::actor::Ray &r = *(raysit.begin() + i);
                        if (hits[i].next != -1) {
                          r.origin = r.origin + r.direction * (hits[i].t * 0.95f);
                          local_queue[hits[i].next].push_back(r);
                        } else if (r.type == gvt::render::actor::Ray::SHADOW && glm::length(r.color) > 0) {
                          img->localAdd(r.id, r.color * r.w, 1.f, r.t);
                        }
                      }
                      for (auto &q : local_queue) {
                        queue_mutex[q.first].lock();
                        queue[q.first].insert(queue[q.first].end(),
                                              std::make_move_iterator(local_queue[q.first].begin()),
                                              std::make_move_iterator(local_queue[q.first].end()));
                        queue_mutex[q.first].unlock();
                      }
                    },
                    ap);

  rays.clear();
}

bool ImageTracer::MessageManager(std::shared_ptr<gvt::comm::Message> msg) { return RayTracer::MessageManager(msg); }

bool ImageTracer::isDone() {
  if (queue.empty()) return true;
  for (auto &q : queue)
    if (!q.second.empty()) return false;
  return true;
}
bool ImageTracer::hasWork() { return !isDone(); }
}
}
