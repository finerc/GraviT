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
 * File:   Ray.h
 * Author: jbarbosa
 *
 * Created on March 28, 2014, 1:29 PM
 */
#ifndef GVT_RAY_PACKET_H
#define GVT_RAY_PACKET_H

#include <gvt/core/Debug.h>
#include <gvt/core/Math.h>
#include <gvt/render/actor/Ray.h>
#include <gvt/render/data/primitives/BBox.h>

#include <climits>

namespace gvt {
namespace render {
namespace actor {

template <typename T> inline T fastmin(const T &a, const T &b) { return (a < b) ? a : b; }

template <typename T> inline T fastmax(const T &a, const T &b) { return (a > b) ? a : b; }

template <size_t simd_width> struct RayPacketIntersection {
  float ox[simd_width];
  float oy[simd_width];
  float oz[simd_width];
  float dx[simd_width];
  float dy[simd_width];
  float dz[simd_width];
  float t[simd_width];
  int mask[simd_width];

  //
  // float data[simd_width * 6];
  // float *lx, *ly, *lz, *ux, *uy, *uz;
  inline RayPacketIntersection(const RayVector::iterator &ray_begin, const RayVector::iterator &ray_end) {
#ifdef __USE_TAU
    TAU_PROFILE("RayPacket.h::RayPacketIntersection","",TAU_DEFAULT);
#endif
    size_t i;
    RayVector::iterator rayit = ray_begin;
    for (i = 0; rayit != ray_end && i < simd_width; ++i, ++rayit) {
      Ray &ray = (*rayit);
      ox[i] = ray.origin[0];
      oy[i] = ray.origin[1];
      oz[i] = ray.origin[2];
      dx[i] = 1.f / ray.direction[0];
      dy[i] = 1.f / ray.direction[1];
      dz[i] = 1.f / ray.direction[2];
      t[i] = ray.t_max;
      mask[i] = 1;
    }
    for (; i < simd_width; ++i) {
      t[i] = -1;
      mask[i] = -1;
    }
  }

  inline bool intersect(const gvt::render::data::primitives::Box3D &bb, int hit[], bool update = false) {
    float lx[simd_width];
    float ly[simd_width];
    float lz[simd_width];
    float ux[simd_width];
    float uy[simd_width];
    float uz[simd_width];

    float minx[simd_width];
    float miny[simd_width];
    float minz[simd_width];

    float maxx[simd_width];
    float maxy[simd_width];
    float maxz[simd_width];

    float tnear[simd_width];
    float tfar[simd_width];

    const float blx = bb.bounds_min[0], bly = bb.bounds_min[1], blz = bb.bounds_min[2];
    const float bux = bb.bounds_max[0], buy = bb.bounds_max[1], buz = bb.bounds_max[2];

#pragma simd
    for (size_t i = 0; i < simd_width; ++i) lx[i] = (blx - ox[i]) * dx[i];
#pragma simd
    for (size_t i = 0; i < simd_width; ++i) ly[i] = (bly - oy[i]) * dy[i];
#pragma simd
    for (size_t i = 0; i < simd_width; ++i) lz[i] = (blz - oz[i]) * dz[i];
#pragma simd
    for (size_t i = 0; i < simd_width; ++i) ux[i] = (bux - ox[i]) * dx[i];
#pragma simd
    for (size_t i = 0; i < simd_width; ++i) uy[i] = (buy - oy[i]) * dy[i];
#pragma simd
    for (size_t i = 0; i < simd_width; ++i) uz[i] = (buz - oz[i]) * dz[i];

#pragma simd
    for (size_t i = 0; i < simd_width; ++i) {
      minx[i] = fastmin(lx[i], ux[i]);
      maxx[i] = fastmax(lx[i], ux[i]);
    }

#pragma simd
    for (size_t i = 0; i < simd_width; ++i) {
      miny[i] = fastmin(ly[i], uy[i]);
      maxy[i] = fastmax(ly[i], uy[i]);
    }

#pragma simd
    for (size_t i = 0; i < simd_width; ++i) {
      minz[i] = fastmin(lz[i], uz[i]);
      maxz[i] = fastmax(lz[i], uz[i]);
    }

#pragma simd
    for (size_t i = 0; i < simd_width; ++i) {
      tnear[i] = fastmax(fastmax(minx[i], miny[i]), minz[i]);
      tfar[i] = fastmin(fastmin(maxx[i], maxy[i]), maxz[i]);
    }

#pragma simd
    for (size_t i = 0; i < simd_width; ++i) {
      hit[i] = (tfar[i] > tnear[i] && (!update || tnear[i] > gvt::render::actor::Ray::RAY_EPSILON) && t[i] > tnear[i])
                   ? 1
                   : -1;
    }
#pragma simd
    for (size_t i = 0; i < simd_width; ++i) {
      if (hit[i] == 1 && update) t[i] = tnear[i];
    }

    for (size_t i = 0; i < simd_width; ++i)
      if (hit[i] == 1) return true;

    return false;
  }
};
}
}
}

#endif
