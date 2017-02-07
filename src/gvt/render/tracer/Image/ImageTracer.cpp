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
#include <gvt/core/utils/timer.h>
namespace gvt {
namespace tracer {
ImageTracer::ImageTracer() : gvt::tracer::RayTracer() {
  // queue_mutex = new std::mutex[meshRef.size()];
  // for (auto &m : meshRef) {
  //   queue[m.first] = gvt::render::actor::RayVector();
  // }
}
ImageTracer::~ImageTracer() {
  // if (queue_mutex != nullptr) delete[] queue_mutex;
  // queue.clear();
}

void ImageTracer::resetBVH() {
  RayTracer::resetBVH();
  // if (queue_mutex != nullptr) delete[] queue_mutex;
  // for (auto &m : meshRef) {
  //   queue[m.first] = gvt::render::actor::RayVector();
  // }
}

void ImageTracer::operator()() {

  img->reset();

  gvt::core::time::timer t_frame(true, "image tracer: frame: ");
  gvt::core::time::timer t_all(false, "image tracer: all timers: ");
  gvt::core::time::timer t_gather(false, "image tracer: gather: ");
  gvt::core::time::timer t_shuffle(false, "image tracer: shuffle: ");
  gvt::core::time::timer t_tracer(false, "image tracer: adapter+trace : ");
  gvt::core::time::timer t_select(false, "image tracer: select : ");
  gvt::core::time::timer t_filter(false, "image tracer: filter : ");
  gvt::core::time::timer t_camera(false, "image tracer: gen rays : ");
  t_camera.resume();
  cam->AllocateCameraRays();
  cam->generateRays();
  t_camera.stop();
  t_filter.resume();
  processRaysAndDrop(cam->rays);
  t_filter.stop();
  gvt::render::actor::RayVector toprocess;
  gvt::render::actor::RayVector returned_rays;
  do {
    int target = -1;
    int amount = 0;
    t_select.resume();
    // for (auto &q : queue) {
    //   if (isInNode(q.first) && q.second.size() > amount) {
    //     amount = q.second.size();
    //     target = q.first;
    //   }
    // }
    queue.dequeue(target, toprocess);
    t_select.stop();

    if (target != -1) {
      t_tracer.resume();
      // gc_rays.add(toprocess.size());
      RayTracer::calladapter(target, toprocess, returned_rays);
      t_shuffle.resume();
      // gc_shuffle.add(returned_rays.size());
      processRays(returned_rays, target);
      t_shuffle.stop();
    }
    // t_select.resume();
    // for (auto &q : queue) {
    //   if (q.second.size() > amount) {
    //     amount = q.second.size();
    //     target = q.first;
    //   }
    // }
    // t_select.stop();
    // if (target != -1) {
    //   t_tracer.resume();
    //   returned_rays.reserve(queue[target].size() * 10);
    //   RayTracer::calladapter(target, queue[target], returned_rays);
    //   queue[target].clear();
    //   t_tracer.stop();
    //   t_shuffle.resume();
    //   processRays(returned_rays, target);
    //   t_shuffle.stop();
    // }
  } while (hasWork());
  t_gather.resume();
  img->composite();
  t_gather.stop();
  t_all = t_gather + t_shuffle + t_tracer + t_select + t_filter;
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
                        for (auto &q : local_queue) {
                          queue.enqueue(q.first, q.second);
                        }
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
                        queue.enqueue(q.first, q.second);
                      }
                    },
                    ap);

  rays.clear();
}

bool ImageTracer::MessageManager(std::shared_ptr<gvt::comm::Message> msg) { return RayTracer::MessageManager(msg); }

bool ImageTracer::isDone() {
  if (queue.empty()) return true;
  // for (auto &q : queue)
  //   if (!q.second.empty()) return false;
  return true;
}
bool ImageTracer::hasWork() { return !isDone(); }
}
}
