#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include <list>
#include <vector>

namespace vistle {

   namespace message {
      class Message;
   }

   class Communicator {

   public:
      Communicator();
      ~Communicator();

      bool dispatch();

   private:
      bool handleMessage(message::Message *message);

      unsigned char *socketBuffer;
      int clientSocket;
      int moduleID;

      int rank;
      int size;

      MPI_Request request;
      std::list<vistle::message::Message *> messageQueue;

      std::vector<vistle::message::Message *> sendMessages;
      std::vector<MPI_Request> sendRequests;

      int messageLength;
      char *rbuf;
   };
}

#endif
