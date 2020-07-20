#include <vistle/core/message.h>
#include <vistle/core/messagequeue.h>
#include <vistle/core/statetracker.h>
#include <vistle/core/object.h>
#include <vistle/core/empty.h>
#include <vistle/core/placeholder.h>
#include <vistle/core/coords.h>
#include <vistle/core/archives.h>
#include <vistle/core/archive_loader.h>
#include <vistle/core/archive_saver.h>

#include "renderer.h"

#include <vistle/util/vecstreambuf.h>
#include <vistle/util/sleep.h>
#include <vistle/util/stopwatch.h>

namespace mpi = boost::mpi;

namespace vistle {

Renderer::Renderer(const std::string &description,
                   const std::string &name, const int moduleID, mpi::communicator comm)
   : Module(description, name, moduleID, comm)
   , m_fastestObjectReceivePolicy(message::ObjectReceivePolicy::Local)
{

   setSchedulingPolicy(message::SchedulingPolicy::Ignore); // compute does not have to be called at all
   setReducePolicy(message::ReducePolicy::Never); // because of COMBINE port
   createInputPort("data_in", "input data", Port::COMBINE);

   m_renderMode = addIntParameter("render_mode", "Render on which nodes?", LocalOnly, Parameter::Choice);
   V_ENUM_SET_CHOICES(m_renderMode, RenderMode);

   m_objectsPerFrame = addIntParameter("objects_per_frame", "Max. no. of objects to load between calls to render", m_numObjectsPerFrame);
   setParameterMinimum(m_objectsPerFrame, Integer(1));

   //std::cerr << "Renderer starting: rank=" << rank << std::endl;
}

Renderer::~Renderer() {

}

// all of those messages have to arrive in the same order an all ranks, but other messages may be interspersed
bool Renderer::needsSync(const message::Message &m) const {

   using namespace vistle::message;

   switch (m.type()) {
      case CANCELEXECUTE:
      case QUIT:
      case KILL:
      case ADDPARAMETER:
      case SETPARAMETER:
      case REMOVEPARAMETER:
         return true;
      case ADDOBJECT:
         return objectReceivePolicy() != ObjectReceivePolicy::Local;
      default:
         return false;
   }
}

std::array<Object::const_ptr,3> splitObject(Object::const_ptr container) {

    std::array<Object::const_ptr,3> geo_norm_data;
    Object::const_ptr &grid = geo_norm_data[0];
    Object::const_ptr &normals = geo_norm_data[1];
    Object::const_ptr &tex = geo_norm_data[2];

    if (auto ph = vistle::PlaceHolder::as(container)) {
        grid = ph->geometry();
        normals = ph->normals();
        tex = ph->texture();
    } else if (auto t = vistle::Texture1D::as(container)) {
        if (auto g = vistle::Coords::as(t->grid())) {
            grid = g;
            normals = g->normals();
            tex = t;
        }
    } else if (auto g = vistle::Coords::as(container)) {
        grid = g;
        normals = g->normals();
    } else if (auto data = vistle::DataBase::as(container)) {
        tex = data;
        grid = data->grid();
        if (auto g = vistle::Coords::as(data->grid())) {
            normals = g->normals();
        }
    } else {
        grid = container;
    }

    return geo_norm_data;
}

bool Renderer::handleMessage(const message::Message *message, const MessagePayload &payload) {

    switch (message->type()) {
    case vistle::message::ADDOBJECT: {
        auto add = static_cast<const message::AddObject *>(message);
        if (payload)
            m_stateTracker->handle(*add, payload->data(), payload->size());
        else
            m_stateTracker->handle(*add, nullptr);
        return handleAddObject(*add);
        break;
    }
    default: {
        break;
    }
    }

    return Module::handleMessage(message, payload);
}

bool Renderer::addColorMap(const std::string &species, Texture1D::const_ptr texture) {

    return true;
}

bool Renderer::removeColorMap(const std::string &species) {

    return true;
}

bool Renderer::handleAddObject(const message::AddObject &add) {

    using namespace vistle::message;
    auto pol = objectReceivePolicy();

    Object::const_ptr obj, placeholder;
    std::vector<Object::const_ptr> objs;
    if (add.rank() == rank()) {
        obj = add.takeObject();
        assert(obj);

        auto geo_norm_tex = splitObject(obj);
        auto &grid = geo_norm_tex[0];
        auto &normals = geo_norm_tex[1];
        auto &tex = geo_norm_tex[2];

        if (size() > 1 && pol == ObjectReceivePolicy::Distribute) {
            auto ph = std::make_shared<PlaceHolder>(obj);
            ph->setGeometry(grid);
            ph->setNormals(normals);
            ph->setTexture(tex);
            placeholder = ph;
        }
    }

    RenderMode rm = static_cast<RenderMode>(m_renderMode->getValue());
    if (size() > 1) {
        if (rm == AllNodes) {
            assert(pol == ObjectReceivePolicy::Distribute);
            broadcastObject(obj, add.rank());
            assert(obj);
        } else if (pol == ObjectReceivePolicy::Distribute) {
            broadcastObject(placeholder, add.rank());
            assert(placeholder);
        }
        if (rm == MasterOnly) {
            if (rank() == 0) {
                if (add.rank() != 0)
                    obj = receiveObject(add.rank());
            } else if (rank() == add.rank()) {
                sendObject(obj, 0);
            }
        }
    }

    if (rm == AllNodes || (rm == MasterOnly && rank() == 0) || (rm != MasterOnly && rank() == add.rank())) {
        assert(obj);
        addInputObject(add.senderId(), add.getSenderPort(), add.getDestPort(), obj);
    } else if (pol == ObjectReceivePolicy::Distribute) {
        assert(placeholder);
        addInputObject(add.senderId(), add.getSenderPort(), add.getDestPort(), placeholder);
    }

    return true;
}

bool Renderer::dispatch(bool block, bool *messageReceived) {
   (void)block;
   int quit = 0;
   bool checkAgain = false;
   int numSync = 0;
   do {
      // process all messages until one needs cooperative processing
      message::Buffer buf;
      message::Message &message = buf;
      bool haveMessage = getNextMessage(buf, false);
      int needSync = 0;
      if (haveMessage) {
         if (needsSync(message))
            needSync = 1;
      }
      int anySync = boost::mpi::all_reduce(comm(), needSync, boost::mpi::maximum<int>());
      if (m_maySleep)
          vistle::adaptive_wait(haveMessage || anySync, this);

      do {
         if (haveMessage) {
            if (messageReceived)
               *messageReceived = true;

            MessagePayload pl;
            if (buf.payloadSize() > 0) {
                pl = Shm::the().getArrayFromName<char>(buf.payloadName());
                pl.unref();
            }
            quit = !handleMessage(&message, pl);
            if (quit) {
                std::cerr << "Quitting: " << message << std::endl;
                break;
            }

            if (needsSync(message))
               needSync = 1;
         }

         if (anySync && !needSync) {
            haveMessage = getNextMessage(buf);
         }

      } while(anySync && !needSync);

      int anyQuit = boost::mpi::all_reduce(comm(), quit, boost::mpi::maximum<int>());
      if (anyQuit) {
          prepareQuit();
          return false;
      }

      int numMessages = messageBacklog.size() + receiveMessageQueue->getNumMessages();
      int maxNumMessages = boost::mpi::all_reduce(comm(), numMessages, boost::mpi::maximum<int>());
      ++numSync;
      checkAgain = maxNumMessages>0 && numSync<m_numObjectsPerFrame;
   } while (checkAgain);

   double start = 0.;
   if (m_benchmark) {
       comm().barrier();
       start = Clock::time();
   }
   if (render() && m_benchmark) {
       comm().barrier();
       const double duration = Clock::time() - start;
       if (rank() == 0) {
           sendInfo("render took %f s", duration);
       }
   }

   return true;
}

int Renderer::numTimesteps() const {

    if (m_objectList.size() <= 1)
        return 0;

    return m_objectList.size() - 1;
}


bool Renderer::addInputObject(int sender, const std::string &senderPort, const std::string & portName,
                                 vistle::Object::const_ptr object) {

   int creatorId = object->getCreator();
   CreatorMap::iterator it = m_creatorMap.find(creatorId);
   if (it != m_creatorMap.end()) {
      if (it->second.age < object->getExecutionCounter()) {
         //std::cerr << "removing all created by " << creatorId << ", age " << object->getExecutionCounter() << ", was " << it->second.age << std::endl;
         removeAllCreatedBy(creatorId);
      } else if (it->second.age == object->getExecutionCounter() && it->second.iter < object->getIteration()) {
         std::cerr << "removing all created by " << creatorId << ", age " << object->getExecutionCounter() << ": new iteration " << object->getIteration() << std::endl;
         removeAllCreatedBy(creatorId);
      } else if (it->second.age > object->getExecutionCounter()) {
         std::cerr << "received outdated object created by " << creatorId << ", age " << object->getExecutionCounter() << ", was " << it->second.age << std::endl;
         return false;
      } else if (it->second.age == object->getExecutionCounter() && it->second.iter > object->getIteration()) {
         std::cerr << "received outdated object created by " << creatorId << ", age " << object->getExecutionCounter() << ": old iteration " << object->getIteration() << std::endl;
         return false;
      }
   } else {
      std::string name = getModuleName(object->getCreator());
      it = m_creatorMap.insert(std::make_pair(creatorId, Creator(object->getCreator(), name))).first;
   }
   Creator &creator = it->second;
   creator.age = object->getExecutionCounter();
   creator.iter = object->getIteration();

   if (Empty::as(object))
       return true;

   auto geo_norm_tex = splitObject(object);

   if (!geo_norm_tex[0]) {
       std::string species = object->getAttribute("_species");
       if (auto tex = vistle::Texture1D::as(object)) {
           auto &cmap = m_colormaps[species];
           cmap.texture = tex;
           cmap.creator = object->getCreator();
           cmap.sender = sender;
           cmap.senderPort = senderPort;
            std::cerr << "added colormap " << species << " without object, width=" << tex->getWidth() << ", range=" << tex->getMin() << " to " << tex->getMax() << std::endl;
            return addColorMap(species, tex);
        }
   }

   std::shared_ptr<RenderObject> ro = addObjectWrapper(sender, senderPort,
                                                       object, geo_norm_tex[0], geo_norm_tex[1], geo_norm_tex[2]);
   if (ro) {
      assert(ro->timestep >= -1);
      if (m_objectList.size() <= size_t(ro->timestep+1))
         m_objectList.resize(ro->timestep+2);
      m_objectList[ro->timestep+1].push_back(ro);
   }

#if 1
   std::string variant;
   std::string noobject;
   if (ro) {
       if (!ro->variant.empty())
           variant = " variant " + ro->variant;
   } else {
       noobject = " NO OBJECT generated";
   }
   std::cerr << "++++++Renderer addInputObject " << object->getName()
             << " type " << object->getType()
             << " creator " << object->getCreator()
             << " exec " << object->getExecutionCounter()
             << " iter " << object->getIteration()
             << " block " << object->getBlock()
             << " timestep " << object->getTimestep()
             << variant
             << noobject
             << std::endl;
#endif

   return true;
}

std::shared_ptr<RenderObject> Renderer::addObjectWrapper(int senderId, const std::string &senderPort, Object::const_ptr container, Object::const_ptr geom, Object::const_ptr normal, Object::const_ptr texture) {

    auto ro = addObject(senderId, senderPort, container, geom, normal, texture);
    if (ro && !ro->variant.empty()) {
        auto it = m_variants.find(ro->variant);
        if (it == m_variants.end()) {
            it = m_variants.emplace(ro->variant, Variant(ro->variant)).first;
        }
        ++it->second.objectCount;
        if (it->second.visible == RenderObject::DontChange)
            it->second.visible = ro->visibility;
    }
    return ro;
}

void Renderer::removeObjectWrapper(std::shared_ptr<RenderObject> ro) {

    std::string variant;
    if (ro)
        variant = ro->variant;
    removeObject(ro);
    if (variant.empty()) {
        auto it = m_variants.find(ro->variant);
        if (it != m_variants.end()) {
            --it->second.objectCount;
        }
    }
}

void Renderer::connectionRemoved(const Port *from, const Port *to) {

   removeAllSentBy(from->getModuleID(), from->getName());
}

void Renderer::removeObject(std::shared_ptr<RenderObject> ro) {
}

void Renderer::removeAllCreatedBy(int creatorId) {

   for (auto &ol: m_objectList) {
      for (auto &ro: ol) {
         if (ro && ro->container && ro->container->getCreator() == creatorId) {
            removeObjectWrapper(ro);
            ro.reset();
         }
      }
      ol.erase(std::remove_if(ol.begin(), ol.end(), [](std::shared_ptr<vistle::RenderObject> ro) { return !ro; }), ol.end());
   }
   while (!m_objectList.empty() && m_objectList.back().empty())
      m_objectList.pop_back();

   // only objects are updated: keep colormap
}

void Renderer::removeAllSentBy(int sender, const std::string &senderPort) {

   for (auto &ol: m_objectList) {
      for (auto &ro: ol) {
         if (ro && ro->senderId == sender && ro->senderPort == senderPort) {
            removeObjectWrapper(ro);
            ro.reset();
         }
      }
      ol.erase(std::remove_if(ol.begin(), ol.end(), [](std::shared_ptr<vistle::RenderObject> ro) { return !ro; }), ol.end());
   }
   while (!m_objectList.empty() && m_objectList.back().empty())
      m_objectList.pop_back();

   // connection cut: remove colormap
   auto it = m_colormaps.begin();
   while (it != m_colormaps.end()) {
       auto &cmap = it->second;
       if (cmap.sender == sender && cmap.senderPort == senderPort) {
           removeColorMap(it->first);
           it = m_colormaps.erase(it);
       } else {
           ++it;
       }
   }
}

void Renderer::removeAllObjects() {
   for (auto &ol: m_objectList) {
      for (auto &ro: ol) {
         if (ro) {
            removeObjectWrapper(ro);
            ro.reset();
         }
      }
      ol.clear();
   }
   m_objectList.clear();

   for (auto &cmap: m_colormaps) {
       removeColorMap(cmap.first);
   }
   m_colormaps.clear();
}

const Renderer::VariantMap &Renderer::variants() const {

    return m_variants;
}

bool Renderer::compute() {
    return true;
}

bool Renderer::changeParameter(const Parameter *p) {
    if (p == m_renderMode) {
        switch(m_renderMode->getValue()) {
        case LocalOnly:
            setObjectReceivePolicy(m_fastestObjectReceivePolicy);
            break;
        case MasterOnly:
            setObjectReceivePolicy(m_fastestObjectReceivePolicy >= message::ObjectReceivePolicy::Master ? m_fastestObjectReceivePolicy : message::ObjectReceivePolicy::Master);
            break;
        case AllNodes:
            setObjectReceivePolicy(message::ObjectReceivePolicy::Distribute);
            break;
        }
    }

    if (p == m_objectsPerFrame) {
        m_numObjectsPerFrame = m_objectsPerFrame->getValue();
    }

    return Module::changeParameter(p);
}

void Renderer::getBounds(Vector &min, Vector &max, int t) {

   if (size_t(t+1) < m_objectList.size()) {
      for (auto &ro: m_objectList[t+1]) {
          ro->updateBounds();
          if (!ro->boundsValid()) {
              continue;
          }
          for (int i=0; i<3; ++i) {
              min[i] = std::min(min[i], ro->bMin[i]);
              max[i] = std::max(max[i], ro->bMax[i]);
          }
      }
   }

   //std::cerr << "t=" << t << ", bounds min=" << min << ", max=" << max << std::endl;
}

void Renderer::getBounds(Vector &min, Vector &max) {

   const Scalar smax = std::numeric_limits<Scalar>::max();
   min = Vector3(smax, smax, smax);
   max = -min;
   for (int t=-1; t<(int)(m_objectList.size())-1; ++t)
      getBounds(min, max, t);
}

} // namespace vistle
