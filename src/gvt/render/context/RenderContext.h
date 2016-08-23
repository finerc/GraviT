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

   GraviT is funded in part by the US National Science Foundation under awards
   ACI-1339863,
   ACI-1339881 and ACI-1339840
   =======================================================================================
   */
#ifndef GVT_RENDER_CONTEXT_H
#define GVT_RENDER_CONTEXT_H

#include <memory>

#include <gvt/core/Context.h>
#include <gvt/core/composite/Composite.h>
#include <gvt/render/data/scene/Image.h>
#include <gvt/render/data/scene/gvtCamera.h>

namespace gvt {
namespace render {
/// internal context for GraviT ray tracing renderer
/** \sa CoreContext
*/
class RenderContext : public gvt::core::CoreContext {
public:
  static void CreateContext();
  virtual ~RenderContext();
  gvt::core::DBNodeH createNodeFromType(gvt::core::String type, gvt::core::String name,
                                        gvt::core::Uuid parent = gvt::core::Uuid::null());
  static RenderContext *instance();

  std::shared_ptr<gvt::render::data::scene::gvtCameraBase> getCamera();
  void setCamera(std::shared_ptr<gvt::render::data::scene::gvtCameraBase>);

  std::shared_ptr<gvt::render::data::scene::Image> getImage();
  void setImage(std::shared_ptr<gvt::render::data::scene::Image>);
  template <typename T> std::shared_ptr<T> getComposite() {
    return std::dynamic_pointer_cast<T>(_composite);
  }
  void setComposite(std::shared_ptr<gvt::core::composite::AbstractCompositeBuffer>);

protected:
  RenderContext();

  std::shared_ptr<gvt::render::data::scene::Image> _img;
  std::shared_ptr<gvt::render::data::scene::gvtCameraBase> _camera;
  std::shared_ptr<gvt::core::composite::AbstractCompositeBuffer> _composite;
};
}
}

#endif // GVT_RENDER_CONTEXT_H
