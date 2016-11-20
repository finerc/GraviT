#include "acomm.h"
#include <cassert>
#include <memory>
#include <mpi.h>

#include <iostream>
namespace gvt {
namespace comm {
acomm::acomm() {}

void acomm::init(int argc, char *argv[]) {
  assert(!communicator::_instance);

  std::cout << "Instancing async communicator..." << std::endl << std::flush;

  // To run MPI_THREAD_MULTIPLE in MVAPICH this is required but it causes severe overhead with MPI calls
  //MVAPICH2 http://mailman.cse.ohio-state.edu/pipermail/mvapich-discuss/2012-December/004151.html
  //MV2_ENABLE_AFFINITY=0
  int req = MPI_THREAD_MULTIPLE;
  int prov;
  MPI_Init_thread(&argc, &argv, req, &prov);

  switch (prov) {
  case MPI_THREAD_SINGLE:
    std::cout << "MPI_THREAD_SINGLE" << std::endl;
    assert(communicator::_instance->lastid()==1);
    break;
  case MPI_THREAD_FUNNELED:
    std::cout << "MPI_THREAD_FUNNELED" << std::endl;
    assert(communicator::_instance->lastid()==1);
    break;
  case MPI_THREAD_SERIALIZED:
    std::cout << "MPI_THREAD_SERIALIZED" << std::endl;
    communicator::_MPI_THREAD_SERIALIZED=true;
    break;
  case MPI_THREAD_MULTIPLE:
    std::cout << "MPI_THREAD_MULTIPLE" << std::endl;
    communicator::_MPI_THREAD_SERIALIZED=false;
    break;
  default:
    std::cout << "Upppsssss" << std::endl;
  }

  communicator::_instance = std::make_shared<acomm>();
  communicator::init(argc, argv);
}

void acomm::run() {
  std::cout << id() << " Communicator thread started" << std::endl;
  while (!_terminate) {
    {

      if (!_outbox.empty()) {
        std::lock_guard<std::mutex> l(moutbox);
        std::shared_ptr<Message> msg = _outbox.front();
        _outbox.erase(_outbox.begin());
        aquireComm();
        MPI_Send(msg->getMessage<void>(), msg->buffer_size(), MPI::BYTE, msg->dst(),
                 CONTROL_SYSTEM_TAG, MPI_COMM_WORLD);
        releaseComm();
      }

      MPI_Status status;
      memset(&status,0, sizeof(MPI_Status));
      int flag;
      aquireComm();
      MPI_Iprobe(MPI_ANY_SOURCE, CONTROL_SYSTEM_TAG, MPI_COMM_WORLD, &flag, &status);
      releaseComm();
      int n_bytes=0;
      MPI_Get_count(&status, MPI_BYTE, &n_bytes);

      if (n_bytes > 0) {
        int sender = status.MPI_SOURCE;
        const auto data_size = n_bytes - sizeof(Message::header);
        std::shared_ptr<Message> msg = std::make_shared<Message>(data_size);

//        std::cout << "Recv : " << n_bytes << " on " << id() << " from " << sender
//                  << std::flush << std::endl;
        aquireComm();
        MPI_Recv(msg->getMessage<void>(), n_bytes, MPI_BYTE, sender, CONTROL_SYSTEM_TAG,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        releaseComm();
        msg->size(data_size);
        std::lock_guard<std::mutex> l(minbox);
        if (msg->system_tag() == CONTROL_USER_TAG) _inbox.push_back(msg);
        if (msg->system_tag() == CONTROL_VOTE_TAG) voting->processMessage(msg);
      }
    }
  }
}

void acomm::send(std::shared_ptr<comm::Message> msg, std::size_t to) {
  assert(msg->tag() >= 0 && msg->tag() < registry_names.size());
  const std::string classname = registry_names[msg->tag()];
  assert(registry_ids.find(classname) != registry_ids.end());
  msg->src(id());
  msg->dst(to);
  std::lock_guard<std::mutex> l(moutbox);
  _outbox.push_back(msg);
};

void acomm::broadcast(std::shared_ptr<comm::Message> msg) {
  assert(msg->tag() >= 0 && msg->tag() < registry_names.size());
  const std::string classname = registry_names[msg->tag()];
  assert(registry_ids.find(classname) != registry_ids.end());
  msg->src(id());
  for (int i = 0; i < lastid(); i++) {
    if (i == id()) continue;
    std::shared_ptr<gvt::comm::Message> new_msg =
                    std::make_shared<gvt::comm::Message>(*msg);

    new_msg->dst(i);
    std::lock_guard<std::mutex> l(moutbox);
    _outbox.push_back(new_msg);
  }
};
}
}
