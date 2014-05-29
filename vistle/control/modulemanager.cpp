/*
 * Visualization Testing Laboratory for Exascale Computing (VISTLE)
 */
#include <util/sysdep.h>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <mpi.h>

#include <sys/types.h>

#include <cstdlib>
#include <sstream>
#include <iostream>
#include <iomanip>

#include <core/message.h>
#include <core/messagequeue.h>
#include <core/object.h>
#include <core/shm.h>
#include <core/parameter.h>
#include <util/findself.h>

#include "modulemanager.h"
#include "communicator.h"

#ifdef NOHUB
#ifdef _WIN32
#define SPAWN_WITH_MPI
#else
#include <unistd.h>
#endif
#endif

#define CERR \
   std::cerr << "mod mgr [" << m_rank << "/" << m_size << "] "

//#define DEBUG

namespace bi = boost::interprocess;

namespace vistle {

using message::Id;

ClusterManager::ClusterManager(int argc, char *argv[], int r, const std::vector<std::string> &hosts)
: m_portManager(new PortManager(this))
, m_stateTracker("ClusterManager state", m_portManager)
, m_quitFlag(false)
, m_rank(r)
, m_size(hosts.size())
, m_barrierActive(false)
{
}

int ClusterManager::getRank() const {

   return m_rank;
}

int ClusterManager::getSize() const {

   return m_size;
}

bool ClusterManager::scanModules(const std::string &dir) {

#ifdef SCAN_MODULES_ON_HUB
    return true;
#else
   bool result = true;
   if (getRank() == 0) {
      AvailableMap availableModules;
      result = vistle::scanModules(dir, Communicator::the().hubId(), availableModules);
      for (auto &p: availableModules) {
         const auto &m = p.second;
         sendHub(message::ModuleAvailable(m.hub, m.name, m.path));
      }
   }
   return result;
#endif
}

std::vector<AvailableModule> ClusterManager::availableModules() const {

   return m_stateTracker.availableModules();
}

bool ClusterManager::checkBarrier(const message::uuid_t &uuid) const {

   assert(m_barrierActive);
   int numLocal = 0;
   for (const auto &m: m_stateTracker.runningMap) {
      if (m.second.hub == Communicator::the().hubId())
         ++numLocal;
   }
   //CERR << "checkBarrier " << uuid << ": #local=" << numLocal << ", #reached=" << reachedSet.size() << std::endl;
   if (reachedSet.size() == numLocal)
      return true;

   return false;
}

void ClusterManager::barrierReached(const message::uuid_t &uuid) {

   assert(m_barrierActive);
   MPI_Barrier(MPI_COMM_WORLD);
   reachedSet.clear();
   m_barrierActive = false;
   CERR << "Barrier [" << uuid << "] reached" << std::endl;
   message::BarrierReached m;
   m.setUuid(uuid);
   sendHub(m);
   m_stateTracker.handle(m);
}

std::string ClusterManager::getModuleName(int id) const {

   return m_stateTracker.getModuleName(id);
}


bool ClusterManager::dispatch(bool &received) {

   bool done = false;

   // handle messages from modules
   for(auto it = m_stateTracker.runningMap.begin(), next = it;
         it != m_stateTracker.runningMap.end();
         it = next) {
      next = it;
      ++next;

      const int modId = it->first;
      const auto &mod = it->second;

      // keep messages from modules that have already reached a barrier on hold
      if (reachedSet.find(modId) != reachedSet.end())
         continue;

      if (mod.hub == Communicator::the().hubId()) {
         bool recv = false;
         message::Buffer buf;
         message::MessageQueue *mq = nullptr;
         auto it = runningMap.find(modId);
         if (it != runningMap.end())
            mq = it->second.recvQueue;
         if (mq) {
            try {
               recv = mq->tryReceive(buf.msg);
            } catch (boost::interprocess::interprocess_exception &ex) {
               CERR << "receive mq " << ex.what() << std::endl;
               exit(-1);
            }
         }

         if (recv) {
            received = true;
            if (!Communicator::the().handleMessage(buf.msg))
               done = true;
         }
      }
   }

   if (m_quitFlag) {
      if (numRunning() == 0)
         return false;

      CERR << numRunning() << " modules still running..." << std::endl;
   }

   return !done;
}

bool ClusterManager::sendAll(const message::Message &message) const {

   // -1 is an invalid module id
   return sendAllOthers(-1, message);
}

bool ClusterManager::sendAllOthers(int excluded, const message::Message &message) const {

#if 1
   message::Buffer buf(message);
   if (Communicator::the().isMaster()) {
      buf.msg.setDestId(Id::Broadcast);
      sendHub(buf.msg);
   } else {
      int senderHub = message.senderId();
      if (senderHub >= Id::ModuleBase)
         senderHub = m_stateTracker.getHub(senderHub);
      if (senderHub == Communicator::the().hubId()) {
         buf.msg.setDestId(Id::ForBroadcast);
         sendHub(buf.msg);
      }
   }
#else
   if (message.senderId() != Communicator::the().hubId())
      sendHub(message);
#endif

   // handle messages to modules
   for(auto it = runningMap.begin(), next = it;
         it != runningMap.end();
         it = next) {
      next = it;
      ++next;

      const int modId = it->first;
      if (modId == excluded)
         continue;
      const auto &mod = it->second;
      const int hub = m_stateTracker.getHub(modId);

      if (hub == Communicator::the().hubId()) {
         mod.sendQueue->send(message);
      }
   }

   return true;
}

bool ClusterManager::sendUi(const message::Message &message) const {

   return Communicator::the().sendHub(message);
}

bool ClusterManager::sendHub(const message::Message &message) const {

   return Communicator::the().sendHub(message);
}

bool ClusterManager::sendMessage(const int moduleId, const message::Message &message) const {

   RunningMap::const_iterator it = runningMap.find(moduleId);
   if (it == runningMap.end()) {
      CERR << "sendMessage: module " << moduleId << " not found" << std::endl;
      return false;
   }

   auto &mod = it->second;
   const int hub = m_stateTracker.getHub(moduleId);

   if (hub == Communicator::the().hubId()) {
      std::cerr << "local send to " << moduleId << ": " << message << std::endl;
      mod.sendQueue->send(message);
   } else {
      std::cerr << "remote send to " << moduleId << ": " << message << std::endl;
      message::Buffer buf(message);
      buf.msg.setDestId(moduleId);
      sendHub(message);
   }

   return true;
}

bool ClusterManager::handle(const message::Message &message) {

   using namespace vistle::message;

   m_stateTracker.handle(message);

   bool result = true;
   switch (message.type()) {

      case message::Message::TRACE: {
         const Trace &trace = static_cast<const Trace &>(message);
         result = handlePriv(trace);
         break;
      }

      case message::Message::QUIT: {

         const message::Quit &quit = static_cast<const message::Quit &>(message);
         //result = handlePriv(quit);
         result = false;
         break;
      }

      case message::Message::SPAWN: {

         const message::Spawn &spawn = static_cast<const message::Spawn &>(message);
         result = handlePriv(spawn);
         break;
      }

      case message::Message::CONNECT: {

         const message::Connect &connect = static_cast<const message::Connect &>(message);
         result = handlePriv(connect);
         break;
      }

      case message::Message::DISCONNECT: {

         const message::Disconnect &disc = static_cast<const message::Disconnect &>(message);
         result = handlePriv(disc);
         break;
      }

      case message::Message::MODULEEXIT: {

         const message::ModuleExit &moduleExit = static_cast<const message::ModuleExit &>(message);
         result = handlePriv(moduleExit);
         //sendHub(moduleExit);
         break;
      }

      case message::Message::COMPUTE: {

         const message::Compute &comp = static_cast<const message::Compute &>(message);
         result = handlePriv(comp);
         break;
      }

      case message::Message::REDUCE: {
         const message::Reduce &red = static_cast<const message::Reduce &>(message);
         result = handlePriv(red);
         break;
      }

      case message::Message::EXECUTIONPROGRESS: {

         const message::ExecutionProgress &prog = static_cast<const message::ExecutionProgress &>(message);
         result = handlePriv(prog);
         break;
      }

      case message::Message::BUSY: {

         const message::Busy &busy = static_cast<const message::Busy &>(message);
         //sendHub(busy);
         result = handlePriv(busy);
         break;
      }

      case message::Message::IDLE: {

         const message::Idle &idle = static_cast<const message::Idle &>(message);
         ///sendHub(idle);
         result = handlePriv(idle);
         break;
      }

      case message::Message::ADDOBJECT: {

         const message::AddObject &m = static_cast<const message::AddObject &>(message);
         result = handlePriv(m);
         break;
      }

      case message::Message::OBJECTRECEIVED: {
         const message::ObjectReceived &m = static_cast<const message::ObjectReceived &>(message);
         result = handlePriv(m);
         break;
      }

      case message::Message::SETPARAMETER: {

         const message::SetParameter &m = static_cast<const message::SetParameter &>(message);
         //sendHub(m);
         result = handlePriv(m);
         break;
      }

      case message::Message::BARRIER: {

         const message::Barrier &m = static_cast<const message::Barrier &>(message);
         result = handlePriv(m);
         break;
      }

      case message::Message::BARRIERREACHED: {

         const message::BarrierReached &m = static_cast<const message::BarrierReached &>(message);
         result = handlePriv(m);
         break;
      }

      case message::Message::SENDTEXT: {
         const message::SendText &m = static_cast<const message::SendText &>(message);
         if (m_rank == 0) {
            if (Communicator::the().isMaster()) {
               message::Buffer buf(m);
               buf.msg.setDestId(Id::MasterHub);
               sendHub(buf.msg);
            } else {
               sendHub(m);
            }
         } else {
            result = Communicator::the().forwardToMaster(m);
         }
         //result = m_moduleManager->handle(m);
         break;
      }

      case Message::STARTED:
      case Message::ADDPORT:
      case Message::ADDPARAMETER:
      case Message::MODULEAVAILABLE:
      case Message::REPLAYFINISHED:
         break;

      default:

         CERR << "unhandled message from (id "
            << message.senderId() << " rank " << message.rank() << ") "
            << "type " << message.type()
            << std::endl;

         break;

   }

   return result;
}

bool ClusterManager::handlePriv(const message::Trace &trace) {

   m_stateTracker.handle(trace);
   if (trace.module() >= Id::ModuleBase) {
      sendMessage(trace.module(), trace);
   } else if (trace.module() == Id::Broadcast) {
      sendAll(trace);
   }
   return true;
}

bool ClusterManager::handlePriv(const message::Spawn &spawn) {

   if (spawn.spawnId() == Id::Invalid) {
      // ignore messages where master hub did not yet create an id
      return true;
   }
   m_stateTracker.handle(spawn);
   if (spawn.destId() != Communicator::the().hubId())
      return true;

   int newId = spawn.spawnId();
   const std::string idStr = boost::lexical_cast<std::string>(newId);

   Module &mod = runningMap[newId];
   std::string smqName = message::MessageQueue::createName("smq", newId, m_rank);
   std::string rmqName = message::MessageQueue::createName("rmq", newId, m_rank);

   try {
      mod.sendQueue = message::MessageQueue::create(smqName);
      mod.recvQueue = message::MessageQueue::create(rmqName);
   } catch (bi::interprocess_exception &ex) {

      CERR << "spawn mq " << ex.what() << std::endl;
      exit(-1);
   }

   MPI_Barrier(MPI_COMM_WORLD);

   message::SpawnPrepared prep(spawn);
   prep.setDestId(Id::LocalHub);
   sendHub(prep);

   // inform newly started module about current parameter values of other modules
   for (auto &mit: m_stateTracker.runningMap) {
      const int id = mit.first;
      const std::string moduleName = getModuleName(id);

      message::Spawn spawn(mit.second.hub, moduleName);
      spawn.setSpawnId(id);
      sendMessage(newId, spawn);

      for (std::string paramname: m_stateTracker.getParameters(id)) {
         const Parameter *param = m_stateTracker.getParameter(id, paramname);

         message::AddParameter add(param, moduleName);
         add.setSenderId(id);
         sendMessage(newId, add);

         message::SetParameter set(id, paramname, param);
         set.setSenderId(id);
         sendMessage(newId, set);
      }
   }

   return true;
}

bool ClusterManager::handlePriv(const message::Connect &connect) {

   int modFrom = connect.getModuleA();
   int modTo = connect.getModuleB();
   const char *portFrom = connect.getPortAName();
   const char *portTo = connect.getPortBName();
   const Port *from = portManager().getPort(modFrom, portFrom);
   const Port *to = portManager().getPort(modTo, portTo);

   message::Connect c = connect;
   if (from && to && from->getType() == Port::INPUT && to->getType() == Port::OUTPUT) {
      c.reverse();
      std::swap(modFrom, modTo);
      std::swap(portFrom, portTo);
   }

   m_stateTracker.handle(c);
   if (portManager().addConnection(modFrom, portFrom, modTo, portTo)) {
      // inform modules about connections
      if (Communicator::the().isMaster()) {
         sendMessage(modFrom, c);
         sendMessage(modTo, c);
         sendUi(c);
      }
   } else {
      return false;
   }
   return true;
}

bool ClusterManager::handlePriv(const message::Disconnect &disconnect) {

   int modFrom = disconnect.getModuleA();
   int modTo = disconnect.getModuleB();
   const char *portFrom = disconnect.getPortAName();
   const char *portTo = disconnect.getPortBName();
   const Port *from = portManager().getPort(modFrom, portFrom);
   const Port *to = portManager().getPort(modTo, portTo);

   message::Disconnect d = disconnect;
   if (from->getType() == Port::INPUT && to->getType() == Port::OUTPUT) {
      d.reverse();
      std::swap(modFrom, modTo);
      std::swap(portFrom, portTo);
   }
   
   m_stateTracker.handle(d);
   if (portManager().removeConnection(modFrom, portFrom, modTo, portTo)) {

      if (Communicator::the().isMaster()) {
         sendMessage(modFrom, d);
         sendMessage(modTo, d);
         sendUi(d);
      }
   } else {

      if (!m_messageQueue.empty()) {
         // only if messages are already queued, there is a chance that this
         // connection might still be established
         return false;
      }
   }

   return true;
}

bool ClusterManager::handlePriv(const message::ModuleExit &moduleExit) {

   sendAllOthers(moduleExit.senderId(), moduleExit);

   int mod = moduleExit.senderId();

   portManager().removeConnections(mod);

   if (!moduleExit.isForwarded()) {

      RunningMap::iterator it = runningMap.find(mod);
      if (it == runningMap.end()) {
         CERR << " Module [" << mod << "] quit, but not found in running map" << std::endl;
         return true;
      }
      if (m_rank == 0) {
         message::ModuleExit exit = moduleExit;
         exit.setForwarded();
         if (!Communicator::the().broadcastAndHandleMessage(exit))
            return false;
      }
      return true;
   }

   m_stateTracker.handle(moduleExit);

   //CERR << " Module [" << mod << "] quit" << std::endl;

   { 
      RunningMap::iterator it = runningMap.find(mod);
      if (it != runningMap.end()) {
         runningMap.erase(it);
      } else {
         CERR << " Module [" << mod << "] not found in map" << std::endl;
      }
   }
   {
      ModuleSet::iterator it = reachedSet.find(mod);
      if (it != reachedSet.end())
         reachedSet.erase(it);
   }

   if (m_barrierActive && checkBarrier(m_barrierUuid))
      barrierReached(m_barrierUuid);

   return true;
}

bool ClusterManager::handlePriv(const message::Compute &compute) {

   assert (compute.getModule() != -1);
   RunningMap::iterator i = runningMap.find(compute.getModule());
   if (i != runningMap.end()) {
      i->second.sendQueue->send(compute);
   }

   return true;
}

bool ClusterManager::handlePriv(const message::ExecutionProgress &prog) {

   m_stateTracker.handle(prog);
   RunningMap::iterator i = runningMap.find(prog.senderId());
   if (i == runningMap.end()) {
      return false;
   }

   auto i2 = m_stateTracker.runningMap.find(prog.senderId());
   if (i2 == m_stateTracker.runningMap.end()) {
      return false;
   }

   // initiate reduction if requested by module
   auto &mod = i->second;
   auto &mod2 = i2->second;
   bool forward = true;
   if (mod2.reducePolicy != message::ReducePolicy::Never
         && prog.stage() == message::ExecutionProgress::Finish
         && !mod.reducing) {
      // will be sent after reduce()
      forward = false;
   }

   //std::cerr << "ExecutionProgress " << prog.stage() << " received from " << prog.senderId() << std::endl;
   bool result = true;
   if (m_rank == 0) {
      switch (prog.stage()) {
         case message::ExecutionProgress::Start: {
            assert(mod.ranksFinished < m_size);
            ++mod.ranksStarted;
            break;
         }

         case message::ExecutionProgress::Finish: {
            ++mod.ranksFinished;
            if (mod.ranksFinished == m_size) {
               assert(mod.ranksStarted == m_size);
               mod.ranksFinished = 0;

               if (!mod.reducing && mod2.reducePolicy != message::ReducePolicy::Never) {
                  mod.reducing = true;
                  message::Reduce red(prog.senderId());
                  Communicator::the().broadcastAndHandleMessage(red);
               } else {
                  mod.ranksStarted = 0;
                  mod.reducing = false;
               }
            }
            break;
         }

         case message::ExecutionProgress::Iteration: {
            break;
         }

         case message::ExecutionProgress::Timestep: {
            break;
         }
      }
   } else {
      result = Communicator::the().forwardToMaster(prog);
   }

   // forward message to all directly connected down-stream modules, but only once
   if (forward) {
      std::set<int> connectedIds;
      auto out = portManager().getOutputPorts(prog.senderId());
      for (Port *port: out) {
         const Port::PortSet *list = NULL;
         if (port) {
            list = portManager().getConnectionList(port);
         }
         if (list) {
            Port::PortSet::const_iterator pi;
            for (pi = list->begin(); pi != list->end(); ++pi) {

               int destId = (*pi)->getModuleID();
               connectedIds.insert(destId);
            }
         }
      }
      for (int id: connectedIds) {
         sendMessage(id, prog);
      }
   }

   return result;
}

bool ClusterManager::handlePriv(const message::Reduce &reduce) {

   m_stateTracker.handle(reduce);
   sendMessage(reduce.module(), reduce);
   return true;
}

bool ClusterManager::handlePriv(const message::Busy &busy) {

   //sendAllOthers(busy.senderId(), busy);
   if (Communicator::the().isMaster()) {
      message::Buffer buf(busy);
      buf.msg.setDestId(Id::MasterHub);
      sendHub(buf.msg);
   }
   return m_stateTracker.handle(busy);
}

bool ClusterManager::handlePriv(const message::Idle &idle) {

   //sendAllOthers(idle.senderId(), idle);
   if (Communicator::the().isMaster()) {
      message::Buffer buf(idle);
      buf.msg.setDestId(Id::MasterHub);
      sendHub(buf.msg);
   }
   return m_stateTracker.handle(idle);
}

bool ClusterManager::handlePriv(const message::ObjectReceived &objRecv) {

   m_stateTracker.handle(objRecv);
   sendMessage(objRecv.senderId(), objRecv);
   return true;
}


bool ClusterManager::handlePriv(const message::SetParameter &setParam) {

   m_stateTracker.handle(setParam);
#ifdef DEBUG
   CERR << "SetParameter: sender=" << setParam.senderId() << ", module=" << setParam.getModule() << ", name=" << setParam.getName() << std::endl;
#endif

   bool handled = true;
   Parameter *param = getParameter(setParam.getModule(), setParam.getName());
   Parameter *applied = NULL;
   if (param) {
      applied = param->clone();
      setParam.apply(applied);
   }
   if (setParam.senderId() != setParam.getModule()) {
      // message to owning module
      RunningMap::iterator i = runningMap.find(setParam.getModule());
      if (i != runningMap.end() && param)
         sendMessage(setParam.getModule(), setParam);
      else
         handled = false;
   } else {
      // notification by owning module about a changed parameter
      if (param) {
         setParam.apply(param);
      } else {
         CERR << "no such parameter: module id=" << setParam.getModule() << ", name=" << setParam.getName() << std::endl;
      }

      // let all modules know that a parameter was changed
      sendAllOthers(setParam.senderId(), setParam);
   }

   if (!setParam.isReply()) {

      // update linked parameters
      if (Communicator::the().isMaster()) {
         Port *port = portManager().getPort(setParam.getModule(), setParam.getName());
         if (port && applied) {
            ParameterSet conn = m_stateTracker.getConnectedParameters(*param);

            for (ParameterSet::iterator it = conn.begin();
                  it != conn.end();
                  ++it) {
               const Parameter *p = *it;
               if (p->module()==setParam.getModule() && p->getName()==setParam.getName()) {
                  // don't update parameter which was set originally again
                  continue;
               }
               message::SetParameter set(p->module(), p->getName(), applied);
               set.setUuid(setParam.uuid());
               sendMessage(p->module(), set);
            }
         } else {
#ifdef DEBUG
            CERR << " SetParameter ["
               << setParam.getModule() << ":" << setParam.getName()
               << "]: port not found" << std::endl;
#endif
         }
      }
   }
   delete applied;

   return handled;
}

bool ClusterManager::handlePriv(const message::AddObject &addObj) {

   m_stateTracker.handle(addObj);
   Object::const_ptr obj = addObj.takeObject();
   assert(obj->refcount() >= 1);
#if 0
   std::cerr << "Module " << addObj.senderId() << ": "
      << "AddObject " << addObj.getHandle() << " (" << obj->getName() << ")"
      << " ref " << obj->refcount()
      << " to port " << addObj.getPortName() << std::endl;
#endif

   Port *port = portManager().getPort(addObj.senderId(), addObj.getPortName());
   const Port::PortSet *list = NULL;
   if (port) {
      list = portManager().getConnectionList(port);
   }
   if (list) {
      Port::PortSet::const_iterator pi;
      for (pi = list->begin(); pi != list->end(); ++pi) {

         int destId = (*pi)->getModuleID();

         message::AddObject a((*pi)->getName(), obj);
         a.setSenderId(addObj.senderId());
         a.setUuid(addObj.uuid());
         a.setRank(addObj.rank());
         sendMessage(destId, a);

         auto it = m_stateTracker.runningMap.find(destId);
         if (it == m_stateTracker.runningMap.end()) {
            CERR << "port connection to module that is not running" << std::endl;
            assert("port connection to module that is not running" == 0);
            continue;
         }
         auto &destMod = it->second;

         message::Compute c(destId);
         c.setUuid(addObj.uuid());
         c.setReason(message::Compute::AddObject);
         if (destMod.schedulingPolicy == message::SchedulingPolicy::Single) {
            sendMessage(destId, c);
         } else {
            c.setAllRanks(true);
            if (!Communicator::the().broadcastAndHandleMessage(c))
               return false;
         }

         if (destMod.objectPolicy == message::ObjectReceivePolicy::NotifyAll
            || destMod.objectPolicy == message::ObjectReceivePolicy::Distribute) {
            message::ObjectReceived recv(addObj.getPortName(), obj);
            recv.setUuid(addObj.uuid());
            recv.setSenderId(destId);

            if (!Communicator::the().broadcastAndHandleMessage(recv))
               return false;
         }

         switch (destMod.reducePolicy) {
            case message::ReducePolicy::Never:
               break;
            case message::ReducePolicy::PerTimestep:
               break;
            case message::ReducePolicy::OverAll:
               break;
         }
      }
   }
   else
      CERR << "AddObject ["
         << addObj.getHandle() << "] to port ["
         << addObj.getPortName() << "] of [" << addObj.senderId() << "]: port not found" << std::endl;

   return true;
}

bool ClusterManager::handlePriv(const message::Barrier &barrier) {

   m_barrierActive = true;
   m_stateTracker.handle(barrier);
   sendHub(barrier);
   CERR << "Barrier [" << barrier.uuid() << "]" << std::endl;
   m_barrierUuid = barrier.uuid();
   if (checkBarrier(m_barrierUuid)) {
      barrierReached(m_barrierUuid);
   } else {
      sendAll(barrier);
   }
   return true;
}

bool ClusterManager::handlePriv(const message::BarrierReached &barrReached) {

   assert(m_barrierActive);
#ifdef DEBUG
   CERR << "BarrierReached [barrier " << barrReached.uuid() << ", module " << barrReached.senderId() << "]" << std::endl;
#endif
   reachedSet.insert(barrReached.senderId());

   if (checkBarrier(m_barrierUuid)) {
      barrierReached(m_barrierUuid);
#ifdef DEBUG
   } else {
      CERR << "BARRIER: reached by " << reachedSet.size() << "/" << numRunning() << std::endl;
#endif
   }

   return true;
}

bool ClusterManager::quit() {

   if (!m_quitFlag)
      sendAll(message::Kill(message::Id::Broadcast));

   // receive all ModuleExit messages from modules
   // retry for some time, modules that don't answer might have crashed
   CERR << "waiting for " << numRunning() << " modules to quit" << std::endl;

   if (m_size > 1)
      MPI_Barrier(MPI_COMM_WORLD);

   m_quitFlag = true;

   return numRunning()==0;
}

bool ClusterManager::quitOk() const {

   return m_quitFlag && numRunning()==0;
}

ClusterManager::~ClusterManager() {

}

PortManager &ClusterManager::portManager() const {

   return *m_portManager;
}

std::vector<std::string> ClusterManager::getParameters(int id) const {

   return m_stateTracker.getParameters(id);
}

Parameter *ClusterManager::getParameter(int id, const std::string &name) const {

   return m_stateTracker.getParameter(id, name);
}

void ClusterManager::queueMessage(const message::Message &msg) {

   const char *m = static_cast<const char *>(static_cast<const void *>(&msg));
   std::copy(m, m+message::Message::MESSAGE_SIZE, std::back_inserter(m_messageQueue));
   //CERR << "queueing " << msg.type() << ", now " << m_messageQueue.size()/message::Message::MESSAGE_SIZE << " in queue" << std::endl;
}

void ClusterManager::replayMessages() {

   if (Communicator::the().isMaster()) {
      std::vector<char> queue;
      std::swap(m_messageQueue, queue);
      //CERR << "replaying " << queue.size()/message::Message::MESSAGE_SIZE << " messages" << std::endl;
      for (size_t i=0; i<queue.size(); i+=message::Message::MESSAGE_SIZE) {
         const message::Message &m = *static_cast<const message::Message *>(static_cast<const void *>(&queue[i]));
         Communicator::the().handleMessage(m);
      }
   }
}

int ClusterManager::numRunning() const {

   int n = 0;
   for (auto &m: runningMap) {
      int state = m_stateTracker.getModuleState(m.first);
      if ((state & StateObserver::Initialized) && !(state & StateObserver::Killed))
         ++n;
   }
   return n;
}

} // namespace vistle
