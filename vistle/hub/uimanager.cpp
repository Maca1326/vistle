/*
 * Visualization Testing Laboratory for Exascale Computing (VISTLE)
 */
#include <core/statetracker.h>
#include <core/messagequeue.h>
#include <core/tcpmessage.h>

#include "uimanager.h"
#include "uiclient.h"
#include "hub.h"

namespace vistle {

UiManager::UiManager(Hub &hub, StateTracker &stateTracker)
: m_stateTracker(stateTracker)
, m_requestQuit(false)
, m_locked(false)
{
}

UiManager::~UiManager() {

   disconnect();
}

void UiManager::sendMessage(const message::Message &msg) const {

   for(auto ent: m_clients) {
      sendMessage(ent, msg);
   }
}

void UiManager::sendMessage(boost::shared_ptr<UiClient> c, const message::Message &msg) const {

   auto &ioService = c->socket().get_io_service();
   message::send(c->socket(), msg);
   ioService.poll();
}

void UiManager::requestQuit() {

   m_requestQuit = true;
   sendMessage(message::Quit());
}

void UiManager::disconnect() {

   sendMessage(message::Quit());

   for(const auto &c: m_clients) {
      c->cancel();
   }

   join();
   if (!m_clients.empty()) {
      std::cerr << "UiManager: waiting for " << m_clients.size() << " clients to quit" << std::endl;
      sleep(1);
      join();
   }

}

void UiManager::addClient(boost::shared_ptr<UiClient> c) {

   m_clients.insert(c);

   if (m_requestQuit) {

      sendMessage(c, message::Quit());
   } else {

      sendMessage(c, message::SetId(c->id()));

      sendMessage(c, message::LockUi(m_locked));

      auto avail = m_stateTracker.availableModules();
      for(const auto &mod: avail) {
         sendMessage(c, message::ModuleAvailable(mod.hub, mod.name, mod.path));
      }

      std::vector<char> state = m_stateTracker.getState();
      for (size_t i=0; i<state.size(); i+=message::Message::MESSAGE_SIZE) {
         const message::Message *msg = (const message::Message *)&state[i];
         sendMessage(c, *msg);
      }

      sendMessage(c, message::ReplayFinished());
   }
}

void UiManager::join() {

   m_clients.clear();
}

void UiManager::lockUi(bool locked) {

   if (m_locked != locked) {
      sendMessage(message::LockUi(locked));
      m_locked = locked;
   }
}

bool UiManager::isLocked() const {

   return m_locked;
}

} // namespace vistle
