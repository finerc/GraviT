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
/**
 * A simple GraviT application that loads some geometry and renders it.
 *
 * This application renders a simple scene of cones and cubes using the GraviT interface.
 * This will run in both single-process and MPI modes. You can alter the work scheduler
 * used by changing line 242.
 *
*/
#include <algorithm>
#include <gvt/core/Math.h>
#include <gvt/core/mpi/Wrapper.h>
#include <gvt/render/Context.h>
#include <gvt/render/Schedulers.h>
#include <gvt/render/Types.h>
#include <gvt/render/data/Domains.h>
#include <set>
#include <vector>

#include <tbb/task_scheduler_init.h>
#include <thread>


#include <gvt/core/Context.h>
#include <gvt/core/comm/comm.h>
#include <gvt/render/Context.h>

#include <gvt/render/composite/IceTComposite.h>
#include <gvt/render/composite/ImageComposite.h>
#include <gvt/render/tracer/Domain/DomainTracer.h>
#include <gvt/render/tracer/Image/ImageTracer.h>


#ifdef GVT_RENDER_ADAPTER_EMBREE
#include <gvt/render/adapter/embree/Wrapper.h>
#endif

#ifdef GVT_RENDER_ADAPTER_MANTA
#include <gvt/render/adapter/manta/Wrapper.h>
#endif

#ifdef GVT_RENDER_ADAPTER_OPTIX
#include <gvt/render/adapter/optix/Wrapper.h>
#endif

#ifdef GVT_USE_MPE
#include "mpe.h"
#endif
#include <gvt/core/data/primitives/BBox.h>
#include <gvt/render/algorithm/Tracers.h>
#include <gvt/render/data/Primitives.h>
#include <gvt/render/data/scene/Image.h>
#include <gvt/render/data/scene/gvtCamera.h>

#include <boost/range/algorithm.hpp>

#include <cstdint>
#include <glob.h>
#include <iostream>
#include <math.h>
#include <ply.h>
#include <stdio.h>
#include <sys/stat.h>

#include "ParseCommandLine.h"

using namespace std;
using namespace gvt::render;

using namespace gvt::core::mpi;
using namespace gvt::render::data::scene;
using namespace gvt::render::schedule;
using namespace gvt::render::data::primitives;

void Rotate(glm::vec3& point, glm::vec3 center, const float angle, const glm::vec3 axis) {
  glm::vec3 p = center - point;
  glm::vec3 t = angle * axis;

  glm::mat4 mAA = glm::rotate(glm::mat4(1.f), t[0], glm::vec3(1, 0, 0)) *
                  glm::rotate(glm::mat4(1.f), t[1], glm::vec3(0, 1, 0)) *
                  glm::rotate(glm::mat4(1.f), t[2], glm::vec3(0, 0, 1));


  point = center + glm::vec3(mAA * glm::vec4(-p, 0.f));
}

// determine if file is a directory
bool isdir(const char *path) {
  struct stat buf;
  stat(path, &buf);
  return S_ISDIR(buf.st_mode);
}
// determine if a file exists
bool file_exists(const char *path) {
  struct stat buf;
  return (stat(path, &buf) == 0);
}
std::vector<std::string> findply(const std::string dirname) {
  glob_t result;
  std::string exp = dirname + "/*.ply";
  glob(exp.c_str(), GLOB_TILDE, NULL, &result);
  std::vector<std::string> ret;
  for (int i = 0; i < result.gl_pathc; i++) {
    ret.push_back(std::string(result.gl_pathv[i]));
  }
  globfree(&result);
  return ret;
}
void test_bvh(gvtPerspectiveCamera &camera);
typedef struct Vertex {
  float x, y, z;
  float nx, ny, nz;
  void *other_props; /* other properties */
} Vertex;

typedef struct Face {
  unsigned char nverts; /* number of vertex indices in list */
  int *verts;           /* vertex index list */
  void *other_props;    /* other properties */
} Face;

PlyProperty vert_props[] = {
  /* list of property information for a vertex */
  { "x", Float32, Float32, offsetof(Vertex, x), 0, 0, 0, 0 },
  { "y", Float32, Float32, offsetof(Vertex, y), 0, 0, 0, 0 },
  { "z", Float32, Float32, offsetof(Vertex, z), 0, 0, 0, 0 },
  { "nx", Float32, Float32, offsetof(Vertex, nx), 0, 0, 0, 0 },
  { "ny", Float32, Float32, offsetof(Vertex, ny), 0, 0, 0, 0 },
  { "nz", Float32, Float32, offsetof(Vertex, nz), 0, 0, 0, 0 },
};

PlyProperty face_props[] = {
  /* list of property information for a face */
  { "vertex_indices", Int32, Int32, offsetof(Face, verts), 1, Uint8, Uint8,
    offsetof(Face, nverts) },
};
static Vertex **vlist;
static Face **flist;

// Used for testing purposes where it specifies the number of ply blocks read by each mpi
//#define DOMAIN_PER_NODE 2

bool setContext(int argc, char **argv) {

  ParseCommandLine cmd("gvtPly");

  cmd.addoption("wsize", ParseCommandLine::INT, "Window size", 2);
  cmd.addoption("eye", ParseCommandLine::FLOAT, "Camera position", 3);
  cmd.addoption("look", ParseCommandLine::FLOAT, "Camera look at", 3);
  cmd.addoption("file", ParseCommandLine::PATH | ParseCommandLine::REQUIRED, "File path");
  cmd.addoption("image", ParseCommandLine::NONE, "Use embeded scene", 0);
  cmd.addoption("domain", ParseCommandLine::NONE, "Use embeded scene", 0);
  cmd.addoption("threads", ParseCommandLine::INT,
                "Number of threads to use (default number cores + ht)", 1);

  cmd.addconflict("image", "domain");

  cmd.parse(argc, argv);

  if (!cmd.isSet("threads")) {
    tbb::task_scheduler_init init(std::thread::hardware_concurrency());
  } else {
    tbb::task_scheduler_init init(cmd.get<int>("threads"));
  }

  // mess I use to open and read the ply file with the c utils I found.
  PlyFile *in_ply;
  Vertex *vert;
  Face *face;
  int elem_count, nfaces, nverts;
  int i, j, k;
  float xmin, ymin, zmin, xmax, ymax, zmax;
  char *elem_name;
  ;
  FILE *myfile;
  char txt[16];
  std::string temp;
  std::string filename, filepath, rootdir;
  rootdir = cmd.get<std::string>("file");
  // rootdir = "/work/01197/semeraro/maverick/DAVEDATA/EnzoPlyData/";
  // filename = "/work/01197/semeraro/maverick/DAVEDATA/EnzoPlyData/block0.ply";
  // myfile = fopen(filename.c_str(),"r");

  gvt::render::RenderContext *cntxt = gvt::render::RenderContext::instance();
  if (cntxt == NULL) {
    std::cout << "Something went wrong initializing the context" << std::endl;
    exit(0);
  }
  gvt::core::DBNodeH root = cntxt->getRootNode();

  // A single mpi node will create db nodes and then broadcast them
  if (MPI::COMM_WORLD.Get_rank() == 0) {
    cntxt->addToSync(cntxt->createNodeFromType("Data", "Data", root.UUID()));
    cntxt->addToSync(cntxt->createNodeFromType("Instances", "Instances", root.UUID()));
  }

  cntxt->syncContext();

  gvt::core::DBNodeH dataNodes = root["Data"];
  gvt::core::DBNodeH instNodes = root["Instances"];

  // Enzo isosurface...
  if (!file_exists(rootdir.c_str())) {
    cout << "File \"" << rootdir << "\" does not exist. Exiting." << endl;
    return 0;
  }

  if (!isdir(rootdir.c_str())) {
    cout << "File \"" << rootdir << "\" is not a directory. Exiting." << endl;
    return 0;
  }
  vector<string> files = findply(rootdir);
  if (files.empty()) {
    cout << "Directory \"" << rootdir << "\" contains no .ply files. Exiting." << endl;
    return 0;
  }
  // read 'em
  vector<string>::const_iterator file;
  // for (k = 0; k < 8; k++) {
  for (file = files.begin(), k = 0; file != files.end(); file++, k++) {
// if defined, ply blocks load are divided across available mpi ranks
// Each block will be loaded by a single mpi rank and a mpi rank can read multiple blocks
#ifdef DOMAIN_PER_NODE
    if (!((k >= MPI::COMM_WORLD.Get_rank() * DOMAIN_PER_NODE) &&
          (k < MPI::COMM_WORLD.Get_rank() * DOMAIN_PER_NODE + DOMAIN_PER_NODE)))
      continue;
#endif

// if all ranks read all ply blocks, one has to create the db node which is then
// broadcasted.
// if not, since each block will be loaded by only one mpi, this mpi rank will create the
// db node
#ifndef DOMAIN_PER_NODE
    if (MPI::COMM_WORLD.Get_rank() == 0)
#endif
      gvt::core::DBNodeH PlyMeshNode =
          cntxt->addToSync(cntxt->createNodeFromType("Mesh", *file, dataNodes.UUID()));

#ifndef DOMAIN_PER_NODE
    cntxt->syncContext();
    gvt::core::DBNodeH PlyMeshNode = dataNodes.getChildren()[k];
#endif
    filepath = *file;
    myfile = fopen(filepath.c_str(), "r");
    in_ply = read_ply(myfile);
    for (i = 0; i < in_ply->num_elem_types; i++) {
      elem_name = setup_element_read_ply(in_ply, i, &elem_count);
      temp = elem_name;
      if (temp == "vertex") {
        vlist = (Vertex **)malloc(sizeof(Vertex *) * elem_count);
        nverts = elem_count;
        setup_property_ply(in_ply, &vert_props[0]);
        setup_property_ply(in_ply, &vert_props[1]);
        setup_property_ply(in_ply, &vert_props[2]);
        for (j = 0; j < elem_count; j++) {
          vlist[j] = (Vertex *)malloc(sizeof(Vertex));
          get_element_ply(in_ply, (void *)vlist[j]);
        }
      } else if (temp == "face") {
        flist = (Face **)malloc(sizeof(Face *) * elem_count);
        nfaces = elem_count;
        setup_property_ply(in_ply, &face_props[0]);
        for (j = 0; j < elem_count; j++) {
          flist[j] = (Face *)malloc(sizeof(Face));
          get_element_ply(in_ply, (void *)flist[j]);
        }
      }
    }
    close_ply(in_ply);
    // smoosh data into the mesh object
    {
      Material *m = new Material();
      Mesh *mesh = new Mesh(m);
      vert = vlist[0];
      xmin = vert->x;
      ymin = vert->y;
      zmin = vert->z;
      xmax = vert->x;
      ymax = vert->y;
      zmax = vert->z;

      for (i = 0; i < nverts; i++) {
        vert = vlist[i];
        xmin = MIN(vert->x, xmin);
        ymin = MIN(vert->y, ymin);
        zmin = MIN(vert->z, zmin);
        xmax = MAX(vert->x, xmax);
        ymax = MAX(vert->y, ymax);
        zmax = MAX(vert->z, zmax);
        mesh->addVertex(glm::vec3(vert->x, vert->y, vert->z));
      }
      glm::vec3 lower(xmin, ymin, zmin);
      glm::vec3 upper(xmax, ymax, zmax);
      gvt::core::data::primitives::Box3D *meshbbox =
          new gvt::core::data::primitives::Box3D(lower, upper);
      // add faces to mesh
      for (i = 0; i < nfaces; i++) {
        face = flist[i];
        mesh->addFace(face->verts[0] + 1, face->verts[1] + 1, face->verts[2] + 1);
      }
      mesh->generateNormals();
      // add Ply mesh to the database
      // PlyMeshNode["file"] =
      // string("/work/01197/semeraro/maverick/DAVEDATA/EnzoPlyDATA/Block0.ply");
      PlyMeshNode["file"] = string(filepath);
      PlyMeshNode["bbox"] = (unsigned long long)meshbbox;
      PlyMeshNode["ptr"] = (unsigned long long)mesh;

      gvt::core::DBNodeH loc = cntxt->createNode("rank", MPI::COMM_WORLD.Get_rank());
      PlyMeshNode["Locations"] += loc;

      cntxt->addToSync(PlyMeshNode);
    }
  }
  cntxt->syncContext();

  // context has the location information of the domain, so for simplicity only one mpi
  // will create the instances
  if (MPI::COMM_WORLD.Get_rank() == 0) {
    for (file = files.begin(), k = 0; file != files.end(); file++, k++) {

      // add instance
      gvt::core::DBNodeH instnode =
          cntxt->createNodeFromType("Instance", "inst", instNodes.UUID());
      gvt::core::DBNodeH meshNode = dataNodes.getChildren()[k];
      gvt::core::data::primitives::Box3D *mbox =
          (gvt::core::data::primitives::Box3D *)meshNode["bbox"].value().toULongLong();
      instnode["id"] = k;
      instnode["meshRef"] = meshNode.UUID();
      auto m = new glm::mat4(1.f);
      auto minv = new glm::mat4(1.f);
      auto normi = new glm::mat3(1.f);
      instnode["mat"] = (unsigned long long)m;
      *minv = glm::inverse(*m);
      instnode["matInv"] = (unsigned long long)minv;
      *normi = glm::transpose(glm::inverse(glm::mat3(*m)));
      instnode["normi"] = (unsigned long long)normi;
      auto il = glm::vec3((*m) * glm::vec4(mbox->bounds_min, 1.f));
      auto ih = glm::vec3((*m) * glm::vec4(mbox->bounds_max, 1.f));
      gvt::core::data::primitives::Box3D *ibox =
          new gvt::core::data::primitives::Box3D(il, ih);
      instnode["bbox"] = (unsigned long long)ibox;
      instnode["centroid"] = ibox->centroid();

      cntxt->addToSync(instnode);
    }
  }

  cntxt->syncContext();

  // add lights, camera, and film to the database
  gvt::core::DBNodeH lightNodes =
      cntxt->createNodeFromType("Lights", "Lights", root.UUID());
  gvt::core::DBNodeH lightNode =
      cntxt->createNodeFromType("PointLight", "conelight", lightNodes.UUID());
  lightNode["position"] = glm::vec3(512.0, 512.0, 2048.0);
  lightNode["color"] = glm::vec3(1000.0, 1000.0, 1000.0);
  // camera
  gvt::core::DBNodeH camNode =
      cntxt->createNodeFromType("Camera", "conecam", root.UUID());
  camNode["eyePoint"] = glm::vec3(512.0, 512.0, 4096.0);
  camNode["focus"] = glm::vec3(512.0, 512.0, 0.0);
  camNode["upVector"] = glm::vec3(0.0, 1.0, 0.0);
  camNode["fov"] = (float)(25.0 * M_PI / 180.0);
  camNode["rayMaxDepth"] = (int)1;
  camNode["raySamples"] = (int)1;
  // film
  gvt::core::DBNodeH filmNode =
      cntxt->createNodeFromType("Film", "conefilm", root.UUID());
  filmNode["width"] = 1900;
  filmNode["height"] = 1080;
  filmNode["outputPath"] = (std::string) "ply";


  if (cmd.isSet("eye")) {
    std::vector<float> eye = cmd.getValue<float>("eye");
    camNode["eyePoint"] = glm::vec3(eye[0], eye[1], eye[2]);
  }

  if (cmd.isSet("look")) {
    std::vector<float> eye = cmd.getValue<float>("look");
    camNode["focus"] = glm::vec3(eye[0], eye[1], eye[2]);
  }
  if (cmd.isSet("wsize")) {
    std::vector<int> wsize = cmd.getValue<int>("wsize");
    filmNode["width"] = wsize[0];
    filmNode["height"] = wsize[1];
  }

  gvt::core::DBNodeH schedNode =
      cntxt->createNodeFromType("Schedule", "Plysched", root.UUID());
  if (cmd.isSet("domain"))
    schedNode["type"] = gvt::render::scheduler::Domain;
  else
    schedNode["type"] = gvt::render::scheduler::Image;

#ifdef GVT_RENDER_ADAPTER_EMBREE
  int adapterType = gvt::render::adapter::Embree;
#elif GVT_RENDER_ADAPTER_MANTA
  int adapterType = gvt::render::adapter::Manta;
#elif GVT_RENDER_ADAPTER_OPTIX
  int adapterType = gvt::render::adapter::Optix;
#elif
  GVT_DEBUG(DBG_ALWAYS, "ERROR: missing valid adapter");
#endif

  schedNode["adapter"] = adapterType;

  // end db setup
  return true;
}


void setCamera() {
  gvt::render::RenderContext *cntxt = gvt::render::RenderContext::instance();
  gvt::core::DBNodeH camNode = cntxt->getRootNode()["Camera"];
  gvt::core::DBNodeH filmNode = cntxt->getRootNode()["Film"];

  std::shared_ptr<gvt::render::data::scene::gvtPerspectiveCamera> mycamera =
      std::make_shared<gvt::render::data::scene::gvtPerspectiveCamera>();
  glm::vec3 cameraposition = camNode["eyePoint"].value().tovec3();
  glm::vec3 focus = camNode["focus"].value().tovec3();
  float fov = camNode["fov"].value().toFloat();
  glm::vec3 up = camNode["upVector"].value().tovec3();
  int rayMaxDepth = camNode["rayMaxDepth"].value().toInteger();
  int raySamples = camNode["raySamples"].value().toInteger();
  //float jitterWindowSize = camNode["jitterWindowSize"].value().toFloat();

  mycamera->lookAt(cameraposition, focus, up);
  mycamera->setMaxDepth(rayMaxDepth);
  mycamera->setSamples(raySamples);
  //mycamera->setJitterWindowSize(jitterWindowSize);
  mycamera->setFOV(fov);
  mycamera->setFilmsize(filmNode["width"].value().toInteger(),
                        filmNode["height"].value().toInteger());

  cntxt->setCamera(mycamera);
}

void setImage() {
  gvt::render::RenderContext *cntxt = gvt::render::RenderContext::instance();
  gvt::core::DBNodeH filmNode = cntxt->getRootNode()["Film"];
  cntxt->setComposite(std::make_shared<gvt::render::composite::IceTComposite>(
      filmNode["width"].value().toInteger(), filmNode["height"].value().toInteger()));
}

int main(int argc, char *argv[]) {
  gvt::comm::scomm::init(argc, argv);
  std::shared_ptr<gvt::comm::communicator> comm = gvt::comm::communicator::singleton();
  gvt::render::RenderContext *cntxt = gvt::render::RenderContext::instance();

  if (!setContext(argc, argv)) return 0;
  setCamera();
  setImage();

  std::shared_ptr<gvt::tracer::Tracer> tracer =
      std::make_shared<gvt::tracer::DomainTracer>();

  cntxt->setTracer(tracer);
  std::shared_ptr<gvt::render::composite::ImageComposite> composite_buffer =
      cntxt->getComposite<gvt::render::composite::ImageComposite>();

  int nFrames =100;
  for (int ii = 0; ii < nFrames; ii++) {

    gvt::core::time::timer t(true, "Frame timer");
    composite_buffer->reset();
    (*tracer)();

//    glm::vec3 light = cntxt->getRootNode()["Lights"].getChildren()[0]["position"].value().tovec3();
//	Rotate(light,cntxt->getCamera()->getFocalPoint(), ((360*2)/(nFrames-1))*M_PI/180, cntxt->getCamera()->getUpVector());
//	cntxt->getRootNode()["Lights"].getChildren()[0]["position"] = light;
//
//	std::shared_ptr<gvt::render::data::scene::gvtCameraBase> camera = cntxt->getCamera();
//	glm::vec3 cameraposition = cntxt->getRootNode()["Camera"]["eyePoint"].value().tovec3();
//	glm::vec3 focus = cntxt->getRootNode()["Camera"]["focus"].value().tovec3();
//	Rotate(cameraposition, focus, ((360*2)/(nFrames-1))*M_PI/180, cntxt->getCamera()->getUpVector());
//	camera->lookAt(cameraposition, focus, cntxt->getCamera()->getUpVector());
//	cntxt->getRootNode()["Camera"]["eyePoint"] = cameraposition;

  // composite_buffer->write(cntxt->getRootNode()["Film"]["outputPath"].value().toString()+std::to_string(ii));



  }

  composite_buffer->write(cntxt->getRootNode()["Film"]["outputPath"].value().toString());
  comm->terminate();

  return 0;
}



