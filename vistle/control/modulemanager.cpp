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

ModuleManager::ModuleManager(int argc, char *argv[], int r, const std::vector<std::string> &hosts)
: m_portManager(this)
, m_stateTracker("ModuleManager state", &m_portManager)
, m_quitFlag(false)
, m_rank(r)
, m_size(hosts.size())
, m_hosts(hosts)
, m_moduleCounter(0)
, m_executionCounter(1)
, m_barrierActive(false)
{

   m_bindir = getbindir(argc, argv);
}

int ModuleManager::getRank() const {

   return m_rank;
}

int ModuleManager::getSize() const {

   return m_size;
}

bool ModuleManager::scanModules(const std::string &dir) {

#ifdef SCAN_MODULES_ON_HUB
    return true;
#else
    return vistle::scanModules(dir, Communicator::the().hubId(), m_availableModules);
#endif
}

std::vector<AvailableModule> ModuleManager::availableModules() const {

    std::vector<AvailableModule> ret;
    for (auto mod: m_availableModules) {
        ret.push_back(mod.second);
    }
    return ret;
}

int ModuleManager::newModuleID() {
   ++m_moduleCounter;

   return m_moduleCounter;
}

int ModuleManager::currentExecutionCount() {

   return m_executionCounter;
}

int ModuleManager::newExecutionCount() {

   return m_executionCounter++;
}

void ModuleManager::resetModuleCounter() {

   m_moduleCounter = 0;
}

bool ModuleManager::checkBarrier(const message::uuid_t &uuid) const {

   assert(m_barrierActive);
   int numLocal = 0;
   for (const auto &m: runningMap) {
      if (m.second.local)
         ++numLocal;
   }
   //CERR << "checkBarrier " << uuid << ": #local=" << numLocal << ", #reached=" << reachedSet.size() << std::endl;
   if (reachedSet.size() == numLocal)
      return true;

   return false;
}

void ModuleManager::barrierReached(const message::uuid_t &uuid) {

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

std::vector<int> ModuleManager::getRunningList() const {

   return m_stateTracker.getRunningList();
}

std::vector<int> ModuleManager::getBusyList() const {

   return m_stateTracker.getBusyList();
}

std::string ModuleManager::getModuleName(int id) const {

   return m_stateTracker.getModuleName(id);
}


bool ModuleManager::dispatch(bool &received) {

   bool done = false;

   // handle messages from modules
   for(RunningMap::iterator next, it = runningMap.begin();
         it != runningMap.end();
         it = next) {
      next = it;
      ++next;

      const int modId = it->first;
      const Module &mod = it->second;

      // keep messages from modules that have already reached a barrier on hold
      if (reachedSet.find(modId) != reachedSet.end())
         continue;

      if (mod.local) {
         bool recv = false;
         char msgRecvBuf[vistle::message::Message::MESSAGE_SIZE];
         message::Message *msg = reinterpret_cast<message::Message *>(msgRecvBuf);
         message::MessageQueue *mq = mod.recvQueue;
         try {
            recv = mq->tryReceive(*msg);
         } catch (boost::interprocess::interprocess_exception &ex) {
            CERR << "receive mq " << ex.what() << std::endl;
            exit(-1);
         }

         if (recv) {
            if (!Communicator::the().handleMessage(*msg))
               done = true;
         }

         if (recv)
            received = true;
      }
   }

   if (m_quitFlag) {
      if (numRunning() == 0)
         return false;

      CERR << numRunning() << " modules still running..." << std::endl;
   }

   return !done;
}

bool ModuleManager::sendAll(const message::Message &message) const {

   // -1 is an invalid module id
   return sendAllOthers(-1, message);
}

bool ModuleManager::sendAllOthers(int excluded, const message::Message &message) const {

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
   for(RunningMap::const_iterator next, it = runningMap.begin();
         it != runningMap.end();
         it = next) {
      next = it;
      ++next;

      const int modId = it->first;
      if (modId == excluded)
         continue;
      const Module &mod = it->second;

      if (mod.local)
         mod.sendQueue->send(message);
   }

   return true;
}

bool ModuleManager::sendUi(const message::Message &message) const {

   return Communicator::the().sendHub(message);
}

bool ModuleManager::sendHub(const message::Message &message) const {

   return Communicator::the().sendHub(message);
}

bool ModuleManager::sendMessage(const int moduleId, const message::Message &message) const {

   RunningMap::const_iterator it = runningMap.find(moduleId);
   if (it == runningMap.end()) {
      CERR << "sendMessage: module " << moduleId << " not found" << std::endl;
      return false;
   }

   auto &mod = it->second;

   if (mod.hub == Communicator::the().hubId()) {
      std::cerr << "local send to " << moduleId << ": " << message << std::endl;
      if (mod.local)
         mod.sendQueue->send(message);
   } else {
      std::cerr << "remote send to " << moduleId << ": " << message << std::endl;
      message::Buffer buf(message);
      buf.msg.setDestId(moduleId);
      sendHub(message);
   }

   return true;
}

bool ModuleManager::handle(const message::ModuleAvailable &avail) {

   m_stateTracker.handle(avail);

   AvailableModule m;
   m.hub = avail.hub();
   m.name = avail.name();
   m.path = avail.path();
   AvailableModule::Key key(m.hub, m.name);
   m_availableModules[key] = m;
#if 0
   if (Communicator::the().isMaster())
      sendHub(avail);
#endif
   return true;
}

bool ModuleManager::handle(const message::Ping &ping) {

   m_stateTracker.handle(ping);
   sendAll(ping);
   return true;
}

bool ModuleManager::handle(const message::Pong &pong) {

   m_stateTracker.handle(pong);
   //CERR << "Pong [" << pong.senderId() << " " << pong.getCharacter() << "]" << std::endl;
   return true;
}

bool ModuleManager::handle(const message::Trace &trace) {

   m_stateTracker.handle(trace);
   if (trace.module() >= Id::ModuleBase) {
      sendMessage(trace.module(), trace);
   } else if (trace.module() == Id::Broadcast) {
      sendAll(trace);
   }
   return true;
}

bool ModuleManager::handle(const message::Spawn &spawn) {

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
   mod.local = true;
   mod.baseRank = 0;
   mod.hub = spawn.hubId();

   std::string smqName = message::MessageQueue::createName("smq", newId, m_rank);
   std::string rmqName = message::MessageQueue::createName("rmq", newId, m_rank);

   try {
      mod.sendQueue = message::MessageQueue::create(smqName);
      mod.recvQueue = message::MessageQueue::create(rmqName);
   } catch (bi::interprocess_exception &ex) {

      CERR << "spawn mq " << ex.what() << std::endl;
      exit(-1);
   }

   sendHub(message::SpawnPrepared(spawn));

   // inform newly started module about current parameter values of other modules
   for (auto &mit: runningMap) {
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

bool ModuleManager::handle(const message::Started &started) {

   m_stateTracker.handle(started);
   // FIXME: not valid for cover
   //assert(m_stateTracker.getModuleName(moduleID) == started.getName());

   sendAllOthers(started.senderId(), started);
   replayMessages();
   return true;
}

bool ModuleManager::handle(const message::Connect &connect) {

   int modFrom = connect.getModuleA();
   int modTo = connect.getModuleB();
   const char *portFrom = connect.getPortAName();
   const char *portTo = connect.getPortBName();
   const Port *from = m_portManager.getPort(modFrom, portFrom);
   const Port *to = m_portManager.getPort(modTo, portTo);

   message::Connect c = connect;
   if (from && to && from->getType() == Port::INPUT && to->getType() == Port::OUTPUT) {
      c.reverse();
      std::swap(modFrom, modTo);
      std::swap(portFrom, portTo);
   }

   m_stateTracker.handle(c);
   if (m_portManager.addConnection(modFrom, portFrom, modTo, portTo)) {
      // inform modules about connections
      if (Communicator::the().isMaster()) {
         sendMessage(modFrom, c);
         sendMessage(modTo, c);
         sendUi(c);
      }
   } else {
      queueMessage(c);
   }
   return true;
}

bool ModuleManager::handle(const message::Disconnect &disconnect) {

   int modFrom = disconnect.getModuleA();
   int modTo = disconnect.getModuleB();
   const char *portFrom = disconnect.getPortAName();
   const char *portTo = disconnect.getPortBName();
   const Port *from = m_portManager.getPort(modFrom, portFrom);
   const Port *to = m_portManager.getPort(modTo, portTo);

   message::Disconnect d = disconnect;
   if (from->getType() == Port::INPUT && to->getType() == Port::OUTPUT) {
      d.reverse();
      std::swap(modFrom, modTo);
      std::swap(portFrom, portTo);
   }
   
   m_stateTracker.handle(d);
   if (m_portManager.removeConnection(modFrom, portFrom, modTo, portTo)) {

      if (Communicator::the().isMaster()) {
         sendMessage(modFrom, d);
         sendMessage(modTo, d);
         sendUi(d);
      }
   } else {

      if (!m_messageQueue.empty()) {
         // only if messages are already queued, there is a chance that this
         // connection might still be established
         queueMessage(d);
      }
   }

   return true;
}

bool ModuleManager::handle(const message::ModuleExit &moduleExit) {

   sendAllOthers(moduleExit.senderId(), moduleExit);

   int mod = moduleExit.senderId();

   m_portManager.removeConnections(mod);

   if (!moduleExit.isForwarded()) {

      RunningMap::iterator it = runningMap.find(mod);
      if (it == runningMap.end()) {
         CERR << " Module [" << mod << "] quit, but not found in running map" << std::endl;
         return true;
      }
      Module &m = it->second;
      if (m.baseRank == m_rank) {
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
         Module &mod = it->second;
         if (m_rank == mod.baseRank) {
         }
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

bool ModuleManager::handle(const message::Compute &compute) {

   m_stateTracker.handle(compute);
   message::Compute toSend = compute;
   if (compute.getExecutionCount() > currentExecutionCount())
      m_executionCounter = compute.getExecutionCount();
   if (compute.getExecutionCount() < 0)
      toSend.setExecutionCount(newExecutionCount());

   if (compute.getModule() != -1) {
      RunningMap::iterator i = runningMap.find(compute.getModule());
      if (i != runningMap.end()) {
         i->second.sendQueue->send(toSend);
      }
   } else {
      // execute all sources in dataflow graph
      for (auto &mod: runningMap) {
         int id = mod.first;
         auto inputs = m_stateTracker.portTracker()->getInputPorts(id);
         bool isSource = true;
         for (auto &input: inputs) {
            if (!input->connections().empty())
               isSource = false;
         }
         if (isSource) {
            toSend.setModule(id);
            Communicator::the().handleMessage(toSend);
         }
      }
   }

   return true;
}

bool ModuleManager::handle(const message::Reduce &reduce) {

   m_stateTracker.handle(reduce);
   sendMessage(reduce.module(), reduce);
   return true;
}

bool ModuleManager::handle(const message::ExecutionProgress &prog) {

   m_stateTracker.handle(prog);
   RunningMap::iterator i = runningMap.find(prog.senderId());
   if (i == runningMap.end()) {
      return false;
   }

   // initiate reduction if requested by module
   auto &mod = i->second;
   bool forward = true;
   if (mod.reducePolicy != message::ReducePolicy::Never
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

               if (!mod.reducing && mod.reducePolicy != message::ReducePolicy::Never) {
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
      auto out = m_portManager.getOutputPorts(prog.senderId());
      for (Port *port: out) {
         const Port::PortSet *list = NULL;
         if (port) {
            list = m_portManager.getConnectionList(port);
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

bool ModuleManager::handle(const message::Busy &busy) {

   //sendAllOthers(busy.senderId(), busy);
   if (Communicator::the().isMaster()) {
      message::Buffer buf(busy);
      buf.msg.setDestId(Id::MasterHub);
      sendHub(buf.msg);
   }
   return m_stateTracker.handle(busy);
}

bool ModuleManager::handle(const message::Idle &idle) {

   //sendAllOthers(idle.senderId(), idle);
   if (Communicator::the().isMaster()) {
      message::Buffer buf(idle);
      buf.msg.setDestId(Id::MasterHub);
      sendHub(buf.msg);
   }
   return m_stateTracker.handle(idle);
}

bool ModuleManager::handle(const message::AddParameter &addParam) {

   m_stateTracker.handle(addParam);
#ifdef DEBUG
   CERR << "AddParameter: module=" << addParam.moduleName() << "(" << addParam.senderId() << "), name=" << addParam.getName() << std::endl;
#endif

   // let all modules know that a parameter was added
   sendAllOthers(addParam.senderId(), addParam);

   replayMessages();
   return true;
}

bool ModuleManager::handle(const message::SetParameter &setParam) {

   m_stateTracker.handle(setParam);
#ifdef DEBUG
   CERR << "SetParameter: sender=" << setParam.senderId() << ", module=" << setParam.getModule() << ", name=" << setParam.getName() << std::endl;
#endif

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
         queueMessage(setParam);
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
      typedef std::set<Parameter *> ParameterSet;

      std::function<ParameterSet (const Port *, ParameterSet)> findAllConnectedPorts;
      findAllConnectedPorts = [this, &findAllConnectedPorts] (const Port *port, ParameterSet conn) -> ParameterSet {
         if (const Port::PortSet *list = this->m_portManager.getConnectionList(port)) {
            for (auto port: *list) {
               Parameter *param = getParameter(port->getModuleID(), port->getName());
               if (param && conn.find(param) == conn.end()) {
                  conn.insert(param);
                  const Port *port = m_portManager.getPort(param->module(), param->getName());
                  conn = findAllConnectedPorts(port, conn);
               }
            }
         }
         return conn;
      };

      if (Communicator::the().isMaster()) {
         Port *port = m_portManager.getPort(setParam.getModule(), setParam.getName());
         if (port && applied) {
            ParameterSet conn = findAllConnectedPorts(port, ParameterSet());

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

   return true;
}

bool ModuleManager::handle(const message::SetParameterChoices &setChoices) {

   m_stateTracker.handle(setChoices);
   sendAllOthers(setChoices.senderId(), setChoices);
   return true;
}

bool ModuleManager::handle(const message::Kill &kill) {

   m_stateTracker.handle(kill);
   sendMessage(kill.getModule(), kill);
   return true;
}

bool ModuleManager::handle(const message::AddObject &addObj) {

   m_stateTracker.handle(addObj);
   Object::const_ptr obj = addObj.takeObject();
   assert(obj->refcount() >= 1);
#if 0
   std::cerr << "Module " << addObj.senderId() << ": "
      << "AddObject " << addObj.getHandle() << " (" << obj->getName() << ")"
      << " ref " << obj->refcount()
      << " to port " << addObj.getPortName() << std::endl;
#endif

   Port *port = m_portManager.getPort(addObj.senderId(), addObj.getPortName());
   const Port::PortSet *list = NULL;
   if (port) {
      list = m_portManager.getConnectionList(port);
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

         auto it = runningMap.find(destId);
         if (it == runningMap.end()) {
            CERR << "port connection to module that is not running" << std::endl;
            assert("port connection to module that is not running" == 0);
            continue;
         }

         Module &destMod = it->second;

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

bool ModuleManager::handle(const message::ObjectReceived &objRecv) {

   m_stateTracker.handle(objRecv);
   sendMessage(objRecv.senderId(), objRecv);
   return true;
}

bool ModuleManager::handle(const message::Barrier &barrier) {

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

bool ModuleManager::handle(const message::BarrierReached &barrReached) {

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

bool ModuleManager::handle(const message::CreatePort &createPort) {

   m_stateTracker.handle(createPort);
   replayMessages();

   // let all modules know that a port was created
   sendAllOthers(createPort.senderId(), createPort);

   return true;
}

bool ModuleManager::handle(const vistle::message::ResetModuleIds &reset)
{
   m_stateTracker.handle(reset);
   if (!runningMap.empty()) {
      CERR << "ResetModuleIds: " << numRunning() << " modules still running" << std::endl;
      return true;
   }

   resetModuleCounter();
   sendUi(message::ReplayFinished());
   return true;
}

bool ModuleManager::handle(const message::ObjectReceivePolicy &receivePolicy)
{
   const int id = receivePolicy.senderId();
   RunningMap::iterator it = runningMap.find(id);
   if (it == runningMap.end()) {
      CERR << " Module [" << id << "] changed ObjectReceivePolicy, but not found in running map" << std::endl;
      return false;
   }
   Module &mod = it->second;
   mod.objectPolicy = receivePolicy.policy();
   return true;
}

bool ModuleManager::handle(const message::SchedulingPolicy &schedulingPolicy)
{
   const int id = schedulingPolicy.senderId();
   RunningMap::iterator it = runningMap.find(id);
   if (it == runningMap.end()) {
      CERR << " Module [" << id << "] changed SchedulingPolicy, but not found in running map" << std::endl;
      return false;
   }
   Module &mod = it->second;
   mod.schedulingPolicy = schedulingPolicy.policy();
   return true;
}

bool ModuleManager::handle(const message::ReducePolicy &reducePolicy)
{
   const int id = reducePolicy.senderId();
   RunningMap::iterator it = runningMap.find(id);
   if (it == runningMap.end()) {
      CERR << " Module [" << id << "] changed ReducePolicy, but not found in running map" << std::endl;
      return false;
   }
   Module &mod = it->second;
   mod.reducePolicy = reducePolicy.policy();
   return true;
}

bool ModuleManager::quit() {

   if (!m_quitFlag)
      sendAll(message::Kill(-1));

   // receive all ModuleExit messages from modules
   // retry for some time, modules that don't answer might have crashed
   CERR << "waiting for " << numRunning() << " modules to quit" << std::endl;

   if (m_size > 1)
      MPI_Barrier(MPI_COMM_WORLD);

   m_quitFlag = true;

   return numRunning()==0;
}

bool ModuleManager::quitOk() const {

   return m_quitFlag && numRunning()==0;
}

ModuleManager::~ModuleManager() {

}

std::vector<char> ModuleManager::getState() const {

   return m_stateTracker.getState();
}

const PortManager &ModuleManager::portManager() const {

   return m_portManager;
}

std::vector<std::string> ModuleManager::getParameters(int id) const {

   return m_stateTracker.getParameters(id);
}

Parameter *ModuleManager::getParameter(int id, const std::string &name) const {

   return m_stateTracker.getParameter(id, name);
}

void ModuleManager::queueMessage(const message::Message &msg) {

   const char *m = static_cast<const char *>(static_cast<const void *>(&msg));
   std::copy(m, m+message::Message::MESSAGE_SIZE, std::back_inserter(m_messageQueue));
   //CERR << "queueing " << msg.type() << ", now " << m_messageQueue.size()/message::Message::MESSAGE_SIZE << " in queue" << std::endl;
}

void ModuleManager::replayMessages() {

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

int ModuleManager::numRunning() const {

   int n = 0;
   for (auto &m: runningMap) {
      int state = m_stateTracker.getModuleState(m.first);
      if ((state & StateObserver::Initialized) && !(state & StateObserver::Killed))
         ++n;
   }
   return n;
}

} // namespace vistle
