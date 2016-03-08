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
/**
 * A simple GraviT application to do some testing.
 * This is supposed to be as close to the osptest application as I can get it.
 *
 * run like this:
 * bin/gvttest -i $WORK/DAVEDATA/EnzoPlyData -o gvtspoot.ppm -cp 512,512,4096 -cd 0,0,-1 -fp 512,512,0 -fov 25.0 -ld 0,0,-1  -geom 1920x1080 -lp 512,512,2048 -sched image -adapt embree
*/
#include <gvt/render/RenderContext.h>
#include <gvt/render/Types.h>
#include <vector>
#include <algorithm>
#include <set>
#include <gvt/core/mpi/Wrapper.h>
#include <gvt/core/Math.h>
#include <gvt/render/data/Dataset.h>
#include <gvt/render/data/Domains.h>
#include <gvt/render/Schedulers.h>

#include <tbb/task_scheduler_init.h>
#include <thread>

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
#include <gvt/render/algorithm/Tracers.h>
#include <gvt/render/data/scene/gvtCamera.h>
#include <gvt/render/data/scene/Image.h>
#include <gvt/render/data/Primitives.h>

#include <boost/range/algorithm.hpp>

#include <math.h>
#include <stdio.h>
#include "../iostuff.h"
#include "../timer.h"

using namespace std;
using namespace gvt::render;
using namespace gvt::core::math;
using namespace gvt::core::mpi;
using namespace gvt::render::data::scene;
using namespace gvt::render::schedule;
using namespace gvt::render::data::primitives;
static Vertex **vlist;
static Face **flist;

#define MIN(a, b) ((a < b) ? (a) : (b))
#define MAX(a, b) ((a > b) ? (a) : (b))
void findbounds(float *array, int numelements, Point4f *lower, Point4f *upper) {
  float xmin, xmax, ymin, ymax, zmin, zmax;
  xmin = array[0];
  ymin = array[1];
  zmin = array[2];
  xmax = xmin;
  ymax = ymin;
  zmax = zmin;
  for (int i = 0; i < numelements; i++) {
    xmin = MIN(array[3 * i], xmin);
    ymin = MIN(array[3 * i + 1], ymin);
    zmin = MIN(array[3 * i + 2], zmin);
    xmax = MAX(array[3 * i], xmax);
    ymax = MAX(array[3 * i + 1], ymax);
    zmax = MAX(array[3 * i + 2], zmax);
  }
  *lower = Point4f(xmin, ymin, zmin);
  *upper = Point4f(xmax, ymax, zmax);
}

int main(int argc, char **argv) {

  // default values
  int width = 1920;
  int height = 1080;
  int warmupframes = 1;
  int benchmarkframes = 10;
  // timer stuff
  my_timer_t startTime, endTime;
  double rendertime = 0.0;
  double warmupframetime = 0.0;
  double iotime = 0.0;
  double modeltime = 0.0;
  // geometry data
  float *vertexarray;
  float *colorarray;
  int32_t *indexarray;
  int numtriangles = 0;
  int nverts, nfaces;
  // camera and light
  Point4f cam_pos = {0.,0.,0.,0.};
  Point4f cam_focus = {0.,0.,-1.0,0.};
  float cam_fovy = (float)(25.0 * M_PI / 180.0);
  Vector4f cam_up = {0.,1.,0.,1.};
  Vector4f light_pos = cam_pos;
  Vector4f light_color = {1.,1.,1.,1.};
  // file related things
  string filepath("");
  string filename("");
  string outputfile("");
  // gravit behavior
  string scheduletype("image");
  string adapter("embree");
  // initialize gravit context database structure
  timeCurrent(&startTime);
  gvt::render::RenderContext *cntxt = gvt::render::RenderContext::instance();
  if (cntxt == NULL) {
    std::cout << "Something went wrong initializing the context" << std::endl;
    exit(0);
  }
  gvt::core::DBNodeH root = cntxt->getRootNode();
  gvt::core::DBNodeH dataNodes = cntxt->createNodeFromType("Data", "Data", root.UUID());
  gvt::core::DBNodeH instNodes = cntxt->createNodeFromType("Instances", "Instances", root.UUID());
  gvt::core::DBNodeH lightNodes = cntxt->createNodeFromType("Lights", "Lights", root.UUID());
  gvt::core::DBNodeH lightNode = cntxt->createNodeFromType("PointLight", "conelight", lightNodes.UUID());
  gvt::core::DBNodeH camNode = cntxt->createNodeFromType("Camera", "conecam", root.UUID());
  gvt::core::DBNodeH filmNode = cntxt->createNodeFromType("Film", "conefilm", root.UUID());
  gvt::core::DBNodeH schedNode = cntxt->createNodeFromType("Schedule", "Enzosched", root.UUID());
  timeCurrent(&endTime);
  modeltime += timeDifferenceMS(&startTime,&endTime);

  // parse the command line
  if ((argc < 2)) 
  {
    // no input so render the defalut empty image
  } 
  else 
  {
    // parse the input
    for (int i = 1; i < argc; i++) 
    {
      const string arg = argv[i];
      if (arg == "-i") 
      {
        filepath = argv[++i];
        if (!file_exists(filepath.c_str())) 
        {
          cout << "File \"" << filepath << "\" does not exist. Exiting." << endl;
          return 0;
        } 
        else if (isdir(filepath.c_str())) 
        {
          vector<string> files = findply(filepath);
          if (!files.empty()) // directory contains .ply files
          {
            vector<string>::const_iterator file;
            int k;
            char txt[16];
            for (file = files.begin(), k = 0; file != files.end(); file++, k++) 
            {
              timeCurrent(&startTime);
              ReadPlyData(*file, vertexarray, colorarray, indexarray, nverts, nfaces);
              timeCurrent(&endTime);
              iotime += timeDifferenceMS(&startTime, &endTime);
              std::cout << k << " file " << timeDifferenceMS(&startTime, &endTime)<< std::endl;

              timeCurrent(&startTime);
              sprintf(txt, "%d", k);
              filename = "block";
              filename += txt;
              gvt::core::DBNodeH EnzoMeshNode = cntxt->createNodeFromType("Mesh", filename.c_str(), dataNodes.UUID());
              Mesh *mesh = new Mesh(new Lambert(Vector4f(0.5, 0.5, 1.0, 1.0)));
              for (int i = 0; i < nverts; i++) 
              {
                mesh->addVertex(Point4f(vertexarray[3 * i], vertexarray[3 * i + 1], vertexarray[3 * i + 2], 1.0));
              }
              for (int i = 0; i < nfaces; i++) // Add faces to mesh
              {
                mesh->addFace(indexarray[3 * i] + 1, indexarray[3 * i + 1] + 1, indexarray[3 * i + 2] + 1);
              }
              Point4f lower;
              Point4f upper;
              findbounds(vertexarray, nverts, &lower, &upper);
              Box3D *meshbbox = new gvt::render::data::primitives::Box3D(lower, upper);
              mesh->generateNormals();
              EnzoMeshNode["file"] = string(filename);
              EnzoMeshNode["bbox"] = (unsigned long long)meshbbox;
              EnzoMeshNode["ptr"] = (unsigned long long)mesh;
              // add instance
              gvt::core::DBNodeH instnode = cntxt->createNodeFromType("Instance", "inst", instNodes.UUID());
              gvt::core::DBNodeH meshNode = EnzoMeshNode;
              Box3D *mbox = (Box3D *)meshNode["bbox"].value().toULongLong();
              instnode["id"] = k;
              instnode["meshRef"] = meshNode.UUID();
              auto m = new gvt::core::math::AffineTransformMatrix<float>(true);
              auto minv = new gvt::core::math::AffineTransformMatrix<float>(true);
              auto normi = new gvt::core::math::Matrix3f();
              instnode["mat"] = (unsigned long long)m;
              *minv = m->inverse();
              instnode["matInv"] = (unsigned long long)minv;
              *normi = m->upper33().inverse().transpose();
              instnode["normi"] = (unsigned long long)normi;
              auto il = (*m) * mbox->bounds[0];
              auto ih = (*m) * mbox->bounds[1];
              Box3D *ibox = new gvt::render::data::primitives::Box3D(il, ih);
              instnode["bbox"] = (unsigned long long)ibox;
              instnode["centroid"] = ibox->centroid();
              timeCurrent(&endTime);
              modeltime += timeDifferenceMS(&startTime, &endTime);
              numtriangles += nfaces;
            }
          } 
          else // directory has no .ply files
          {
            filepath = "";
          } 
        } 
        else // filepath is not a directory but a .ply file
        {
          timeCurrent(&startTime);
          ReadPlyData(filepath, vertexarray, colorarray, indexarray, nverts, nfaces);
          timeCurrent(&endTime);
          iotime += timeDifferenceMS(&startTime, &endTime);
          timeCurrent(&startTime);
          gvt::core::DBNodeH EnzoMeshNode = cntxt->createNodeFromType("Mesh", filepath.c_str(), dataNodes.UUID());
          Mesh *mesh = new Mesh(new Lambert(Vector4f(0.5, 0.5, 1.0, 1.0)));
          for (int i = 0; i < nverts; i++) {
            mesh->addVertex(Point4f(vertexarray[3 * i], vertexarray[3 * i + 1], vertexarray[3 * i + 2], 1.0));
          }
          for (int i = 0; i < nfaces; i++) // Add faces to mesh
          {
            mesh->addFace(indexarray[3 * i] +1, indexarray[3 * i + 1]+1 , indexarray[3 * i + 2]+1 );
          }
          Point4f lower;
          Point4f upper;
          findbounds(vertexarray, nverts, &lower, &upper);
          Box3D *meshbbox = new gvt::render::data::primitives::Box3D(lower, upper);
          mesh->generateNormals();
          EnzoMeshNode["file"] = string(filepath);
          EnzoMeshNode["bbox"] = (unsigned long long)meshbbox;
          EnzoMeshNode["ptr"] = (unsigned long long)mesh;
          // add instance
          gvt::core::DBNodeH instnode = cntxt->createNodeFromType("Instance", "inst", instNodes.UUID());
          gvt::core::DBNodeH meshNode = EnzoMeshNode;
          Box3D *mbox = (Box3D *)meshNode["bbox"].value().toULongLong();
          instnode["id"] = 0;
          instnode["meshRef"] = meshNode.UUID();
          auto m = new gvt::core::math::AffineTransformMatrix<float>(true);
          auto minv = new gvt::core::math::AffineTransformMatrix<float>(true);
          auto normi = new gvt::core::math::Matrix3f();
          instnode["mat"] = (unsigned long long)m;
          *minv = m->inverse();
          instnode["matInv"] = (unsigned long long)minv;
          *normi = m->upper33().inverse().transpose();
          instnode["normi"] = (unsigned long long)normi;
          auto il = (*m) * mbox->bounds[0];
          auto ih = (*m) * mbox->bounds[1];
          Box3D *ibox = new gvt::render::data::primitives::Box3D(il, ih);
          instnode["bbox"] = (unsigned long long)ibox;
          instnode["centroid"] = ibox->centroid();
          timeCurrent(&endTime);
          modeltime += timeDifferenceMS(&startTime, &endTime);
          numtriangles += nfaces;
        }
      } 
      else if (arg == "-bench") 
      {
        if (++i < argc) 
        {
          std::string arg2(argv[i]);
          size_t pos = arg2.find("x");
          if (pos != std::string::npos) 
          {
            arg2.replace(pos, 1, " ");
            std::stringstream ss(arg2);
            ss >> warmupframes >> benchmarkframes;
          }
        }
      } 
      else if (arg == "-geom") 
      {
        if (++i < argc) 
        {
          std::string arg2(argv[i]);
          size_t pos = arg2.find("x");
          if (pos != std::string::npos) 
          {
            arg2.replace(pos, 1, " ");
            std::stringstream ss(arg2);
            ss >> width >> height;
          }
        }
      } 
      else if (arg == "-o") 
      {
        outputfile = argv[++i];
      }
      else if (arg == "-adapt")
      {
        adapter = argv[++i];
      }
      else if (arg == "-sched")
      {
        scheduletype = argv[++i];
      }
      else if (arg == "-fov")
      {
        cam_fovy = (float)(atof(argv[++i]) * M_PI/180.0);
      }
      else if (arg == "-cp") // camera location
      {
        if (++i < argc)
        {
          std::string arg2(argv[i]);
          size_t pos = arg2.find(",");
          if(pos!=std::string::npos)
          {
            arg2.replace(pos,1," ");
          }
          pos = arg2.find(",");
          if(pos!=std::string::npos)
          {
            arg2.replace(pos,1," ");
          }
          float camx,camy,camz;
          std::stringstream ss(arg2);
          ss >> camx >> camy >> camz;
          cam_pos = {camx,camy,camz,0.0};
        }
      }
      else if (arg=="-fp") // camera focus point
      {
        if (++i < argc)
        {
          std::string arg2(argv[i]);
          size_t pos = arg2.find(",");
          if(pos!=std::string::npos)
          {
            arg2.replace(pos,1," ");
          }
          pos = arg2.find(",");
          if(pos!=std::string::npos)
          {
            arg2.replace(pos,1," ");
          }
          float camfx,camfy,camfz;
          std::stringstream ss(arg2);
          ss >> camfx >> camfy >> camfz;
          cam_focus = {camfx,camfy,camfz,0.0};
        }
      }
      else if (arg=="-cu") // camera up vector
      {
        if (++i < argc)
        {
          std::string arg2(argv[i]);
          size_t pos = arg2.find(",");
          if(pos!=std::string::npos)
          {
            arg2.replace(pos,1," ");
          }
          pos = arg2.find(",");
          if(pos!=std::string::npos)
          {
            arg2.replace(pos,1," ");
          }
          float cux,cuy,cuz;
          std::stringstream ss(arg2);
          ss >> cux >> cuy >> cuz;
          cam_up = {cux,cuy,cuz,1.0};
        }
      }
      else if (arg=="-lp") // light position
      {
        if (++i < argc)
        {
          std::string arg2(argv[i]);
          size_t pos = arg2.find(",");
          if(pos!=std::string::npos)
          {
            arg2.replace(pos,1," ");
          }
          pos = arg2.find(",");
          if(pos!=std::string::npos)
          {
            arg2.replace(pos,1," ");
          }
          float lpx,lpy,lpz;
          std::stringstream ss(arg2);
          ss >> lpx >> lpy >> lpz;
          light_pos = {lpx,lpy,lpz,0.0};
        }
      }
    }
  }
  tbb::task_scheduler_init init(std::thread::hardware_concurrency());
#if 1
  MPI_Init(&argc, &argv);
  MPI_Pcontrol(0);
  int rank = -1;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  timeCurrent(&startTime);
  // add lights, camera, and film to the database
  lightNode["position"] = light_pos;
  lightNode["color"] = light_color;
  // camera
  camNode["eyePoint"] = cam_pos;
  camNode["focus"] = cam_focus;
  camNode["upVector"] = cam_up;
  camNode["fov"] = cam_fovy;
  // film
  filmNode["width"] = width;
  filmNode["height"] = height;
  // schedule
  if(scheduletype.compare("image") == 0)
  {
    schedNode["type"] = gvt::render::scheduler::Image;
  }
  else if(scheduletype.compare("domain") == 0)
  {
    schedNode["type"] = gvt::render::scheduler::Domain;
  }
  // adapter
  if(adapter.compare("embree") == 0)
  {
    std::cout << " embree adapter " << std::endl;
#ifdef GVT_RENDER_ADAPTER_EMBREE
    schedNode["adapter"] = gvt::render::adapter::Embree;
#else
    std::cout << "Embree adapter missing. recompile" << std::endl;
    exit(1);
#endif
  }
  else if(adapter.compare("manta") == 0)
  {
    std::cout << " manta adapter " << std::endl;
#ifdef GVT_RENDER_ADAPTER_MANTA
    schedNode["adapter"] = gvt::render::adapter::Manta;
#else
    std::cout << "Manta adapter missing. recompile" << std::endl;
    exit(1);
#endif
  }
  else if(adapter.compare("optix") == 0)
  {
    std::cout << " optix adapter " << std::endl;
#ifdef GVT_RENDER_ADAPTER_OPTIX
    schedNode["adapter"] = gvt::render::adapter::Optix;
#else
    std::cout << "Optix adapter missing. recompile" << std::endl;
    exit(1);
#endif
  }
  else
  {
    std::cout << "unknown adapter, " << adapter << ", specified." << std::endl;
    exit(1);
  }
  // add empty mesh in case of no file path. Render empty image.
  if(filepath.empty()) {
    gvt::core::DBNodeH EnzoMeshNode = cntxt->createNodeFromType("Mesh", filepath.c_str(), dataNodes.UUID());
    Mesh *mesh = new Mesh(new Lambert(Vector4f(0.5, 0.5, 1.0, 1.0)));
    Point4f lower = {0.,0.,0.,0.};
    Point4f upper = {1.,1.,1.,0.};
    //findbounds(vertexarray, nverts, &lower, &upper);
    Box3D *meshbbox = new gvt::render::data::primitives::Box3D(lower, upper);
    mesh->generateNormals();
    EnzoMeshNode["file"] = string(filepath);
    EnzoMeshNode["bbox"] = (unsigned long long)meshbbox;
    EnzoMeshNode["ptr"] = (unsigned long long)mesh;
    // add instance
    gvt::core::DBNodeH instnode = cntxt->createNodeFromType("Instance", "inst", instNodes.UUID());
    gvt::core::DBNodeH meshNode = EnzoMeshNode;
    Box3D *mbox = (Box3D *)meshNode["bbox"].value().toULongLong();
    instnode["id"] = 0;
    instnode["meshRef"] = meshNode.UUID();
    auto m = new gvt::core::math::AffineTransformMatrix<float>(true);
    auto minv = new gvt::core::math::AffineTransformMatrix<float>(true);
    auto normi = new gvt::core::math::Matrix3f();
    instnode["mat"] = (unsigned long long)m;
    *minv = m->inverse();
    instnode["matInv"] = (unsigned long long)minv;
    *normi = m->upper33().inverse().transpose();
    instnode["normi"] = (unsigned long long)normi;
    auto il = (*m) * mbox->bounds[0];
    auto ih = (*m) * mbox->bounds[1];
    Box3D *ibox = new gvt::render::data::primitives::Box3D(il, ih);
    instnode["bbox"] = (unsigned long long)ibox;
    instnode["centroid"] = ibox->centroid();
    numtriangles += nfaces;
  }

  // setup gvtCamera from database entries
  gvtPerspectiveCamera mycamera;
  Point4f cameraposition = camNode["eyePoint"].value().toPoint4f();
  Point4f focus = camNode["focus"].value().toPoint4f();
  float fov = camNode["fov"].value().toFloat();
  Vector4f up = camNode["upVector"].value().toVector4f();
  mycamera.lookAt(cameraposition, focus, up);
  mycamera.setFOV(fov);
  mycamera.setFilmsize(filmNode["width"].value().toInteger(), filmNode["height"].value().toInteger());

  // setup image from database sizes
  Image myimage(mycamera.getFilmSizeWidth(), mycamera.getFilmSizeHeight(), outputfile);
  // if empty file path add empty mesh and other data.

  timeCurrent(&endTime);
  modeltime += timeDifferenceMS(&startTime, &endTime);
  int schedType = root["Schedule"]["type"].value().toInteger();

  switch (schedType) {
  case gvt::render::scheduler::Image: {
    timeCurrent(&startTime);
    gvt::render::algorithm::Tracer<ImageScheduler> tracer(mycamera.rays, myimage);
    for (int z = 0; z < warmupframes; z++) {
      mycamera.AllocateCameraRays();
      mycamera.generateRays();
      tracer();
    }
    timeCurrent(&endTime);
    warmupframetime = timeDifferenceMS(&startTime,&endTime);
    timeCurrent(&startTime);
    for (int z = 0; z < benchmarkframes; z++) {
      mycamera.AllocateCameraRays();
      mycamera.generateRays();
      myimage.clear();
      tracer();
      // gvt::render::algorithm::Tracer<ImageScheduler>(mycamera.rays, myimage)();
    }
    timeCurrent(&endTime);
    rendertime += timeDifferenceMS(&startTime, &endTime);
    break;
  }
  case gvt::render::scheduler::Domain: {
    timeCurrent(&startTime);
    gvt::render::algorithm::Tracer<DomainScheduler> tracer(mycamera.rays, myimage);
    //gvt::render::algorithm::Tracer<DomainScheduler>(mycamera.rays, myimage)();
    for (int z = 0; z < warmupframes; z++) {
      mycamera.AllocateCameraRays();
      mycamera.generateRays();
      tracer();
    }
    timeCurrent(&endTime);
    warmupframetime = timeDifferenceMS(&startTime,&endTime);
    timeCurrent(&startTime);
    for (int z = 0; z < benchmarkframes; z++) {
      mycamera.AllocateCameraRays();
      mycamera.generateRays();
      myimage.clear();
      tracer();
    }
    timeCurrent(&endTime);
    rendertime += timeDifferenceMS(&startTime, &endTime);
    break;
  }
  default: {
    std::cout << "unknown schedule type provided: " << schedType << std::endl;
    break;
  }
  }

  float millionsoftriangles = numtriangles / 1000000;
  float millisecondsperframe = rendertime / benchmarkframes;
  float framespersecond = (1000 * benchmarkframes) / rendertime;
  myimage.Write();
  std::cout << scheduletype << "," << width << "," << height << "," << warmupframes << ",";
  std::cout << benchmarkframes << "," << iotime << "," << modeltime << ",";
  std::cout << warmupframetime << "," << millisecondsperframe << "," << framespersecond << std::endl;
#ifdef GVT_USE_MPI
  if (MPI::COMM_WORLD.Get_size() > 1) MPI_Finalize();
#endif
}
