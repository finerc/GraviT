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
 * File:   Material.cpp
 * Author: jbarbosa
 *
 * Created on April 18, 2014, 3:07 PM
 */

#include <cmath>
#include <gvt/render/data/DerivedTypes.h>
#include <gvt/render/data/primitives/Material.h>

using namespace gvt::render::actor;
using namespace gvt::render::data::primitives;
using namespace gvt::render::data::scene;

Material::Material() {}

Material::Material(const Material &orig) {}

Material::~Material() {}

glm::vec4 Material::shade(const Ray &ray, const glm::vec4 &sufaceNormal, const Light *lightSource) {
  return glm::vec4();
}

RayVector Material::ao(const Ray &ray, const glm::vec4 &sufaceNormal, float samples) { return RayVector(); }

RayVector Material::secondary(const Ray &ray, const glm::vec4 &sufaceNormal, float samples) { return RayVector(); }

Lambert::Lambert(const glm::vec4 &kd) : Material(), kd(kd) {}

Lambert::Lambert(const Lambert &orig) : Material(orig), kd(orig.kd) {}

Lambert::~Lambert() {}

glm::vec4 Lambert::shade(const Ray &ray, const glm::vec4 &N, const Light *lightSource) {

  glm::vec4 V = glm::normalize(ray.direction);
  float NdotL = std::max(0.f, std::abs(glm::dot(N, V)));
  Color lightSourceContrib = lightSource->contribution(ray);
  Color diffuse = (lightSourceContrib * kd) * (NdotL * ray.w);
  return diffuse;
}

RayVector Lambert::ao(const Ray &ray, const glm::vec4 &sufaceNormal, float samples) { return RayVector(); }

RayVector Lambert::secundary(const Ray &ray, const glm::vec4 &sufaceNormal, float samples) { return RayVector(); }

Phong::Phong(const glm::vec4 &kd, const glm::vec4 &ks, const float &alpha) : Material(), kd(kd), ks(ks), alpha(alpha) {}

Phong::Phong(const Phong &orig) : Material(orig), kd(orig.kd), ks(orig.ks), alpha(orig.alpha) {}

Phong::~Phong() {}

glm::vec4 Phong::shade(const Ray &ray, const glm::vec4 &N, const Light *lightSource) {
  glm::vec4 hitPoint = (glm::vec4)ray.origin + (ray.direction * ray.t);
  glm::vec4 L = glm::normalize(lightSource->position - hitPoint);

  float NdotL = std::max(0.f, glm::dot(N, L));
  glm::vec4 R = ((N * 2.f) * NdotL) - L;
  float VdotR = std::max(0.f, glm::dot(R, (-ray.direction)));
  float power = VdotR * std::pow(VdotR, alpha);

  glm::vec4 lightSourceContrib = lightSource->contribution(ray); //  distance;

  Color finalColor = (lightSourceContrib * kd) * (NdotL * ray.w);
  finalColor += (lightSourceContrib * ks) * (power * ray.w);
  return finalColor;
}

RayVector Phong::ao(const Ray &ray, const glm::vec4 &sufaceNormal, float samples) { return RayVector(); }

RayVector Phong::secundary(const Ray &ray, const glm::vec4 &sufaceNormal, float samples) { return RayVector(); }

BlinnPhong::BlinnPhong(const glm::vec4 &kd, const glm::vec4 &ks, const float &alpha)
    : Material(), kd(kd), ks(ks), alpha(alpha) {}

BlinnPhong::BlinnPhong(const BlinnPhong &orig) : Material(orig), kd(orig.kd), ks(orig.ks), alpha(orig.alpha) {}

BlinnPhong::~BlinnPhong() {}

glm::vec4 BlinnPhong::shade(const Ray &ray, const glm::vec4 &N, const Light *lightSource) {
  glm::vec4 hitPoint = (glm::vec4)ray.origin + (ray.direction * ray.t);
  glm::vec4 L = glm::normalize(lightSource->position - hitPoint);
  float NdotL = std::max(0.f, glm::dot(N, L));

  glm::vec4 H = glm::normalize(L - ray.direction);

  float NdotH = glm::dot(H, N);
  float power = NdotH * std::pow(NdotH, alpha);

  glm::vec4 lightSourceContrib = lightSource->contribution(ray);

  Color diffuse = (lightSourceContrib * kd) * (NdotL * ray.w);
  Color specular = (lightSourceContrib * ks) * (power * ray.w);

  Color finalColor = (diffuse + specular);
  return finalColor;
}

RayVector BlinnPhong::ao(const Ray &ray, const glm::vec4 &sufaceNormal, float samples) { return RayVector(); }

RayVector BlinnPhong::secundary(const Ray &ray, const glm::vec4 &sufaceNormal, float samples) { return RayVector(); }
