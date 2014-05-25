/*
 * Visualization Testing Laboratory for Exascale Computing (VISTLE)
 */

#include <iostream>
#include <fstream>
#include <exception>
#include <cstdlib>
#include <sstream>

#include <util/findself.h>
#include <util/spawnprocess.h>
#include <util/sleep.h>
#include <core/object.h>
#include <control/executor.h>
#include <core/message.h>
#include <core/tcpmessage.h>
#include <core/porttracker.h>
#include <core/statetracker.h>
#include <core/shm.h>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>

#include "uimanager.h"
#include "uiclient.h"
#include "hub.h"

#ifdef HAVE_PYTHON
#include "pythoninterpreter.h"
#endif

namespace asio = boost::asio;
using boost::shared_ptr;
using namespace vistle;
using message::Router;
using message::Id;

#define CERR std::cerr << "Hub: "

std::string hostname() {

   static std::string hname;

   if (hname.empty()) {
      const size_t HOSTNAMESIZE = 256;

      char hostname[HOSTNAMESIZE];
      gethostname(hostname, HOSTNAMESIZE-1);
      hostname[HOSTNAMESIZE-1] = '\0';
      hname = hostname;
   }
   return hname;
}

Hub *hub_instance = nullptr;

Hub::Hub()
: m_port(31093)
, m_acceptor(m_ioService)
, m_stateTracker("Hub state")
, m_uiManager(*this, m_stateTracker)
, m_uiCount(0)
, m_managerConnected(false)
, m_quitting(false)
, m_isMaster(true)
, m_slaveCount(0)
, m_hubId(Id::Invalid)
, m_moduleCount(0)
, m_traceMessages(-1)
{

   assert(!hub_instance);
   hub_instance = this;

   message::DefaultSender::init(m_hubId, 0);
}

Hub::~Hub() {

   hub_instance = nullptr;
}

Hub &Hub::the() {

   return *hub_instance;
}

const StateTracker &Hub::stateTracker() const {

   return m_stateTracker;
}

StateTracker &Hub::stateTracker() {

   return m_stateTracker;
}

unsigned short Hub::port() const {

   return m_port;
}

bool Hub::sendMessage(shared_ptr<socket> sock, const message::Message &msg) {

   return message::send(*sock, msg);
}

bool Hub::startServer() {

   while (!m_acceptor.is_open()) {

      asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v6(), m_port);
      m_acceptor.open(endpoint.protocol());
      m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
      try {
         m_acceptor.bind(endpoint);
      } catch(const boost::system::system_error &err) {
         if (err.code() == boost::system::errc::address_in_use) {
            m_acceptor.close();
            ++m_port;
            continue;
         }
         throw(err);
      }
      m_acceptor.listen();
      CERR << "listening for connections on port " << m_port << std::endl;
      startAccept();
   }

   return true;
}

bool Hub::startAccept() {
   
   shared_ptr<asio::ip::tcp::socket> sock(new asio::ip::tcp::socket(m_ioService));
   addSocket(sock);
   m_acceptor.async_accept(*sock, boost::bind<void>(&Hub::handleAccept, this, sock, asio::placeholders::error));
   return true;
}

void Hub::handleAccept(shared_ptr<asio::ip::tcp::socket> sock, const boost::system::error_code &error) {

   if (error) {
      removeSocket(sock);
      return;
   }

   addClient(sock);

   message::Identify ident;
   sendMessage(sock, ident);

   startAccept();
}

void Hub::addSocket(shared_ptr<asio::ip::tcp::socket> sock, message::Identify::Identity ident) {

   bool ok = m_sockets.insert(std::make_pair(sock, ident)).second;
   assert(ok);
}

bool Hub::removeSocket(shared_ptr<asio::ip::tcp::socket> sock) {

   return m_sockets.erase(sock) > 0;
}

void Hub::addClient(shared_ptr<asio::ip::tcp::socket> sock) {

   //CERR << "new client" << std::endl;
   m_clients.insert(sock);
}

void Hub::addSlave(int id, shared_ptr<asio::ip::tcp::socket> sock) {

   const int slaveid = Id::MasterHub - id;
   m_slaveSockets[slaveid] = sock;

   message::SetId set(slaveid);
   sendMessage(sock, set);
   for (auto &am: m_availableModules) {
      message::ModuleAvailable m(m_hubId, am.second.name, am.second.path);
      sendMessage(sock, m);
   }
}

bool Hub::dispatch() {

   m_ioService.poll();

   bool ret = true;
   bool work = false;
   for (auto &sock: m_clients) {

      message::Identify::Identity senderType = message::Identify::UNKNOWN;
      auto it = m_sockets.find(sock);
      if (it != m_sockets.end())
         senderType = it->second;
      boost::asio::socket_base::bytes_readable command(true);
      sock->io_control(command);
      if (command.get() > 0) {
         char buf[message::Message::MESSAGE_SIZE];
         message::Message &msg = *reinterpret_cast<message::Message *>(buf);
         bool received = false;
         if (message::recv(*sock, msg, received) && received) {
            work = true;
            if (!handleMessage(sock, msg)) {
               ret = false;
               break;
            }
         }
      }
   }

   if (auto pid = vistle::try_wait()) {
      work = true;
      auto it = m_processMap.find(pid);
      if (it == m_processMap.end()) {
         CERR << "unknown process with PID " << pid << " exited" << std::endl;
      } else {
         CERR << "process with id " << it->second << " (PID " << pid << ") exited" << std::endl;
         m_processMap.erase(it);
      }
   }

   if (m_quitting) {
      if (m_processMap.empty()) {
         ret = false;
      } else {
         CERR << "still " << m_processMap.size() << " processes running" << std::endl;
         for (const auto &proc: m_processMap) {
            std::cerr << "   id: " << proc.second << ", pid: " << proc.first << std::endl;
         }
      }
   }

   vistle::adaptive_wait(work);

   return ret;
}

bool Hub::sendMaster(const message::Message &msg) {

   assert(!m_isMaster);
   if (m_isMaster) {
      return false;
   }

   int numSent = 0;
   for (auto &sock: m_sockets) {
      if (sock.second == message::Identify::HUB) {
         ++numSent;
         sendMessage(sock.first, msg);
      }
   }
   assert(numSent == 1);
   return numSent == 1;
}

bool Hub::sendManager(const message::Message &msg) {

   if (!m_managerConnected)
      return false;

   int numSent = 0;
   for (auto &sock: m_sockets) {
      if (sock.second == message::Identify::MANAGER) {
         ++numSent;
         sendMessage(sock.first, msg);
      }
   }
   assert(numSent == 1);
   return numSent == 1;
}

bool Hub::sendSlaves(const message::Message &msg) {

   assert(m_isMaster);
   if (!m_isMaster)
      return false;

   int senderHub = msg.senderId();
   if (senderHub > 0) {
      std::cerr << "mod id " << senderHub;
      senderHub = m_stateTracker.getHub(senderHub);
      std::cerr << " -> hub id " << senderHub << std::endl;
   }

   for (auto &sock: m_slaveSockets) {
      if (sock.first != senderHub) {
         std::cerr << "to slave id: " << sock.first << " (!= " << senderHub << ")" << std::endl;
         sendMessage(sock.second, msg);
      }
   }
   return true;
}

bool Hub::sendSlave(const message::Message &msg, int dest) {

   assert(m_isMaster);
   if (!m_isMaster)
      return false;

   auto it = m_slaveSockets.find(dest);
   if (it == m_slaveSockets.end())
      return false;

   return sendMessage(it->second, msg);
}

bool Hub::sendUi(const message::Message &msg) {

   m_uiManager.sendMessage(msg);
   return true;
}

bool Hub::handleUiMessage(const message::Message &msg) {

   return sendMaster(msg);
}

int Hub::idToHub(int id) const {

   if (id >= Id::ModuleBase)
      return m_stateTracker.getHub(id);

   return id;
}

bool Hub::handleMessage(shared_ptr<asio::ip::tcp::socket> sock, const message::Message &msg) {

   using namespace vistle::message;

   message::Identify::Identity senderType = message::Identify::UNKNOWN;
   auto it = m_sockets.find(sock);
   if (sock) {
      assert(it != m_sockets.end());
      senderType = it->second;
   }

   bool mgr=false, ui=false, master=false, slave=false, handle=false;

   bool track = Router::the().toTracker(msg, senderType);
   m_stateTracker.handle(msg, track);

   if (Router::the().toManager(msg, senderType)) {
      sendManager(msg);
      mgr = true;
   }
   if (Router::the().toUi(msg, senderType)) {
      sendUi(msg);
      ui = true;
   }
   const int dest = idToHub(msg.destId());
   const int sender = idToHub(msg.senderId());
   if (dest > Id::MasterHub) {
      if (Router::the().toMasterHub(msg, senderType, sender)) {
         sendMaster(msg);
         master = true;
      }
      if (Router::the().toSlaveHub(msg, senderType, sender)) {
         sendSlaves(msg);
         slave = true;
      }
      assert(!(slave && master));
   } else {
      if (dest != m_hubId) {
         if (m_isMaster) {
            sendSlaves(msg);
            slave = true;
         }
      }
   }

   if (Router::the().toHandler(msg, senderType)) {

      handle=true;

      switch(msg.type()) {

         case Message::SPAWN: {
            auto &spawn = static_cast<const Spawn &>(msg);
            if (m_isMaster) {
               assert(spawn.spawnId() == Id::Invalid);
               auto notify = spawn;
               notify.setSenderId(m_hubId);
               notify.setSpawnId(Id::ModuleBase + m_moduleCount);
               ++m_moduleCount;
               notify.setDestId(spawn.hubId());
               m_stateTracker.handle(notify);
               sendUi(notify);
               sendManager(notify);
               sendSlaves(notify);
            } else {
               assert(spawn.spawnId() >= Id::ModuleBase);
               sendManager(spawn);
            }
            break;
         }

         case Message::SPAWNPREPARED: {

            auto &spawn = static_cast<const SpawnPrepared &>(msg);
            assert(spawn.hubId() == m_hubId);

            std::string name = spawn.getName();
            AvailableModule::Key key(spawn.hubId(), name);
            auto it = m_availableModules.find(key);
            if (it == m_availableModules.end()) {
               CERR << "refusing to spawn " << name << ": not in list of available modules" << std::endl;
               return true;
            }
            std::string path = it->second.path;

            std::string executable = path;
            std::vector<std::string> argv;
            argv.push_back("spawn_vistle.sh");
            argv.push_back(executable);
            argv.push_back(Shm::instanceName(hostname(), m_port));
            argv.push_back(boost::lexical_cast<std::string>(spawn.spawnId()));

            auto pid = spawn_process("spawn_vistle.sh", argv);
            if (pid) {
               //std::cerr << "started " << executable << " with PID " << pid << std::endl;
               m_processMap[pid] = spawn.spawnId();
            } else {
               std::cerr << "program " << executable << " failed to start" << std::endl;
            }
            break;
         }
         case Message::IDENTIFY: {
            auto &id = static_cast<const Identify &>(msg);
            CERR << "ident msg: " << id.identity() << std::endl;
            if (id.identity() != Identify::UNKNOWN) {
               it->second = id.identity();
            }
            switch(id.identity()) {
               case Identify::UNKNOWN: {
                  if (m_isMaster) {
                     sendMessage(sock, Identify(Identify::HUB));
                  } else {
                     sendMessage(sock, Identify(Identify::SLAVEHUB));
                  }
                  break;
               }
               case Identify::MANAGER: {
                  assert(!m_managerConnected);
                  m_managerConnected = true;

                  if (m_hubId != Id::Invalid) {
                     message::SetId set(m_hubId);
                     sendMessage(sock, set);
                     if (m_hubId < Id::MasterHub) {
                        for (auto &am: m_availableModules) {
                           message::ModuleAvailable m(m_hubId, am.second.name, am.second.path);
                           sendMessage(sock, m);
                        }
                     }
                  }
                  if (m_isMaster) {
                     processScript();
                  }
                  break;
               }
               case Identify::UI: {
                  ++m_uiCount;
                  boost::shared_ptr<UiClient> c(new UiClient(m_uiManager, m_uiCount, sock));
                  m_uiManager.addClient(c);
                  break;
               }
               case Identify::HUB: {
                  assert(!m_isMaster);
                  CERR << "master hub connected" << std::endl;
                  break;
               }
               case Identify::SLAVEHUB: {
                  assert(m_isMaster);
                  CERR << "slave hub connected" << std::endl;
                  ++m_slaveCount;
                  addSlave(m_slaveCount, sock);
                  break;
               }
            }
            break;
         }

         case Message::SETID: {

            assert(!m_isMaster);
            auto &set = static_cast<const SetId &>(msg);
            m_hubId = set.getId();
            message::DefaultSender::init(m_hubId, 0);
            Router::init(message::Identify::SLAVEHUB, m_hubId);
            CERR << "got hub id " << m_hubId << std::endl;
            if (m_managerConnected) {
               sendManager(set);
            }
            scanModules(m_bindir + "/../libexec/module", m_hubId, m_availableModules);
            for (auto &am: m_availableModules) {
               message::ModuleAvailable m(m_hubId, am.second.name, am.second.path);
               sendManager(m);
               m.setDestId(Id::MasterHub);
               sendMaster(m);
            }
            break;
         }

         case Message::EXEC: {

            auto &exec = static_cast<const Exec &>(msg);
            if (exec.destId() == m_hubId) {
               std::string executable = exec.pathname();
               auto args = exec.args();
               std::vector<std::string> argv;
               argv.push_back("spawn_vistle.sh");
               argv.push_back(executable);
               for (auto &a: args) {
                  argv.push_back(a);
               }
               auto pid = spawn_process("spawn_vistle.sh", argv);
               if (pid) {
                  //std::cerr << "started " << executable << " with PID " << pid << std::endl;
                  m_processMap[pid] = exec.moduleId();
               } else {
                  std::cerr << "program " << executable << " failed to start" << std::endl;
               }
            } else {
               sendSlaves(exec);
            }
            break;
         }

         case Message::QUIT: {

            //std::cerr << "hub: broadcasting message: " << msg << std::endl;
            auto &quit = static_cast<const Quit &>(msg);
            if (senderType == message::Identify::MANAGER) {
               m_uiManager.requestQuit();
               if (m_isMaster)
                  sendSlaves(quit);
               m_quitting = true;
               return true;
            } else if (senderType == message::Identify::HUB) {
               m_uiManager.requestQuit();
               sendManager(quit);
               m_quitting = true;
               return true;
            } else {
               sendSlaves(quit);
            }
            break;
         }

         default: {
            break;
         }

      }
   }

   if (m_traceMessages == -1 || msg.type() == m_traceMessages) {
      if (track) std::cerr << "t"; else std::cerr << ".";
      if (mgr) std::cerr << "m" ;else std::cerr << ".";
      if (ui) std::cerr << "u"; else std::cerr << ".";
      if (slave) { std::cerr << "s"; } else if (master) { std::cerr << "M"; } else std::cerr << ".";
      std::cerr << " " << msg << std::endl;
   }

   return true;
}

bool Hub::init(int argc, char *argv[]) {

   m_bindir = getbindir(argc, argv);

   namespace po = boost::program_options;
   po::options_description desc("usage");
   desc.add_options()
      ("help,h", "show this message")
      ("hub,c", po::value<std::string>(), "connect to hub")
      ("batch,b", "do not start user interface")
      ("gui,g", "start graphical user interface")
      ("tui,t", "start command line interface")
      ("filename", "Vistle script to process")
      ;
   po::variables_map vm;
   try {
      po::positional_options_description popt;
      popt.add("filename", 1);
      po::store(po::command_line_parser(argc, argv).options(desc).positional(popt).run(), vm);
      po::notify(vm);
   } catch (std::exception &e) {
      CERR << e.what() << std::endl;
      CERR << desc << std::endl;
      return false;
   }

   if (vm.count("help")) {
      CERR << desc << std::endl;
      return false;
   }

   if (vm.count("") > 0) {
      CERR << desc << std::endl;
      return false;
   }

   startServer();

   std::string uiCmd = "vistle_gui";

   std::string masterhost;
   unsigned short masterport = 31093;
   if (vm.count("hub") > 0) {
      uiCmd.clear();
      m_isMaster = false;
      masterhost = vm["hub"].as<std::string>();
      auto colon = masterhost.find(':');
      if (colon != std::string::npos) {
         masterport = boost::lexical_cast<unsigned short>(masterhost.substr(colon+1));
         masterhost = masterhost.substr(0, colon);
      }

      if (!connectToMaster(masterhost, masterport)) {
         CERR << "failed to connect to master at " << masterhost << ":" << masterport << std::endl;
         return false;
      }
   } else {
      // this is the master hub
      m_hubId = Id::MasterHub;
      message::DefaultSender::init(m_hubId, 0);
      Router::init(message::Identify::HUB, m_hubId);

#ifdef SCAN_MODULES_ON_HUB
      scanModules(m_bindir + "/../libexec/module", m_hubId, m_availableModules);
      for (auto &am: m_availableModules) {
         message::ModuleAvailable m(m_hubId, am.second.name, am.second.path);
         m_stateTracker.handle(m);
      }
#endif
   }

   // start UI
   if (const char *pbs_env = getenv("PBS_ENVIRONMENT")) {
      if (std::string("PBS_INTERACTIVE") != pbs_env) {
         CERR << "apparently running in PBS batch mode - defaulting to no UI" << std::endl;
         uiCmd.clear();
      }
   }
   if (vm.count("gui")) {
      uiCmd = "vistle_gui";
   } else if (vm.count("tui")) {
      uiCmd = "blower";
   }
   if (vm.count("batch")) {
      uiCmd.clear();
   }

   if (!uiCmd.empty()) {
      std::string uipath = m_bindir + "/" + uiCmd;
      startUi(uipath);
   }

   if (vm.count("filename") == 1) {
      m_scriptPath = vm["filename"].as<std::string>();
   }

   std::string port = boost::lexical_cast<std::string>(this->port());

   // start manager on cluster
   std::string cmd = m_bindir + "/vistle_manager";
   std::vector<std::string> args;
   args.push_back("spawn_vistle.sh");
   args.push_back(cmd);
   args.push_back(hostname());
   args.push_back(port);
   auto pid = vistle::spawn_process("spawn_vistle.sh", args);
   if (!pid) {
      CERR << "failed to spawn Vistle manager " << std::endl;
      exit(1);
   }
   m_processMap[pid] = 0;

   return true;
}

bool Hub::connectToMaster(const std::string &host, unsigned short port) {

   assert(!m_isMaster);

   asio::ip::tcp::resolver resolver(m_ioService);
   asio::ip::tcp::resolver::query query(host, boost::lexical_cast<std::string>(port));
   asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
   boost::system::error_code ec;
   m_masterSocket.reset(new boost::asio::ip::tcp::socket(m_ioService));
   asio::connect(*m_masterSocket, endpoint_iterator, ec);
   if (ec) {
      CERR << "could not establish connection to " << host << ":" << port << std::endl;
      return false;
   }

#if 0
   message::Identify ident(message::Identify::SLAVEHUB);
   sendMessage(m_masterSocket, ident);
#endif
   addSocket(m_masterSocket, message::Identify::HUB);
   addClient(m_masterSocket);

   CERR << "connected to master at " << host << ":" << port << std::endl;
   return true;
}

bool Hub::startUi(const std::string &uipath) {

   std::string port = boost::lexical_cast<std::string>(this->port());

   std::vector<std::string> args;
   args.push_back(uipath);
   args.push_back("-from-vistle");
   args.push_back("localhost");
   args.push_back(port);
   auto pid = vistle::spawn_process(uipath, args);
   if (!pid) {
      CERR << "failed to spawn UI " << uipath << std::endl;
      return false;
   }

   m_processMap[pid] = 0;

   return true;
}

bool Hub::processScript() {

#ifdef HAVE_PYTHON
   m_uiManager.lockUi(true);
   if (!m_scriptPath.empty()) {
      PythonInterpreter inter(m_scriptPath);
      while(inter.check()) {
         dispatch();
      }
   }
   m_uiManager.lockUi(false);
   return true;
#else
   return false;
#endif
}

int main(int argc, char *argv[]) {

   Hub hub;
   if (!hub.init(argc, argv)) {
      return 1;
   }

   while(hub.dispatch())
      ;

   return 0;
}
