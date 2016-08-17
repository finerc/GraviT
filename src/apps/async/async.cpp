#include <iostream>

#include <gvt/core/Context.h>
#include <gvt/core/acomm/acommunicator.h>
#include <gvt/core/acomm/message.h>
#include <gvt/render/Context.h>

#include <gvt/render/tracer/Image/ImageTracer.h>

#include <chrono>
#include <thread>

int main(int argc, char *argv[]) {

  std::shared_ptr<gvt::comm::acommunicator> comm =
      gvt::comm::acommunicator::instance(argc, argv);

  gvt::render::RenderContext *cntxt = gvt::render::RenderContext::instance();

  if (comm->id() == 0) {
    std::shared_ptr<gvt::comm::Message> msg =
        std::make_shared<gvt::comm::Message>(sizeof(int));
    int data = 0;
    while (data < 9) {
      msg->setcontent(&data, sizeof(int));
      comm->send(msg, comm->maxid() - 1);
      while (!comm->hasMessages())
        ;
      std::shared_ptr<gvt::comm::Message> msg2 = comm->popMessage();
      data = *(int *)(msg2->msg_ptr()) + 1;
      // std::cout << "Data : " << data << std::endl;
    }
    std::cout << "Out of the loop " << comm->id() << std::endl;
  }

  if (comm->maxid() > 1 && comm->id() == comm->maxid() - 1) {
    std::shared_ptr<gvt::comm::Message> msg =
        std::make_shared<gvt::comm::Message>(sizeof(int));
    int data = 0;
    while (data < 9) {

      while (!comm->hasMessages())
        ;
      std::shared_ptr<gvt::comm::Message> msg2 = comm->popMessage();
      data = *(int *)(msg2->msg_ptr()) + 1;
      msg->setcontent(&data, sizeof(int));
      comm->send(msg, 0);
    }
    std::cout << "Out of the loop " << comm->id() << std::endl;
  }

  std::shared_ptr<gvt::tracer::ImageTracer> tracer =
      std::make_shared<gvt::tracer::ImageTracer>();

  // std::this_thread::sleep_for(std::chrono::seconds(10));
  comm->terminate();

  (*tracer)();
  return 0;
}
