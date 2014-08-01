#ifndef COMMUNICATOR_COLLECTIVE_H
#define COMMUNICATOR_COLLECTIVE_H

#include <vector>

#include <boost/asio.hpp>

#include <mpi.h>

#include <core/message.h>

#include "export.h"

namespace vistle {

class Parameter;
class PythonEmbed;
class ClusterManager;

class V_CONTROLEXPORT Communicator {
   friend class ClusterManager;

 public:
   Communicator(int argc, char *argv[], int rank, const std::vector<std::string> &hosts);
   ~Communicator();
   static Communicator &the();

   bool scanModules(const std::string &dir);

   void run();
   bool dispatch(bool *work);
   bool handleMessage(const message::Message &message);
   bool forwardToMaster(const message::Message &message);
   bool broadcastAndHandleMessage(const message::Message &message);
   bool sendMessage(int receiver, const message::Message &message) const;
   void setQuitFlag();

   int hubId() const;
   int getRank() const;
   int getSize() const;

   unsigned short uiPort() const;

   ClusterManager &clusterManager() const;
   bool connectHub(const std::string &host, unsigned short port);

 private:
   bool sendHub(const message::Message &message);

   ClusterManager *m_moduleManager;

   bool isMaster() const;
   int m_hubId;
   const int m_rank;
   const int m_size;

   bool m_quitFlag;

   int m_recvSize;
   std::vector<char> m_recvBufTo0, m_recvBufToAny;
   MPI_Request m_reqAny, m_reqToRank0;

   message::Message::Type m_traceMessages;

   static Communicator *s_singleton;

   Communicator(const Communicator &other); // not implemented

   boost::asio::io_service m_ioService;
   boost::asio::ip::tcp::socket m_hubSocket;
};

} // namespace vistle

#endif
