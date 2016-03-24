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

//
// EmbreeMeshAdapter.cpp
//

#define TBB_PREVIEW_STATIC_PARTITIONER 1

#include "gvt/render/adapter/embree/data/EmbreeMeshAdapter.h"

#include "gvt/core/CoreContext.h"

#include <gvt/core/Debug.h>
#include <gvt/core/Math.h>

#include <gvt/render/actor/Ray.h>
// #include <gvt/render/adapter/embree/data/Transforms.h>
#include <gvt/render/data/DerivedTypes.h>
#include <gvt/render/data/primitives/Mesh.h>
#include <gvt/render/data/scene/ColorAccumulator.h>
#include <gvt/render/data/scene/Light.h>

#include <atomic>
#include <future>
#include <thread>

#include <boost/atomic.hpp>
#include <boost/foreach.hpp>
#include <boost/timer/timer.hpp>

#include <tbb/blocked_range.h>
#include <tbb/mutex.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_for_each.h>
#include <tbb/partitioner.h>
#include <tbb/tick_count.h>

// TODO: add logic for other packet sizes

#if defined(GVT_AVX_TARGET)
#define GVT_EMBREE_ALGORITHM RTC_INTERSECT8
#define GVT_EMBREE_PACKET_SIZE 8
#define GVT_EMBREE_PACKET_TYPE RTCRay8
#define GVT_EMBREE_INTERSECTION rtcIntersect8
#define GVT_EMBREE_OCCULUSION rtcOccluded8
#elif defined(GVT_AVX2_TARGET)
#define GVT_EMBREE_ALGORITHM RTC_INTERSECT16
#define GVT_EMBREE_PACKET_SIZE 16
#define GVT_EMBREE_PACKET_TYPE RTCRay16
#define GVT_EMBREE_INTERSECTION rtcIntersect16
#define GVT_EMBREE_OCCULUSION rtcOccluded16
#else
#define GVT_EMBREE_ALGORITHM RTC_INTERSECT4
#define GVT_EMBREE_PACKET_SIZE 4
#define GVT_EMBREE_PACKET_TYPE RTCRay4
#define GVT_EMBREE_INTERSECTION rtcIntersect4
#define GVT_EMBREE_OCCULUSION rtcOccluded4
#endif

using namespace gvt::render::actor;
using namespace gvt::render::adapter::embree::data;
using namespace gvt::render::data::primitives;

static std::atomic<size_t> counter(0);
struct embVertex {
  float x, y, z, a;
};
struct embTriangle {
  int v0, v1, v2;
};

void error_handler(const RTCError code, const char *str = nullptr) {
  if (code == RTC_NO_ERROR) return;

  printf("Embree: ");
  switch (code) {
  case RTC_UNKNOWN_ERROR:
    printf("RTC_UNKNOWN_ERROR");
    break;
  case RTC_INVALID_ARGUMENT:
    printf("RTC_INVALID_ARGUMENT");
    break;
  case RTC_INVALID_OPERATION:
    printf("RTC_INVALID_OPERATION");
    break;
  case RTC_OUT_OF_MEMORY:
    printf("RTC_OUT_OF_MEMORY");
    break;
  case RTC_UNSUPPORTED_CPU:
    printf("RTC_UNSUPPORTED_CPU");
    break;
  case RTC_CANCELLED:
    printf("RTC_CANCELLED");
    break;
  default:
    printf("invalid error code");
    break;
  }
  if (str) {
    printf(" (");
    while (*str) putchar(*str++);
    printf(")\n");
  }
  exit(1);
}

EmbreeMeshAdapter::EmbreeMeshAdapter(std::map<int, gvt::render::data::primitives::Mesh *> &meshRef,
                                     std::map<int, glm::mat4 *> &instM, std::map<int, glm::mat4 *> &instMinv,
                                     std::map<int, glm::mat3 *> &instMinvN,
                                     std::vector<gvt::render::data::scene::Light *> &lights,
                                     std::vector<size_t> instances, bool unique)
    : Adapter(meshRef, instM, instMinv, instMinvN, lights, instances, unique) {

  device = rtcNewDevice(NULL);
  error_handler(rtcDeviceGetError(device));
  /* set error handler */
  rtcDeviceSetErrorFunction(device, error_handler);

  global_scene = rtcDeviceNewScene(device, RTC_SCENE_STATIC, GVT_EMBREE_ALGORITHM);

  for (auto &inst : instances) {

    // int i = inst;
    Mesh *mesh = meshRef[inst];
    GVT_ASSERT(mesh, "EmbreeMeshAdapter: mesh pointer in the database is null");

    if (std::find(_mesh.begin(), _mesh.end(), mesh) == _mesh.end()) {
      mesh->generateNormals();
      int numVerts = mesh->vertices.size();
      int numTris = mesh->faces.size();
      RTCScene scene = rtcDeviceNewScene(device, RTC_SCENE_STATIC, GVT_EMBREE_ALGORITHM);
      unsigned geomId = rtcNewTriangleMesh(scene, RTC_GEOMETRY_STATIC, numTris, numVerts);

      embVertex *vertices = (embVertex *)rtcMapBuffer(scene, geomId, RTC_VERTEX_BUFFER);
      embTriangle *triangles = (embTriangle *)rtcMapBuffer(scene, geomId, RTC_INDEX_BUFFER);

      for (int i = 0; i < numVerts; i++) {
        vertices[i].x = mesh->vertices[i][0];
        vertices[i].y = mesh->vertices[i][1];
        vertices[i].z = mesh->vertices[i][2];
      }

      for (int i = 0; i < numTris; i++) {
        gvt::render::data::primitives::Mesh::Face f = mesh->faces[i];
        triangles[i].v0 = f.get<0>();
        triangles[i].v1 = f.get<1>();
        triangles[i].v2 = f.get<2>();
      }

      rtcUnmapBuffer(scene, geomId, RTC_VERTEX_BUFFER);
      rtcUnmapBuffer(scene, geomId, RTC_INDEX_BUFFER);
      rtcCommit(scene);

      unsigned instID = rtcNewInstance(global_scene, scene);
      glm::mat4 *m = instM[inst];
      glm::mat4 tt = glm::transpose(*m);
      float *n = glm::value_ptr(tt);

      float mm[] = { n[0], n[4], n[8], n[1], n[5], n[9], n[2], n[6], n[10], n[3], n[7], n[11] };
      rtcSetTransform(global_scene, instID, RTC_MATRIX_COLUMN_MAJOR, mm);
      rtcUpdate(global_scene, instID);
      _scene.push_back(scene);
      _mesh.insert(mesh);
      _inst2mesh[instID] = mesh;
      _inst2mat[instID] = instMinvN[inst];
      mesh2scene[mesh] = scene;
      scene2geomid[scene] = geomId;

    } else {
      unsigned instID = rtcNewInstance(global_scene, mesh2scene[mesh]);
      glm::mat4 *m = instM[inst];
      // (gvt::core::math::AffineTransformMatrix<float> *)node["mat"].value().toULongLong();
      glm::mat4 tt = glm::transpose(*m);
      float *n = glm::value_ptr(tt);

      float mm[] = { n[0], n[4], n[8], n[1], n[5], n[9], n[2], n[6], n[10], n[3], n[7], n[11] };

      rtcSetTransform(global_scene, instID, RTC_MATRIX_COLUMN_MAJOR, mm);

      rtcUpdate(global_scene, instID);
      _inst2mesh[instID] = mesh;
      _inst2mat[instID] = instMinvN[inst];
    }
  }

  // TODO: note: embree doesn't save normals in its mesh structure, have to
  // calculate the normal based on uv value
  // later we might have to copy the mesh normals to a local structure so we can
  // correctly calculate the bounced rays

  rtcCommit(global_scene);
}

EmbreeMeshAdapter::~EmbreeMeshAdapter() {
  for (auto &sg : scene2geomid) rtcDeleteGeometry(sg.first, sg.second);
  for (auto scene : _scene) rtcDeleteScene(scene);
  rtcDeleteScene(global_scene);
  rtcDeleteDevice(device);
}

struct embreeParallelTrace {
  /**
   * Pointer to EmbreeMeshAdapter to get Embree scene information
   */
  gvt::render::adapter::embree::data::EmbreeMeshAdapter *adapter;

  /**
   * Shared ray list used in the current trace() call
   */
  gvt::render::actor::RayVector &rayList;

  /**
   * Shared outgoing ray list used in the current trace() call
   */
  gvt::render::actor::RayVector &moved_rays;

  /**
   * Number of rays to work on at once [load balancing].
   */
  const size_t workSize;

  /**
   * Index into the shared `rayList`.  Atomically incremented to 'grab'
   * the next set of rays.
   */
  // std::atomic<size_t> &sharedIdx;

  /**
   * Stored transformation matrix in the current instance
   */
  // const glm::mat4 *m;
  //
  // /**
  //  * Stored inverse transformation matrix in the current instance
  //  */
  // const glm::mat4 *minv;
  //
  // /**
  //  * Stored upper33 inverse matrix in the current instance
  //  */
  // const glm::mat3 *normi;

  /**
   * Stored transformation matrix in the current instance
   */
  const std::vector<gvt::render::data::scene::Light *> &lights;

  /**
   * Count the number of rays processed by the current trace() call.
   *
   * Used for debugging purposes
   */
  std::atomic<size_t> &counter;

  /**
   * Thread local outgoing ray queue
   */
  gvt::render::actor::RayVector localDispatch;

  /**
   * List of shadow rays to be processed
   */
  gvt::render::actor::RayVector shadowRays;

  /**
   * Size of Embree packet
   */
  // const size_t packetSize; // TODO: later make this configurable

  const size_t begin, end;

  // gvt::render::data::primitives::Mesh *mesh;
  /**
   * Construct a embreeParallelTrace struct with information needed for the
   * thread
   * to do its tracing
   */
  embreeParallelTrace(gvt::render::adapter::embree::data::EmbreeMeshAdapter *adapter,
                      gvt::render::actor::RayVector &rayList, gvt::render::actor::RayVector &moved_rays,
                      const size_t workSize, std::vector<gvt::render::data::scene::Light *> &lights,
                      std::atomic<size_t> &counter, const size_t begin, const size_t end)
      : adapter(adapter), rayList(rayList), moved_rays(moved_rays), workSize(workSize), lights(lights),
        counter(counter), begin(begin), end(end) {}
  /**
   * Convert a set of rays from a vector into a GVT_EMBREE_PACKET_TYPE ray packet.
   *
   * \param ray4          reference of GVT_EMBREE_PACKET_TYPE struct to write to
   * \param valid         aligned array of 4 ints to mark valid rays
   * \param resetValid    if true, reset the valid bits, if false, re-use old
   * valid to know which to convert
   * \param packetSize    number of rays to convert
   * \param rays          vector of rays to read from
   * \param startIdx      starting point to read from in `rays`
   */
  void prepGVT_EMBREE_PACKET_TYPE(GVT_EMBREE_PACKET_TYPE &ray4, int valid[GVT_EMBREE_PACKET_SIZE],
                                  const bool resetValid, const int localPacketSize, gvt::render::actor::RayVector &rays,
                                  const size_t startIdx) {
    // reset valid to match the number of active rays in the packet
    if (resetValid) {
      for (int i = 0; i < localPacketSize; i++) {
        valid[i] = -1;
      }
      for (int i = localPacketSize; i < localPacketSize; i++) {
        valid[i] = 0;
      }
    }

    // convert localPacketSize rays into embree's GVT_EMBREE_PACKET_TYPE struct
    for (int i = 0; i < localPacketSize; i++) {
      if (valid[i]) {
        const Ray &r = rays[startIdx + i];
        // const auto origin = r.origin; // transform ray to local space
        // const auto direction = r.direction;

        //      const auto &origin = r.origin; // transform ray to local space
        //      const auto &direction = r.direction;

        ray4.orgx[i] = r.origin[0];
        ray4.orgy[i] = r.origin[1];
        ray4.orgz[i] = r.origin[2];
        ray4.dirx[i] = r.direction[0];
        ray4.diry[i] = r.direction[1];
        ray4.dirz[i] = r.direction[2];
        ray4.tnear[i] = gvt::render::actor::Ray::RAY_EPSILON;
        ray4.tfar[i] = FLT_MAX;
        ray4.geomID[i] = RTC_INVALID_GEOMETRY_ID;
        ray4.primID[i] = RTC_INVALID_GEOMETRY_ID;
        ray4.instID[i] = RTC_INVALID_GEOMETRY_ID;
        ray4.mask[i] = -1;
        ray4.time[i] = gvt::render::actor::Ray::RAY_EPSILON;
      }
    }
  }

  /**
   * Generate shadow rays for a given ray
   *
   * \param r ray to generate shadow rays for
   * \param normal calculated normal
   * \param primId primitive id for shading
   * \param mesh pointer to mesh struct [TEMPORARY]
   */
  void generateShadowRays(const gvt::render::actor::Ray &r, const glm::vec3 &normal, int primID,
                          gvt::render::data::primitives::Mesh *mesh, unsigned int *randSeed) {
    for (gvt::render::data::scene::Light *light : lights) {
      GVT_ASSERT(light, "generateShadowRays: light is null for some reason");
      // Try to ensure that the shadow ray is on the correct side of the
      // triangle.
      // Technique adapted from "Robust BVH Ray Traversal" by Thiago Ize.
      // Using about 8 * ULP(t).

      gvt::render::data::Color c;
      glm::vec3 lightPos;
      if (light->LightT == gvt::render::data::scene::Light::Area) {
        lightPos = ((gvt::render::data::scene::AreaLight *)light)->GetPosition(randSeed);
        c = mesh->shadeFaceAreaLight(primID, r, normal, light, lightPos);
      } else {
        // FIXME: remove dependency on mesh->shadeFace
        c = mesh->shadeFace(primID, r, normal, light);
        lightPos = light->position;
      }

      const float multiplier = 1.f - 16.0f * gvt::render::actor::Ray::RAY_EPSILON;
      const glm::vec3 origin = r.origin + r.direction * (r.t * multiplier);
      const glm::vec3 dir = light->position - origin;
      const float t_max = glm::length(dir);

      // note: ray copy constructor is too heavy, so going to build it manually
      shadowRays.push_back(Ray(origin, glm::normalize(dir), r.w, Ray::SHADOW, r.depth));

      Ray &shadow_ray = shadowRays.back();
      shadow_ray.t_min = r.t_min;
      shadow_ray.t = t_max;
      shadow_ray.id = r.id;
      shadow_ray.t_max = t_max;
      shadow_ray.color = c;
    }
  }

  /**
   * Test occlusion for stored shadow rays.  Add missed rays
   * to the dispatch queue.
   */
  void traceShadowRays() {
    RTCScene scene = adapter->getScene();
    GVT_EMBREE_PACKET_TYPE ray4 = {};
    RTCORE_ALIGN(16) int valid[GVT_EMBREE_PACKET_SIZE] = { 0 };

    for (size_t idx = 0; idx < shadowRays.size(); idx += GVT_EMBREE_PACKET_SIZE) {
      const size_t localPacketSize =
          (idx + GVT_EMBREE_PACKET_SIZE > shadowRays.size()) ? (shadowRays.size() - idx) : GVT_EMBREE_PACKET_SIZE;

      // create a shadow packet and trace with rtcOccluded
      prepGVT_EMBREE_PACKET_TYPE(ray4, valid, true, localPacketSize, shadowRays, idx);
      GVT_EMBREE_OCCULUSION(valid, scene, ray4);

      for (size_t pi = 0; pi < localPacketSize; pi++) {
        if (valid[pi] && ray4.geomID[pi] == (int)RTC_INVALID_GEOMETRY_ID) {
          // ray is valid, but did not hit anything, so add to dispatch queue
          localDispatch.push_back(shadowRays[idx + pi]);
        }
      }
    }
    shadowRays.clear();
  }

  /**
   * Trace function.
   *
   * Loops through rays in `rayList`, converts them to embree format, and traces
   * against embree's scene
   *
   * Threads work on rays in chunks of `workSize` units.  An atomic add on
   * `sharedIdx` distributes
   * the ranges of rays to work on.
   *
   * After getting a chunk of rays to work with, the adapter loops through in
   * sets of `packetSize`.  Right
   * now this supports a 4 wide packet [Embree has support for 8 and 16 wide
   * packets].
   *
   * The packet is traced and re-used until all of the 4 rays and their
   * secondary rays have been traced to
   * completion.  Shadow rays are added to a queue and are tested after each
   * intersection test.
   *
   * The `while(validRayLeft)` loop behaves something like this:
   *
   * r0: primary -> secondary -> secondary -> ... -> terminated
   * r1: primary -> secondary -> secondary -> ... -> terminated
   * r2: primary -> secondary -> secondary -> ... -> terminated
   * r3: primary -> secondary -> secondary -> ... -> terminated
   *
   * It is possible to get diverging packets such as:
   *
   * r0: primary   -> secondary -> terminated
   * r1: secondary -> secondary -> terminated
   * r2: shadow    -> terminated
   * r3: primary   -> secondary -> secondary -> secondary -> terminated
   *
   * TODO: investigate switching terminated rays in the vector with active rays
   * [swap with ones at the end]
   *
   * Terminated above means:
   * - shadow ray hits object and is occluded
   * - primary / secondary ray miss and are passed out of the queue
   *
   * After a packet is completed [including its generated rays], the system
   * moves on * to the next packet
   * in its chunk. Once a chunk is completed, the thread increments `sharedIdx`
   * again to get more work.
   *
   * If `sharedIdx` grows to be larger than the incoming ray size, then the
   * thread is complete.
   */
  void operator()() {
#ifdef GVT_USE_DEBUG
    boost::timer::auto_cpu_timer t_functor("EmbreeMeshAdapter: thread trace time: %w\n");
#endif
    GVT_DEBUG(DBG_ALWAYS, "EmbreeMeshAdapter: started thread");

    GVT_DEBUG(DBG_ALWAYS, "EmbreeMeshAdapter: getting mesh [hack for now]");
    RTCScene scene = adapter->getScene();
    localDispatch.reserve((end - begin) * 2);
    shadowRays.reserve(GVT_EMBREE_PACKET_SIZE * lights.size());
    GVT_DEBUG(DBG_ALWAYS, "EmbreeMeshAdapter: starting while loop");
    TLRand randEngine;
    randEngine.SetSeed(begin);
    GVT_EMBREE_PACKET_TYPE ray4 = {};
    RTCORE_ALIGN(16) int valid[GVT_EMBREE_PACKET_SIZE] = { 0 };

    for (size_t localIdx = begin; localIdx < end; localIdx += GVT_EMBREE_PACKET_SIZE) {
      // this is the local packet size. this might be less than the main
      // packetSize due to uneven amount of rays
      const size_t localPacketSize =
          (localIdx + GVT_EMBREE_PACKET_SIZE > end) ? (end - localIdx) : GVT_EMBREE_PACKET_SIZE;

      // trace a packet of rays, then keep tracing the generated secondary
      // rays to completion
      // tracks to see if there are any valid rays left in the packet, if so,
      // keep tracing
      // NOTE: perf issue: this will cause smaller and smaller packets to be
      // traced at a time - need to track to see effects
      bool validRayLeft = true;

      // the first time we enter the loop, we want to reset the valid boolean
      // list that was
      // modified with the previous packet
      bool resetValid = true;
      while (validRayLeft) {
        validRayLeft = false;

        prepGVT_EMBREE_PACKET_TYPE(ray4, valid, resetValid, localPacketSize, rayList, localIdx);
        GVT_EMBREE_INTERSECTION(valid, scene, ray4);
        resetValid = false;

        for (size_t pi = 0; pi < localPacketSize; pi++) {

          if (valid[pi]) {
            // counter++; // tracks rays processed [atomic]
            auto &r = rayList[localIdx + pi];

            //  std::cout << "->" << ray4.instID[pi] << " : " << ray4.geomID[pi] << " : " << ray4.primID[pi] <<
            //  std::endl;

            // std::cout << "." << std::endl;
            if (ray4.geomID[pi] != (int)RTC_INVALID_GEOMETRY_ID) {
              // ray has hit something
              Mesh *mesh = (*adapter)._inst2mesh[ray4.instID[pi]];
              glm::mat3 *m = (*adapter)._inst2mat[ray4.instID[pi]];
              // gvt::core::math::AffineTransformMatrix<float> *m = _inst2mat[ray4.instID[pi]];

              // shadow ray hit something, so it should be dropped
              if (r.type == gvt::render::actor::Ray::SHADOW) {
                continue;
              }

              float t = ray4.tfar[pi];
              r.t = t;

              // FIXME: embree does not take vertex normal information, the
              // examples have the application calculate the normal using
              // math similar to the bottom.  this means we have to keep
              // around a 'faces_to_normals' list along with a 'normals' list
              // for the embree adapter
              //
              // old fixme: fix embree normal calculation to remove dependency
              // from gvt mesh
              // for some reason the embree normals aren't working, so just
              // going to manually calculate the triangle normal
              // Vector4f embreeNormal = Vector4f(ray4.Ngx[pi], ray4.Ngy[pi],
              // ray4.Ngz[pi], 0.0);

              glm::vec3 manualNormal;
              {
                const int triangle_id = ray4.primID[pi];
#ifndef FLAT_SHADING
                const float u = ray4.u[pi];
                const float v = ray4.v[pi];
                const Mesh::FaceToNormals &normals = mesh->faces_to_normals[triangle_id]; // FIXME: need to
                                                                                          // figure out
                                                                                          // to store
                                                                                          // `faces_to_normals`
                                                                                          // list
                const glm::vec3 &a = mesh->normals[normals.get<1>()];
                const glm::vec3 &b = mesh->normals[normals.get<2>()];
                const glm::vec3 &c = mesh->normals[normals.get<0>()];
                manualNormal = a * u + b * v + c * (1.0f - u - v);

                manualNormal = glm::normalize((*m) * manualNormal);
#else
                int I = mesh->faces[triangle_id].get<0>();
                int J = mesh->faces[triangle_id].get<1>();
                int K = mesh->faces[triangle_id].get<2>();

                glm::vec3 a = mesh->vertices[I];
                glm::vec3 b = mesh->vertices[J];
                glm::vec3 c = mesh->vertices[K];
                glm::vec3 u = b - a;
                glm::vec3 v = c - a;
                rayList glm::vec3 normal;
                normal[0] = u[1] * v[2] - u[2] * v[1];
                normal[1] = u[2] * v[0] - u[0] * v[2];
                normal[2] = u[0] * v[1] - u[1] * v[0];
                manualNormal = glm::normalize((*m) * normal);
#endif
              }
              const glm::vec3 &normal = manualNormal;

              // reduce contribution of the color that the shadow rays get
              if (r.type == gvt::render::actor::Ray::SECONDARY) {
                t = (t > 1) ? 1.f / t : t;
                r.w = r.w * t;
              }
              unsigned int *randSeed = randEngine.ReturnSeed();

              generateShadowRays(r, normal, ray4.primID[pi], mesh, randSeed);

              int ndepth = r.depth - 1;

              float p = 1.f - randEngine.fastrand(0, 1); //(float(rand()) / RAND_MAX);
              // replace current ray with generated secondary ray
              if (ndepth > 0 && r.w > p) {
                r.type = gvt::render::actor::Ray::SECONDARY;
                const float multiplier =
                    1.0f - 16.0f * gvt::render::actor::Ray::RAY_EPSILON; // TODO: move out somewhere / make static
                const float t_secondary = multiplier * r.t;
                r.origin = r.origin + r.direction * t_secondary;

                // TODO: remove this dependency on mesh, store material object in the database
                // r.setDirection(adapter->getMesh()->getMaterial()->CosWeightedRandomHemisphereDirection2(normal));
                r.direction = glm::normalize(mesh->getMaterial()->CosWeightedRandomHemisphereDirection2(normal));

                r.w = r.w * glm::dot(r.direction, normal);
                r.depth = ndepth;
                validRayLeft = true; // we still have a valid ray in the packet to trace
              } else {
                // secondary ray is terminated, so disable its valid bit
                valid[pi] = 0;
              }
            } else {
              valid[pi] = 0;
              if (!(*adapter).unique || r.type == gvt::render::actor::Ray::SHADOW) localDispatch.push_back(r);
              // ray is valid, but did not hit anything, so add to dispatch
              // queue and disable it
            }
          }
        }

        // trace shadow rays generated by the packet
        traceShadowRays();
      }
    }

#ifdef GVT_USE_DEBUG
    size_t shadow_count = 0;
    size_t primary_count = 0;
    size_t secondary_count = 0;
    size_t other_count = 0;
    for (auto &r : localDispatch) {
      switch (r.type) {
      case gvt::render::actor::Ray::SHADOW:
        shadow_count++;
        break;
      case gvt::render::actor::Ray::PRIMARY:
        primary_count++;
        break;
      case gvt::render::actor::Ray::SECONDARY:
        secondary_count++;
        break;
      default:
        other_count++;
        break;
      }
    }
    GVT_DEBUG(DBG_ALWAYS, "Local dispatch : " << localDispatch.size() << ", types: primary: " << primary_count
                                              << ", shadow: " << shadow_count << ", secondary: " << secondary_count
                                              << ", other: " << other_count);
#endif

    // copy localDispatch rays to outgoing rays queue
    std::unique_lock<std::mutex> moved(adapter->_outqueue);
    moved_rays.insert(moved_rays.end(), localDispatch.begin(), localDispatch.end());
    moved.unlock();
  }
};

void EmbreeMeshAdapter::trace(gvt::render::actor::RayVector &rayList, gvt::render::actor::RayVector &moved_rays,
                              size_t _begin, size_t _end) {
#ifdef GVT_USE_DEBUG
  boost::timer::auto_cpu_timer t_functor("EmbreeMeshAdapter: trace time: %w\n");
#endif

  if (_end == 0) _end = rayList.size();

  this->begin = _begin;
  this->end = _end;

  const size_t numThreads = std::thread::hardware_concurrency();
  const size_t workSize = std::max((size_t)4, (size_t)((end - begin) / (numThreads * 2))); // size of 'chunk'
                                                                                           // of rays to work
                                                                                           // on
  static tbb::auto_partitioner ap;
  tbb::parallel_for(tbb::blocked_range<size_t>(begin, end, workSize),
                    [&](tbb::blocked_range<size_t> chunk) {

                      embreeParallelTrace(this, rayList, moved_rays, chunk.end() - chunk.begin(), lights, counter,
                                          chunk.begin(), chunk.end())();
                    },
                    ap);

  GVT_DEBUG(DBG_ALWAYS, "EmbreeMeshAdapter: Forwarding rays: " << moved_rays.size());
}
