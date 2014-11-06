/*
 * Visualization Testing Laboratory for Exascale Computing (VISTLE)
 */
#include <boost/foreach.hpp>
#include <boost/asio.hpp>

#include <mpi.h>

#include <sys/types.h>

#include <cstdlib>
#include <iostream>
#include <iomanip>

#include <core/message.h>
#include <core/tcpmessage.h>
#include <core/object.h>
#include <core/parameter.h>
#include <util/findself.h>
#include <util/sleep.h>
#include <util/tools.h>

#include "communicator.h"
#include "clustermanager.h"

#define CERR \
   std::cerr << "comm [" << m_rank << "/" << m_size << "] "

using namespace boost::interprocess;
namespace asio = boost::asio;

namespace vistle {

using message::Id;

enum MpiTags {
   TagModulue,
   TagToRank0,
   TagToAny,
};

Communicator *Communicator::s_singleton = NULL;

Communicator::Communicator(int argc, char *argv[], int r, const std::vector<std::string> &hosts)
: m_clusterManager(new ClusterManager(argc, argv, r, hosts))
, m_hubId(message::Id::Invalid)
, m_rank(r)
, m_size(hosts.size())
, m_quitFlag(false)
, m_recvSize(0)
, m_traceMessages(message::Message::INVALID)
, m_hubSocket(m_ioService)
{
   vassert(s_singleton == NULL);
   s_singleton = this;

   CERR << "started" << std::endl;

   message::DefaultSender::init(m_hubId, m_rank);

   // post requests for length of next MPI message
   if (m_size > 1) {
      MPI_Irecv(&m_recvSize, 1, MPI_INT, MPI_ANY_SOURCE, TagToAny, MPI_COMM_WORLD, &m_reqAny);

      if (m_rank == 0) {
         MPI_Irecv(m_recvBufTo0.buf.data(), m_recvBufTo0.buf.size(), MPI_BYTE, MPI_ANY_SOURCE, TagToRank0, MPI_COMM_WORLD, &m_reqToRank0);
      }
   }
}

Communicator &Communicator::the() {

   vassert(s_singleton && "make sure to use the vistle Python module only from within vistle");
   if (!s_singleton)
      exit(1);
   return *s_singleton;
}

int Communicator::hubId() const {

   return m_hubId;
}

bool Communicator::isMaster() const {

   vassert(m_hubId <= Id::MasterHub); // make sure that hub id has already been set
   return m_hubId == Id::MasterHub;
}

int Communicator::getRank() const {

   return m_rank;
}

int Communicator::getSize() const {

   return m_size;
}

bool Communicator::connectHub(const std::string &host, unsigned short port) {

   int ret = 1;
   if (getRank() == 0) {

      CERR << "connecting to hub on " << host << ":" << port << "..." << std::flush;
      asio::ip::tcp::resolver resolver(m_ioService);
      asio::ip::tcp::resolver::query query(host, boost::lexical_cast<std::string>(port));
      asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
      boost::system::error_code ec;
      asio::connect(m_hubSocket, endpoint_iterator, ec);
      if (ec) {
         std::cerr << std::endl;
         CERR << "could not establish connection to hub at " << host << ":" << port << std::endl;
         ret = false;
      } else {
         std::cerr << " ok." << std::endl;
      }
   }

   MPI_Bcast(&ret, 1, MPI_INT, 0, MPI_COMM_WORLD);

   return ret;
}

bool Communicator::sendHub(const message::Message &message) {

   if (getRank() == 0)
      return message::send(m_hubSocket, message);
   else
      return true;
}


bool Communicator::scanModules(const std::string &dir) {

    return m_clusterManager->scanModules(dir);
}

void Communicator::setQuitFlag() {

   m_quitFlag = true;
}

void Communicator::run() {

   bool work = false;
   while (dispatch(&work)) {

      if (parentProcessDied())
         throw(except::parent_died());

      vistle::adaptive_wait(work);
   }
}

bool Communicator::dispatch(bool *work) {

   bool done = false;

   bool received = false;
   received = false;

   // check for new UIs and other network clients
   if (m_rank == 0) {

      if (!done)
         done = m_quitFlag;

      if (done) {
         sendHub(message::Quit());
      }
   }

   // handle or broadcast messages received from slaves (rank > 0)
   if (m_size > 1) {
      if (m_rank == 0) {
         int flag;
         MPI_Status status;
         MPI_Test(&m_reqToRank0, &flag, &status);
         if (flag && status.MPI_TAG == TagToRank0) {

            vassert(m_rank == 0);
            received = true;
            message::Message *message = &m_recvBufTo0.msg;
            if (message->broadcast()) {
               if (!broadcastAndHandleMessage(*message))
                  done = true;
            }  else {
               if (!handleMessage(*message))
                  done = true;
            }
            MPI_Irecv(m_recvBufTo0.buf.data(), m_recvBufTo0.buf.size(), MPI_BYTE, MPI_ANY_SOURCE, TagToRank0, MPI_COMM_WORLD, &m_reqToRank0);
         }
      }

      // test for message size from another MPI node
      //    - receive actual message from broadcast (on any rank)
      //    - receive actual message from slave rank (on rank 0) for broadcasting
      //    - handle message
      //    - post another MPI receive for size of next message
      int flag;
      MPI_Status status;
      MPI_Test(&m_reqAny, &flag, &status);
      if (flag && status.MPI_TAG == TagToAny) {

         vassert(m_recvSize <= m_recvBufToAny.buf.size());
         MPI_Bcast(m_recvBufToAny.buf.data(), m_recvSize, MPI_BYTE,
               status.MPI_SOURCE, MPI_COMM_WORLD);
         if (m_recvSize > 0) {
            received = true;

            message::Message *message = &m_recvBufToAny.msg;
#if 0
            printf("[%02d] message from [%02d] message type %d m_size %d\n",
                  m_rank, status.MPI_SOURCE, message->getType(), mpiMessageSize);
#endif
            if (!handleMessage(*message))
               done = true;
         }

         MPI_Irecv(&m_recvSize, 1, MPI_INT, MPI_ANY_SOURCE, TagToAny, MPI_COMM_WORLD, &m_reqAny);
      }
   }

   if (m_rank == 0) {
      m_ioService.poll();
      message::Buffer buf;
      bool gotMsg = false;
      if (!message::recv(m_hubSocket, buf.msg, gotMsg)) {
         broadcastAndHandleMessage(message::Quit());
         done = true;
      } else if (gotMsg) {
         received = true;
         if(!broadcastAndHandleMessage(buf.msg))
            done = true;
      }
   }

   // test for messages from modules
   if (done) {
      if (m_clusterManager) {
         m_clusterManager->quit();
         m_quitFlag = false;
         done = false;
      }
   }
   if (m_clusterManager->quitOk()) {
      done = true;
   }
   if (!done && m_clusterManager) {
      done = !m_clusterManager->dispatch(received);
   }
   if (done) {
      delete m_clusterManager;
      m_clusterManager = nullptr;
   }

   if (work)
      *work = received;

   return !done;
}

bool Communicator::sendMessage(const int moduleId, const message::Message &message) const {

   return clusterManager().sendMessage(moduleId, message);
}

bool Communicator::forwardToMaster(const message::Message &message) {

   vassert(m_rank != 0);
   if (m_rank != 0) {

      MPI_Send(const_cast<message::Message *>(&message), message.m_size, MPI_BYTE, 0, TagToRank0, MPI_COMM_WORLD);
   }

   return true;
}

bool Communicator::broadcastAndHandleMessage(const message::Message &message) {

   if (m_rank == 0) {
      std::vector<MPI_Request> s(m_size);
      for (int index = 0; index < m_size; ++index) {
         if (index != m_rank)
            MPI_Isend(const_cast<unsigned int *>(&message.m_size), 1, MPI_UNSIGNED, index, TagToAny,
                  MPI_COMM_WORLD, &s[index]);
      }

      MPI_Bcast(const_cast<message::Message *>(&message), message.m_size, MPI_BYTE, m_rank, MPI_COMM_WORLD);

      const bool result = handleMessage(message);

      // wait for completion
      for (int index=0; index<m_size; ++index) {

         if (index == m_rank)
            continue;

         MPI_Wait(&s[index], MPI_STATUS_IGNORE);
      }

      return result;
   } else {
      MPI_Send(const_cast<message::Message *>(&message), message.m_size, MPI_BYTE, 0, TagToRank0, MPI_COMM_WORLD);

      // message will be handled when received again from rank 0
      return true;
   }
}


bool Communicator::handleMessage(const message::Message &message) {

   switch(message.type()) {
      case message::Message::SETID: {
         auto set = static_cast<const message::SetId &>(message);
         m_hubId = set.getId();
         CERR << "got id " << m_hubId << std::endl;
         message::DefaultSender::init(m_hubId, m_rank);
         return true;
         break;
      }
      default:
         return m_clusterManager->handle(message);
   }

   return true;
}

Communicator::~Communicator() {

   delete m_clusterManager;
   m_clusterManager = NULL;

   if (m_size > 1) {
      int dummy = 0;
      MPI_Request s;
      MPI_Isend(&dummy, 1, MPI_INT, (m_rank + 1) % m_size, TagToAny, MPI_COMM_WORLD, &s);
      if (m_rank == 1) {
         MPI_Request s2;
         MPI_Isend(&dummy, 1, MPI_BYTE, 0, TagToRank0, MPI_COMM_WORLD, &s2);
         //MPI_Wait(&s2, MPI_STATUS_IGNORE);
      }
      //MPI_Wait(&s, MPI_STATUS_IGNORE);
      //MPI_Wait(&m_reqAny, MPI_STATUS_IGNORE);
   }
   //MPI_Barrier(MPI_COMM_WORLD);
}

ClusterManager &Communicator::clusterManager() const {

   return *m_clusterManager;
}

} // namespace vistle
