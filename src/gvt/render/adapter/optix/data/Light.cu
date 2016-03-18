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
 * File:   Light.cu
 * Author: Roberto Ribeiro
 *
 * Created on February 4, 2016, 11:00 PM
 */

#include "Light.cuh"
#include "cutil_math.h"

using namespace gvt::render::data::cuda_primitives;


//BaseLight::BaseLight(const float4 position) : position(position) {}

//BaseLight::~BaseLight() {}

__device__ float4 BaseLight::contribution(const Ray &ray) const { return make_float4(0.f); }

//PointLight::PointLight(const float4 position, const float4 color) : BaseLight(position), color(color) {}

//PointLight::~PointLight() {}

__device__ float4 PointLight::contribution(const Ray &ray) const {
  float distance = 1.f / length(((float4)position - ray.origin));
  distance = (distance > 1.f) ? 1.f : distance;
  return color * (distance + 0.5f);
}

//AmbientLight::AmbientLight(const float4 color) : BaseLight(), color(color) {}

//AmbientLight::~AmbientLight() {}

__device__ float4 AmbientLight::contribution(const Ray &ray) const { return color; }