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
#ifndef GVT_RENDER_RENDERER_H
#define GVT_RENDER_RENDERER_H

#include <gvt/core/context/Variant.h>
#include <gvt/render/RenderContext.h>
#include <gvt/render/Schedulers.h>
#include <gvt/render/data/scene/Image.h>
#include <gvt/render/data/scene/gvtCamera.h>
#include <gvt/render/algorithm/Tracers.h>

namespace gvt {
namespace render {
/// GraviT rendering object. Builds components from context and renders scene. 
/** the renderer class contains instances of the camera, film, and scene data. It
 * builds these objects upon construction. It also calls the sync context function
 * before rendering to synchronizes all of the context modifications.
*/
class gvtRenderer {
public:
  //static void CreateRenderer();
  ~gvtRenderer();
  static gvtRenderer *instance();
  void render();
  void WriteImage();

protected:
  gvtRenderer(); // constructor
  static gvtRenderer *__singleton;
  RenderContext *ctx;
  data::scene::gvtPerspectiveCamera *camera;
  data::scene::Image *myimage;
  algorithm::AbstractTrace *tracer;
  gvt::core::DBNodeH rootnode;
  gvt::core::DBNodeH datanode;
  gvt::core::DBNodeH instancesnode;
  gvt::core::DBNodeH lightsnode;
  gvt::core::DBNodeH cameranode;
  gvt::core::DBNodeH filmnode;
  
};
}
}

#endif // GVT_RENDER_CONTEXT_H
