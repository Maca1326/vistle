#ifndef MODULEMANAGER_H
#define MODULEMANAGER_H

#include <vector>
#include <map>

#include <boost/interprocess/ipc/message_queue.hpp>

#include <util/directory.h>

#include <core/statetracker.h>
#include <core/message.h>
#include <core/messagequeue.h>

#include "portmanager.h"
#include "export.h"

namespace vistle {

namespace message {
   class Message;
   class MessageQueue;
}

class Parameter;

class V_CONTROLEXPORT ModuleManager {
   friend class Communicator;

 public:
   ModuleManager(int argc, char *argv[], int rank, const std::vector<std::string> &hosts);
   ~ModuleManager();
   static ModuleManager &the();

   bool scanModules(const std::string &dir);

   bool dispatch(bool &received);

   bool sendMessage(int receiver, const message::Message &message) const;
   bool sendAll(const message::Message &message) const;
   bool sendAllOthers(int excluded, const message::Message &message) const;
   bool sendUi(const message::Message &message) const;
   bool sendHub(const message::Message &message) const;

   bool quit();
   bool quitOk() const;

   int getRank() const;
   int getSize() const;

   void resetModuleCounter();
   int newModuleID();
   int currentExecutionCount();
   int newExecutionCount();

   std::vector<AvailableModule> availableModules() const;

   std::vector<int> getRunningList() const;
   std::vector<int> getBusyList() const;
   std::string getModuleName(int id) const;
   std::vector<std::string> getParameters(int id) const;
   Parameter *getParameter(int id, const std::string &name) const;

   std::vector<char> getState() const;

   PortManager &portManager() const;

   bool handle(const message::Message &msg);

 private:
   void queueMessage(const message::Message &msg);
   void replayMessages();
   std::vector<char> m_messageQueue;

   boost::shared_ptr<PortManager> m_portManager;
   StateTracker m_stateTracker;
   bool m_quitFlag;

   bool handlePriv(const message::Ping &ping);
   bool handlePriv(const message::Pong &pong);
   bool handlePriv(const message::Trace &trace);
   bool handlePriv(const message::Spawn &spawn);
   bool handlePriv(const message::Started &started);
   bool handlePriv(const message::Connect &connect);
   bool handlePriv(const message::Disconnect &disc);
   bool handlePriv(const message::ModuleExit &moduleExit);
   bool handlePriv(const message::Compute &compute);
   bool handlePriv(const message::Reduce &reduce);
   bool handlePriv(const message::ExecutionProgress &prog);
   bool handlePriv(const message::Busy &busy);
   bool handlePriv(const message::Idle &idle);
   bool handlePriv(const message::CreatePort &createPort);
   bool handlePriv(const message::AddParameter &addParam);
   bool handlePriv(const message::SetParameter &setParam);
   bool handlePriv(const message::SetParameterChoices &setChoices);
   bool handlePriv(const message::Kill &kill);
   bool handlePriv(const message::AddObject &addObj);
   bool handlePriv(const message::ObjectReceived &objRecv);
   bool handlePriv(const message::Barrier &barrier);
   bool handlePriv(const message::BarrierReached &barrierReached);
   bool handlePriv(const message::ResetModuleIds &reset);
   bool handlePriv(const message::ObjectReceivePolicy &receivePolicy);
   bool handlePriv(const message::SchedulingPolicy &schedulingPolicy);
   bool handlePriv(const message::ReducePolicy &reducePolicy);
   bool handlePriv(const message::ModuleAvailable &avail);

   std::string m_bindir;

   const int m_rank;
   const int m_size;
   const std::vector<std::string> m_hosts;

   AvailableMap m_availableModules;

   struct Module {
      message::MessageQueue *sendQueue;
      message::MessageQueue *recvQueue;

      Module(): sendQueue(nullptr), recvQueue(nullptr),
         hub(-1), local(false), baseRank(0),
         ranksStarted(0), ranksFinished(0), reducing(false),
         objectPolicy(message::ObjectReceivePolicy::Single), schedulingPolicy(message::SchedulingPolicy::Single), reducePolicy(message::ReducePolicy::Never)
         {}
      ~Module() {
         delete sendQueue;
         delete recvQueue;
      }
      int hub;
      bool local;
      int baseRank;
      int ranksStarted, ranksFinished;
      bool reducing;
      message::ObjectReceivePolicy::Policy objectPolicy;
      message::SchedulingPolicy::Schedule schedulingPolicy;
      message::ReducePolicy::Reduce reducePolicy;
   };
   typedef std::map<int, Module> RunningMap;
   RunningMap runningMap;
   int numRunning() const;

   int m_moduleCounter; //< used for module ids
   int m_executionCounter; //< incremented each time the pipeline is executed

   // barrier related stuff
   bool checkBarrier(const message::uuid_t &uuid) const;
   void barrierReached(const message::uuid_t &uuid);
   bool m_barrierActive;
   message::uuid_t m_barrierUuid;
   int m_reachedBarriers;
   typedef std::set<int> ModuleSet;
   ModuleSet reachedSet;
};

} // namespace vistle

#endif
