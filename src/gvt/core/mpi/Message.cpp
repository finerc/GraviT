#include <mpi.h>
#include <pthread.h>
#include <unistd.h>
#include <iostream>
#include "Application.h"
#include "gvt/core/mpi/Message.h"
#include "gvt/core/mpi/MessageQ.h"
#include <string>
#include <fstream>
#include <strstream>

#define DEBUG_DISABLE_MSGTHREAD 0
#define DEBUG_DISABLE_ISREADY 0

using namespace gvt::core::mpi;

// #define DEBUG_MSG_THREAD

// #define LOGGING
// #define COUNTING 1
#ifdef COUNTING
static pthread_mutex_t ctor_lock = PTHREAD_MUTEX_INITIALIZER;
static int mknt = 0;
static int mdel = 0;
#endif

#ifdef LOGGING
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

void log(std::string s) {
  pthread_mutex_lock(&log_lock);
  std::fstream fs;
  fs.open(std::string("log_") + std::to_string(Application::GetApplication()->GetMessageManager()->GetRank()),
          std::fstream::in | std::fstream::out | std::fstream::app);
  fs << s;
  fs.close();
  pthread_mutex_unlock(&log_lock);
}
#endif

void *MessageManager::messageThread(void *p) {
  MessageManager *theMessageManager = (MessageManager *)p;
  Application *theApplication = Application::GetApplication();

  MPI_Init(theApplication->GetPArgC(), theApplication->GetPArgV());
  MPI_Comm_rank(MPI_COMM_WORLD, &theMessageManager->mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &theMessageManager->mpi_size);

  pthread_mutex_lock(&theMessageManager->lock);

  theMessageManager->wait = 1;

  // bds put signal here to signal the main thread.
  pthread_cond_signal(&theMessageManager->cond);
  while (theMessageManager->wait == 1) pthread_cond_wait(&theMessageManager->cond, &theMessageManager->lock);

  theMessageManager->wait = 3;
  pthread_cond_signal(&theMessageManager->cond);
  pthread_mutex_unlock(&theMessageManager->lock);

  Message *m = NULL;

#if DEBUG_DISABLE_MSGTHREAD
  Message *pending_message = new Message();
  while (!theApplication->IsDoneSet()) {
    if (pending_message->IsReady()) {
    }
    // if (theApplication->GetOutgoingMessageQueue()->IsReady()) {
    // }
  }
#else
  Message *pending_message = new Message();
  while (!theApplication->IsDoneSet()) {
    if (pending_message->IsReady()) {
      pending_message->Receive();

#ifdef DEBUG_MSG_THREAD
        printf("Rank %d: MessageManager::messageThread: mesage received\n", theApplication->GetRank());
#endif
      // Handle collective operations in the MPI thread

      if (pending_message->header.collective) {
        Work *w = theApplication->Deserialize(pending_message);
        delete pending_message;
        pending_message = NULL;

        bool kill_me = w->Action();
        delete w;
        if (kill_me) {
          theApplication->Kill();
        }
      } else {
        theApplication->GetIncomingMessageQueue()->Enqueue(pending_message);
      }

      pending_message = new Message();
    }

    // while (theApplication->GetOutgoingMessageQueue()->IsReady()) {
    if (theApplication->GetOutgoingMessageQueue()->IsReady()) {

      bool serve = true;
      if (m) { // wait until previous message
        if (m->IsIsendDone()) {
          delete m;
          m = NULL;
        } else {
          serve = false;
        }
      }

      if (serve) {
        // Message *m = theApplication->GetOutgoingMessageQueue()->Dequeue();
        m = theApplication->GetOutgoingMessageQueue()->Dequeue();
        if (m) {

#ifdef DEBUG_MSG_THREAD
          printf("Rank %d: MessageManager::messageThread: message to send\n", theApplication->GetRank());
#endif
          // Send it on
          m->Send();

#ifdef DEBUG_MSG_THREAD
          printf("Rank %d: MessageManager::messageThread: message sent\n", theApplication->GetRank());
#endif
          // If this is a collective, execute its work here in the message thread
          if (m->header.collective) {
            Work *w = theApplication->Deserialize(m);
            bool kill_me = w->Action();

            if (m->blocking) {
              pthread_mutex_lock(&m->lock);
              pthread_cond_signal(&m->cond);
              pthread_mutex_unlock(&m->lock);
            } else
              delete m;
  
            if (kill_me) {
#ifdef DEBUG_MSG_THREAD
              printf("Rank %d: MessageManager::messageThread kill_me=true theApplication->Kill()\n", theApplication->GetRank());
#endif
              theApplication->Kill();
            }
          }
          // } else
          //   delete m;
        } else {
#ifdef DEBUG_MSG_THREAD
          printf("Rank %d: NULL message detected for send\n", theApplication->GetRank());
          // exit(1);
#endif
          break;
        }
      }
    }
  }
#endif // DEBUG_DISABLE_MSGTHREAD

#ifdef DEBUG_MSG_THREAD
  printf("Rank %d: MessageManager::messageThread: just got out of forever message loop. theApplication->IsDoneSet(): %d\n", theApplication->GetRank(), theApplication->IsDoneSet());
#endif

  if (pending_message) delete pending_message;

  // Make sure outgoing queue is flushed to make sure any
  // quit message is propagated

  Message *message;
  while ((message = theApplication->GetOutgoingMessageQueue()->Dequeue()) != NULL) {
#ifdef DEBUG_MSG_THREAD
    printf("Rank %d: MessageManager::messageThread: flushing outgoing message queue\n", theApplication->GetRank());
#endif
    message->Send();
    delete message;
  }

#ifdef DEBUG_MSG_THREAD
  printf("Rank %d: MessageManager::messageThread: flushed outgoing message queue\n", theApplication->GetRank());
#endif

  MPI_Barrier(MPI_COMM_WORLD);

#ifdef DEBUG_MSG_THREAD
  printf("Rank %d: MessageManager::messageThread: passed barrier. calling mpi finalize\n", theApplication->GetRank());
#endif
  
  MPI_Finalize();


#ifdef COUNTING
  sleep(theMessageManager->mpi_rank);
  std::cerr << mknt << " created, " << mdel << " deleted\n";
#endif

  pthread_exit(NULL);
}

MessageManager::MessageManager() {
  pthread_mutex_init(&lock, NULL);
  pthread_cond_init(&cond, NULL);
  pthread_mutex_lock(&lock);
}

MessageManager::~MessageManager() { pthread_mutex_unlock(&lock); }

void MessageManager::Wait() { pthread_join(tid, NULL); }

void MessageManager::Initialize() {
  // create messageThread and wait for it to tell you it started.
  if (pthread_create(&tid, NULL, messageThread, this)) {
    std::cerr << "Failed to spawn MPI thread\n";
    exit(1);
  }

  wait = 0;
  while (wait == 0) pthread_cond_wait(&cond, &lock);
}

void MessageManager::Start() {
  wait = 2;
  pthread_cond_signal(&cond);
  while (wait == 2) pthread_cond_wait(&cond, &lock);
}

Message::Message(Work *w, bool collective, bool b) {
#ifdef COUNTING
  pthread_mutex_lock(&ctor_lock);
  id = mknt++;
  pthread_mutex_unlock(&ctor_lock);
#endif

  header.type = w->GetType();
  pending = 0;

  blocking = b;
  if (blocking) {
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);
    pthread_mutex_lock(&lock);
  }

  header.collective = collective;
  header.broadcast_root = Application::GetApplication()->GetRank();
  header.destination = -1;

  w->Serialize(header.size, serialized);

#ifdef LOGGING
  std::strstream s;
  s << id << (header.collective ? "+c\n" : "+n\n");
  log(s.str());
#endif
}

Message::Message(Work *w, int destination) {
#ifdef COUNTING
  pthread_mutex_lock(&ctor_lock);
  id = mknt++;
  pthread_mutex_unlock(&ctor_lock);
#endif

  header.type = w->GetType();
  pending = 0;

  blocking = false;

  header.broadcast_root = -1;
  header.collective = false;
  header.destination = destination;

  w->Serialize(header.size, serialized);

#ifdef LOGGING
  std::strstream s;
  s << id << "+n\n";
  log(s.str());
#endif
}

Message::Message() {
#ifdef COUNTING
  pthread_mutex_lock(&ctor_lock);
  id = mknt++;
  pthread_mutex_unlock(&ctor_lock);
#endif

  serialized = NULL;
  pending = 1;

#ifdef LOGGING
  std::strstream s;
  s << "pending...  " << id << "\n";
  log(s.str());
#endif

  MPI_Irecv((unsigned char *)&header, sizeof(header), MPI_UNSIGNED_CHAR, MPI_ANY_SOURCE, Message::HEADER_TAG,
            MPI_COMM_WORLD, &request);
}

Message::~Message() {
#ifdef COUNTING
  pthread_mutex_lock(&ctor_lock);
  mdel++;
  pthread_mutex_unlock(&ctor_lock);
#endif

  if (pending) {
#ifdef LOGGING
    std::strstream s;
    s << "cancelling " << id << (header.collective ? "+c\n" : "+n\n");
    log(s.str());
#endif
    MPI_Cancel(&request);
  }

#ifdef LOGGING
  std::strstream s;
  s << id << (header.collective ? "-c\n" : "-n\n");
  log(s.str());
#endif

  if (serialized) free(serialized);
}

void Message::Wait() {
  if (!blocking)
    std::cerr << "Error... waiting on non-blocking message?\n";
  else
    pthread_cond_wait(&cond, &lock);
}

bool Message::IsReady() {
#if DEBUG_DISABLE_ISREADY
  return false;
#else
// #ifdef DEBUG_MSG_THREAD
//   printf("Rank %d: Message::IsReady\n", Application::GetApplication()->GetRank());
// #endif
  int read_ready;
  int err = MPI_Test(&request, &read_ready, &status);
#ifdef DEBUG_MSG_THREAD
  // printf("MPI_Test(err): %d\n", err);
  if (read_ready != 0)
    printf("Rank %d: Message::IsReady: incoming message available: read_ready %d status.MPI_SOURCE: %d\n", Application::GetApplication()->GetRank(), read_ready, status.MPI_SOURCE);
#endif
  return read_ready != 0;
#endif
}

bool Message::IsIsendDone() {
  if (header.broadcast_root != -1)
    return true;
  int write_header_done;
  int write_body_done;
  MPI_Test(&isend_header_request, &write_header_done, &isend_header_status);
  bool done = (write_header_done != 0);
  if (header.size) {
    MPI_Test(&isend_body_request, &write_body_done, &isend_body_status);
    done = (write_header_done != 0 && write_body_done != 0);
  }
  return done;
}

void Message::Enqueue() { 
  Application::GetApplication()->GetOutgoingMessageQueue()->Enqueue(this);
#ifdef DEBUG_MSG_THREAD
  printf("Rank %d: enqueued message to outgoingMessageQueue\n", Application::GetApplication()->GetRank());
#endif
}

void Message::Receive() {
#ifdef LOGGING
  std::strstream s;
  s << "satsisfied...  " << id << "\n";
  log(s.str());
#endif

  pending = 0;

  if (header.size) {
    serialized = (unsigned char *)malloc(header.size);
#ifdef DEBUG_MSG_THREAD
    printf("Rank %d: Message::Receive: MPI_Recv starts (body) header.size: %lu\n", Application::GetApplication()->GetRank(), header.size);
#endif
    MPI_Recv(serialized, header.size, MPI_UNSIGNED_CHAR, header.source, Message::BODY_TAG, MPI_COMM_WORLD, &status);
#ifdef DEBUG_MSG_THREAD
    printf("Rank %d: Message::Receive: MPI_Recv ends (body) header.size: %lu\n", Application::GetApplication()->GetRank(), header.size);
#endif
  }

  if (header.broadcast_root != -1) Send();
}

void Message::Send() {
  header.source = Application::GetApplication()->GetRank();

  if (header.broadcast_root != -1) {
    int d = ((Application::GetApplication()->GetSize() + Application::GetApplication()->GetRank()) -
             header.broadcast_root) %
            Application::GetApplication()->GetSize();

    int l = (2 * d) + 1;
    if (l < Application::GetApplication()->GetSize()) {
      header.destination = (header.broadcast_root + l) % Application::GetApplication()->GetSize();

      MPI_Send((unsigned char *)&header, sizeof(header), MPI_UNSIGNED_CHAR, header.destination, Message::HEADER_TAG,
               MPI_COMM_WORLD);
      if (header.size)
        MPI_Send(serialized, header.size, MPI_UNSIGNED_CHAR, header.destination, Message::BODY_TAG, MPI_COMM_WORLD);

      if ((l + 1) < Application::GetApplication()->GetSize()) {
        header.destination = (header.broadcast_root + l + 1) % Application::GetApplication()->GetSize();

        MPI_Send((unsigned char *)&header, sizeof(header), MPI_UNSIGNED_CHAR, header.destination, Message::HEADER_TAG,
                 MPI_COMM_WORLD);
        if (header.size)
          MPI_Send(serialized, header.size, MPI_UNSIGNED_CHAR, header.destination, Message::BODY_TAG, MPI_COMM_WORLD);
      }
    }

  } else {
#ifdef DEBUG_MSG_THREAD
    printf("Rank %d: Message::Send: MPI_Send (header)\n", Application::GetApplication()->GetRank());
#endif
    // MPI_Send((unsigned char *)&header, sizeof(header), MPI_UNSIGNED_CHAR, header.destination, Message::HEADER_TAG,
    //          MPI_COMM_WORLD);
   int err1 = MPI_Isend((unsigned char *)&header, sizeof(header), MPI_UNSIGNED_CHAR, header.destination, Message::HEADER_TAG,
             MPI_COMM_WORLD, &isend_header_request);
#ifdef DEBUG_MSG_THREAD
   if (err1 != MPI_SUCCESS) {
     printf("MPI_Error MPI_Isend (header): Rank %d: err1 %d\n", Application::GetApplication()->GetRank(), err1);
     exit(1);
   }
#endif
    if (header.size) {
#ifdef DEBUG_MSG_THREAD
    printf("Rank %d: Message::Send: MPI_Isend (body) starts header.size: %lu\n", Application::GetApplication()->GetRank(), header.size);
#endif
      // MPI_Send(serialized, header.size, MPI_UNSIGNED_CHAR, header.destination, Message::BODY_TAG, MPI_COMM_WORLD);
      int err2 = MPI_Isend(serialized, header.size, MPI_UNSIGNED_CHAR, header.destination, Message::BODY_TAG, MPI_COMM_WORLD, &isend_body_request);
#ifdef DEBUG_MSG_THREAD
      if (err2 != MPI_SUCCESS) {
        printf("MPI_Error MPI_Isend (body): Rank %d: err2 %d\n", Application::GetApplication()->GetRank(), err2);
        exit(1);
      }
      printf("Rank %d: Message::Send: MPI_Isend (body) ends header.size: %lu\n", Application::GetApplication()->GetRank(), header.size);
#endif
    }
  }
}
