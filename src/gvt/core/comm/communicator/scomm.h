#ifndef MPICOMMLIB_SCOMM
#define MPICOMMLIB_SCOMM

#include <gvt/core/comm/communicator.h>
namespace gvt {
namespace comm {
struct scomm : communicator {
  scomm();
  static void init(int argc = 0, char *argv[] = nullptr);
  virtual void run();
};
}
}

#endif /*MPICOMMLIB*/
