/*
 * Visualization Testing Laboratory for Exascale Computing (VISTLE)
 */

#include <vistle/util/sysdep.h>
#include <vistle/util/hostname.h>

#include <sys/types.h>

#include <cstdlib>
#include <cstdio>

#include <sstream>
#include <iostream>
#include <iomanip>

#include <boost/mpi.hpp>
#include <boost/lexical_cast.hpp>

#include <vistle/core/message.h>
#include <vistle/core/messagequeue.h>
#include <vistle/core/object.h>
#include <vistle/core/shm.h>

#include "communicator.h"
#include "executor.h"

namespace interprocess = boost::interprocess;
namespace mpi = boost::mpi;

namespace vistle {

using namespace message;

Executor::Executor(int argc, char *argv[], boost::mpi::communicator comm)
   : m_name("vistle")
     , m_rank(-1)
     , m_size(-1)
     , m_comm(NULL)
     , m_argc(argc)
     , m_argv(argv)
{
   m_size = comm.size();
   m_rank = comm.rank();

   if (argc < 4) {
      std::cerr << "usage: " << argv[0] << " [hostname] [port] [dataPort]" << std::endl;
      std::cerr << "  hostname and port where Vistle hub can be reached have to be specified" << std::endl;
      exit(1);
   }

   unsigned short port = boost::lexical_cast<unsigned short>(argv[2]);
   m_name = Shm::instanceName(argv[1], port);
   unsigned short dataPort = boost::lexical_cast<unsigned short>(argv[3]);

   // broadcast name of vistle session
   mpi::broadcast(comm, m_name, 0);

   std::string hostname = vistle::hostname();
   std::vector<std::string> hostnames;
   mpi::all_gather(comm, hostname, hostnames);

   // determine first rank on each host
   bool first = true;
#ifndef SHMPERRANK
   for (int index = 0; index < m_rank; index ++)
      if (hostnames[index] == hostname)
         first = false;
#endif

   if (first) {
      vistle::Shm::remove(m_name, 0, m_rank);
      vistle::Shm::create(m_name, 0, m_rank);
   }
   comm.barrier();
   if (!first)
      vistle::Shm::attach(m_name, 0, m_rank);
   comm.barrier();

   m_comm = new vistle::Communicator(m_rank, hostnames, comm);
   if (!m_comm->connectHub(argv[1], port, dataPort)) {
      std::stringstream err;
      err << "failed to connect to Vistle hub on " << argv[1] << ":" << port;
      throw vistle::exception(err.str());
   }
}

Executor::~Executor()
{
   delete m_comm;

   Shm::the().detach();
}

void Executor::setModuleDir(const std::string &dir) {


    return m_comm->setModuleDir(dir);
}


bool Executor::config(int argc, char *argv[]) {

   return true;
}

void Executor::run() {

   if (!config(m_argc, m_argv))
      return;

   m_comm->run();
}

} // namespace vistle
