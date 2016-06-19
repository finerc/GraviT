#ifndef APPS_RENDER_MPI_APP_H
#define APPS_RENDER_MPI_APP_H

#include <string>

namespace apps {
namespace render {
namespace mpi {
namespace commandline {

struct Options {
  enum TracerType {
    ASYNC_DOMAIN = 0,
    ASYNC_IMAGE,
    SYNC_DOMAIN,
    SYNC_IMAGE,
    PING_TEST,
    NUM_TRACERS
  };

  enum AdapterType { EMBREE, MANTA, OPTIX };

  int tracer = ASYNC_DOMAIN;
  int adapter = EMBREE;
  int width = 1920;
  int height = 1080;
  bool obj = false;
  int instanceCountX = 1;
  int instanceCountY = 1;
  int instanceCountZ = 1;
  int numFrames = 1;
  int numTbbThreads;
  int ply_count = 8;
  std::string infile;
  std::string model_name = std::string("model");
  // glm::vec3 eye;
  // glm::vec3 look;
};

}  // namespace commandline
}  // namespace mpi
}  // namespace render
}  // namespace apps

#endif