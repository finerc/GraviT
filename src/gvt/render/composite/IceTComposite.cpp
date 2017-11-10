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

#include <cstdlib>
#include <fstream>
#include <gvt/render/composite/IceTComposite.h>
#include <iostream>
#include <mpi.h>
#include <sstream>

#include <IceT.h>
#include <IceTDevCommunication.h>
#include <IceTDevContext.h>
#include <IceTDevDiagnostics.h>
#include <IceTDevImage.h>
#include <IceTDevPorting.h>
#include <IceTDevState.h>
#include <IceTMPI.h>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/partitioner.h>
#include <thread>

#include <glm/glm.hpp>
namespace gvt {
namespace render {
namespace composite {

const IceTFloat black[] = { 1.0, 0.0, 0.0, 1.0 };

IceTComposite::IceTComposite(std::size_t width, std::size_t height)
    : gvt::render::composite::ImageComposite(width, height) {

  comm = icetCreateMPICommunicator(MPI::COMM_WORLD);
  icetCreateContext(comm);
  //
  icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);
  //
  icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_FLOAT);
  icetSetDepthFormat(ICET_IMAGE_DEPTH_NONE);
  // icetSetDepthFormat(ICET_IMAGE_DEPTH_FLOAT);
  icetCompositeMode(ICET_COMPOSITE_MODE_BLEND);
  // icetCompositeMode(ICET_COMPOSITE_MODE_Z_BUFFER);
  icetStrategy(ICET_STRATEGY_VTREE);
  icetSingleImageStrategy(ICET_SINGLE_IMAGE_STRATEGY_AUTOMATIC);

  color_buffer = static_cast<IceTFloat *>(malloc(width * height * 4 * sizeof(IceTFloat)));
  depth_buffer = static_cast<IceTFloat *>(malloc(width * height * sizeof(IceTFloat)));
  reset();
}

void IceTComposite::resize(int w, int h) {
  width = w;
  height = h;
  if(color_buffer) free(color_buffer);
  if(depth_buffer) free(depth_buffer);
  color_buffer = static_cast<IceTFloat *>(malloc(width * height * 4 * sizeof(IceTFloat)));
  depth_buffer = static_cast<IceTFloat *>(malloc(width * height * sizeof(IceTFloat)));
  reset();
}

IceTComposite::~IceTComposite() {
  free(color_buffer);
  free(depth_buffer);
  // icetDestroyMPICommunicator(comm);
}

void IceTComposite::reset() {
  memset(color_buffer, 0, (width * height * 4 * sizeof(IceTFloat)));
  memset(depth_buffer, 0, (width * height * sizeof(IceTFloat)));
}

float *IceTComposite::composite() {
  IceTSizeType num_pixels;
  IceTSizeType i;
  icetResetTiles();
  icetAddTile(0, 0, width, height, 0);
  // IceTInt g_local_valid_pixel_viewport[4] = { 0, 0, width, height };
  int size;
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  num_pixels = width * height;
  if(size == 1)
    color_buffer_final = color_buffer;
  else {
  IceTImage image = icetCompositeImage(color_buffer, NULL, NULL, NULL, NULL, black);
  num_pixels = icetImageGetNumPixels(image);
  color_buffer_final = icetImageGetColorf(image);
  }
  return color_buffer_final;
}

void IceTComposite::localAdd(size_t x, size_t y, const glm::vec3 &color, float alpha, float t) {
  for (int i = 0; i < 3; i++) {
    color_buffer[(y * width + x) * 4 + i] += color[i];
    if (color_buffer[(y * width + x) * 4 + i] > 1.f) color_buffer[(y * width + x) * 4 + i] = 1.f;
  }
  color_buffer[(y * width + x) * 4 + 3] += alpha;
};

void IceTComposite::localAdd(size_t idx, const glm::vec3 &color, float alpha, float t) {
  for (int i = 0; i < 3; i++) {
    color_buffer[idx * 4 + i] += color[i];
    if (color_buffer[idx * 4 + i] > 1.f) color_buffer[idx * 4 + i] = 1.f;
  }
  color_buffer[idx * 4 + 3] += alpha;
};

void IceTComposite::write(std::string filename) {

  if (MPI::COMM_WORLD.Get_rank() != 0) return;

  std::string ext = ".ppm";
  // switch (format) {
  // case PPM:
  //   ext = ".ppm";
  //   break;
  // default:
  //   GVT_DEBUG(DBG_ALWAYS, "ERROR: unknown image format '" << format << "'");
  //   return;
  // }

  std::stringstream header;
  header << "P6" << std::endl << std::flush;
  header << width << " " << height << std::endl << std::flush;
  header << "255" << std::endl << std::flush;

  std::fstream file;
  file.open((filename + ext).c_str(), std::fstream::out | std::fstream::trunc | std::fstream::binary);
  file << header.str();

  std::cout << "Image write " << width << " x " << height << std::endl << std::flush;
  // reverse row order so image is correctly oriented
  for (int j = height - 1; j >= 0; j--) {
    int offset = j * width;
    for (int i = 0; i < width; ++i) {
      int index = 4 * (offset + i);
      // if (rgb[index + 0] == 0 || rgb[index + 1] == 0 || rgb[index + 2] == 0) std::cout
      // << ".";
      file << (unsigned char)(color_buffer_final[index + 0] * 255)
           << (unsigned char)(color_buffer_final[index + 1] * 255)
           << (unsigned char)(color_buffer_final[index + 2] * 255);
    }
  }
  std::cout << std::endl << std::flush;
  file.close();
};
}
}
}
