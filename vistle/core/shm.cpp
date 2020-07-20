
#include <vistle/util/sysdep.h>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/transform.hpp>

#include <limits.h>

#include <vistle/util/valgrind.h>
#include "messagequeue.h"
#include "scalars.h"
#include <cassert>

#include "archives.h"
#include "shm.h"
#include "shm_reference.h"
#include "object.h"
#include "shm_reference_impl.h"

#include "celltree.h"
#include "archives_config.h"

using namespace boost::interprocess;
namespace interprocess = boost::interprocess;

namespace vistle {

template<int> size_t memorySize();

template<> size_t memorySize<4>() {

   return INT_MAX;
}

template<> size_t memorySize<8>() {

   if (RUNNING_ON_VALGRIND) {
      std::cerr << "running under valgrind: reducing shmem size" << std::endl;
      return (size_t)1 << 32;
   } else {
#if defined(__APPLE__)
      return (size_t)1 << 32;
#elif defined(_WIN32)
      return (size_t)1 << 30; // 1 GB
#else
      return (size_t)1 << 40; // 1 TB
#endif
   }
}

Shm* Shm::s_singleton = nullptr;
#ifdef SHMDEBUG
#ifdef NO_SHMEM
shm<ShmDebugInfo>::vector *Shm::s_shmdebug = new shm<ShmDebugInfo>::vector;
std::recursive_mutex *Shm::s_shmdebugMutex = new std::recursive_mutex;
#else
shm<ShmDebugInfo>::vector *Shm::s_shmdebug = nullptr;
boost::interprocess::interprocess_recursive_mutex *Shm::s_shmdebugMutex = nullptr;
#endif
#endif

std::string Shm::instanceName(const std::string &host, unsigned short port) {

   std::stringstream str;
   str << "vistle_" << host << "_u" << getuid() << "_p" << port;
   return str.str();
}

Shm::Shm(const std::string &name, const int m, const int r, size_t size, bool create)
   : m_allocator(nullptr)
   , m_name(name)
   , m_remove(create)
   , m_id(m)
   , m_rank(r)
   , m_objectId(0)
   , m_arrayId(0)
   , m_shmDeletionMutex(nullptr)
   , m_objectDictionaryMutex(nullptr)
#ifndef NO_SHMEM
   , m_shm(nullptr)
#endif
{

#ifdef SHMDEBUG
      if (create) {
          std::cerr << "SHMDEBUG: won't remove shm " << m_name << std::endl;
          m_remove = false;
      }
#endif

#ifdef NO_SHMEM
      (void)size;
      m_allocator = new void_allocator();
      m_shmDeletionMutex = new std::recursive_mutex;
      m_objectDictionaryMutex = new std::recursive_mutex;
#else
#ifdef WIN32
	  m_shm = new managed_windows_shared_memory(open_or_create, m_name.c_str(), size);
#else
      if (create) {
         m_shm = new managed_shared_memory(open_or_create, m_name.c_str(), size);
      } else {
         m_shm = new managed_shared_memory(open_only, m_name.c_str());
      }
#endif

      m_allocator = new void_allocator(shm().get_segment_manager());

      m_shmDeletionMutex = m_shm->find_or_construct<boost::interprocess::interprocess_recursive_mutex>("shmdelete_mutex")();
      m_objectDictionaryMutex = m_shm->find_or_construct<boost::interprocess::interprocess_recursive_mutex>("shm_dictionary_mutex")();

#ifdef SHMDEBUG
      s_shmdebugMutex = m_shm->find_or_construct<boost::interprocess::interprocess_recursive_mutex>("shmdebug_mutex")();
      s_shmdebug = m_shm->find_or_construct<vistle::shm<ShmDebugInfo>::vector>("shmdebug")(0, ShmDebugInfo(), allocator());
#endif
#endif

      m_lockCount = 0;
}

Shm::~Shm() {

#ifndef NO_SHMEM
   if (m_remove) {
      shared_memory_object::remove(m_name.c_str());
      std::cerr << "removed shm " << m_name << std::endl;
   }
   delete m_shm;
#endif

   delete m_allocator;
}

void Shm::setId(int id) {

    m_id = id;
}

void Shm::detach() {

   delete s_singleton;
   s_singleton = nullptr;
}

void Shm::setRemoveOnDetach() {

   m_remove = true;
}

std::string Shm::shmIdFilename() {

   std::stringstream name;
#ifdef _WIN32
   
   
    DWORD dwRetVal = 0;
    UINT uRetVal   = 0;
    TCHAR szTempFileName[MAX_PATH];  
    TCHAR lpTempPathBuffer[MAX_PATH];

    //  Gets the temp path env string (no guarantee it's a valid path).
    dwRetVal = GetTempPath(MAX_PATH,          // length of the buffer
                           lpTempPathBuffer); // buffer for path 
    if (dwRetVal > MAX_PATH || (dwRetVal == 0))
    {
        name << "/tmp/vistle_shmids_";
    }

    //  Generates a temporary file name. 
    uRetVal = GetTempFileName(lpTempPathBuffer, // directory for tmp files
                              TEXT("DEMO"),     // temp file name prefix 
                              0,                // create unique name 
                              szTempFileName);  // buffer for name 
    if (uRetVal == 0)
    {
        
        name << "/tmp/vistle_shmids_";
    }
	name << szTempFileName;
#else
   name << "/tmp/vistle_shmids_" << getuid() << ".txt";
#endif
   return name.str();
}

void Shm::lockObjects() const {
#ifndef NO_SHMEM
#ifndef SHMPERRANK
   if (m_lockCount != 0) {
       //std::cerr << "Shm::lockObjects(): lockCount=" << m_lockCount << std::endl;
   }
   //assert(m_lockCount==0);
   ++m_lockCount;
   m_shmDeletionMutex->lock();
#endif
#endif
}

void Shm::unlockObjects() const {
#ifndef NO_SHMEM
#ifndef SHMPERRANK
   m_shmDeletionMutex->unlock();
   --m_lockCount;
   //assert(m_lockCount==0);
   if (m_lockCount != 0) {
       //std::cerr << "Shm::unlockObjects(): lockCount=" << m_lockCount << std::endl;
   }
#endif
#endif
}

void Shm::lockDictionary() const {
#ifdef NO_SHMEM
   m_objectDictionaryMutex->lock();
#endif
}

void Shm::unlockDictionary() const {
#ifdef NO_SHMEM
   m_objectDictionaryMutex->unlock();
#endif
}

int Shm::objectID() const {
    return m_objectId;
}

int Shm::arrayID() const {
    return m_arrayId;
}

void Shm::setObjectID(int id) {
    assert(id >= m_objectId);
    m_objectId = id;
}

void Shm::setArrayID(int id) {
    assert(id >= m_arrayId);
    m_arrayId = id;
}

namespace {

std::string shmSegName(const std::string &name, const int rank) {
#ifdef SHMPERRANK
    return name + "_r" + std::to_string(rank);
#else
    (void)rank;
    return name;
#endif
}

}

bool Shm::cleanAll(int rank) {

   std::fstream shmlist;
   shmlist.open(shmIdFilename().c_str(), std::ios::in);

   bool ret = true;

   while (!shmlist.eof() && !shmlist.fail()) {
      std::string shmid;
      shmlist >> shmid;
      if (!shmid.empty()) {
         bool ok = true;
         bool log = true;

         if (shmid.find("_send_") != std::string::npos || shmid.find("_recv_") != std::string::npos) {
            //std::cerr << "removing message queue: id " << shmid << std::flush;
            ok = message::MessageQueue::message_queue::remove(shmid.c_str());
            log = false;
         } else {
            std::cerr << "removing shared memory: id " << shmid << std::flush;
            ok = shared_memory_object::remove(shmSegName(shmid.c_str(), rank).c_str());
         }
         if (log)
             std::cerr << ": " << (ok ? "ok" : "failure") << std::endl;

         if (!ok)
            ret = false;
      }
   }
   shmlist.close();

   ::remove(shmIdFilename().c_str());

   return ret;
}

std::string Shm::name() const {

    return shmSegName(instanceName(), m_rank);
}

const std::string &Shm::instanceName() const {

   return m_name;
}

const Shm::void_allocator &Shm::allocator() const {

   return *m_allocator;
}

Shm &Shm::the() {
#ifndef MODULE_THREAD
   assert(s_singleton && "make sure to create or attach to a shm segment first");
   if (!s_singleton)
      exit(1);
#endif
   return *s_singleton;
}

bool Shm::isAttached() {
    return s_singleton;
}

bool Shm::remove(const std::string &name, const int id, const int rank) {

   std::string n = shmSegName(name, rank);
   return interprocess::shared_memory_object::remove(n.c_str());
}

Shm & Shm::create(const std::string &name, const int id, const int rank) {

   std::string n = shmSegName(name, rank);

   if (!s_singleton) {
      {
         // store name of shared memory segment for possible clean up
         std::ofstream shmlist;
         shmlist.open(shmIdFilename().c_str(), std::ios::out | std::ios::app);
         shmlist << name << std::endl;
      }

      size_t memsize = memorySize<sizeof(void *)>();

      do {
         try {
            s_singleton = new Shm(n, id, rank, memsize, true);
         } catch (boost::interprocess::interprocess_exception & /*ex*/) {
            memsize /= 2;
         }
      } while (!s_singleton && memsize >= 4096);

      if (!s_singleton) {
         throw(shm_exception("failed to create shared memory segment"));

         std::cerr << "failed to create shared memory: module id: " << id
                   << ", rank: " << rank << std::endl;
      }
   }

   assert(s_singleton && "failed to create shared memory");

   return *s_singleton;
}

Shm & Shm::attach(const std::string &name, const int id, const int rank) {

   std::string n = shmSegName(name, rank);

   if (!s_singleton) {
      size_t memsize = memorySize<sizeof(void *)>();

      do {
         try {
            s_singleton = new Shm(n, id, rank, memsize, false);
         } catch (boost::interprocess::interprocess_exception &) {
            memsize /= 2;
         }
      } while (!s_singleton && memsize >= 4096);

      if (!s_singleton) {
         std::cerr << "failed to attach to shared memory: module id: " << id
                    << ", rank: " << rank << std::endl;
      }
   }

   if (!s_singleton) {

      throw(shm_exception("failed to attach to shared memory segment "+name));
   }

   assert(s_singleton && "failed to attach to shared memory");

   return *s_singleton;
}

#ifndef NO_SHMEM
managed_shm & Shm::shm() {

   return *m_shm;
}

const managed_shm & Shm::shm() const {

   return *m_shm;
}
#endif

std::string Shm::createObjectId(const std::string &id) {

#ifdef MODULE_THREAD
   assert(m_id < 0);
#else
   assert(m_id > 0 || !id.empty());
#endif
   assert(id.size() < sizeof(shm_name_t));
   if (!id.empty()) {
      return id;
   }
   std::stringstream name;
   name
#ifdef MODULE_THREAD
        << -m_id << "h"
#else
        << m_id << "m"
#endif
        << m_objectId++ << "o"
        << m_rank << "r";

   assert(name.str().size() < sizeof(shm_name_t));

   return name.str();
}

std::string Shm::createArrayId(const std::string &id) {
   assert(id.size() < sizeof(shm_name_t));
   if (!id.empty()) {
      return id;
   }

   std::stringstream name;
   name
#ifdef MODULE_THREAD
        << -m_id << "h"
#else
        << m_id << "m"
#endif
        << m_arrayId++ << "a"
        << m_rank << "r";

   assert(name.str().size() < sizeof(shm_name_t));

   return name.str();
}

void Shm::markAsRemoved(const std::string &name) {
#ifdef SHMDEBUG
   s_shmdebugMutex->lock();

   for (size_t i=0; i<s_shmdebug->size(); ++i) {
      if (!strncmp(name.c_str(), (*s_shmdebug)[i].name, sizeof(shm_name_t))) {
          if ((*s_shmdebug)[i].deleted > 0) {
              std::cerr << "Shm: MULTIPLE deletion of " << name << " (count=" << (int)(*s_shmdebug)[i].deleted << ")" << std::endl;
          }
          assert((*s_shmdebug)[i].deleted == 0);
          ++(*s_shmdebug)[i].deleted;
      }
   }

   s_shmdebugMutex->unlock();
#endif
}

void Shm::addObject(const std::string &name, const shm_handle_t &handle) {
#ifdef SHMDEBUG
   s_shmdebugMutex->lock();
   Shm::the().s_shmdebug->push_back(ShmDebugInfo('O', name, handle));
   s_shmdebugMutex->unlock();
#endif
}

void Shm::addArray(const std::string &name, const ShmData *array) {
#ifdef SHMDEBUG
   s_shmdebugMutex->lock();
   auto handle = getHandleFromArray(array);
   Shm::the().s_shmdebug->push_back(ShmDebugInfo('V', name, handle));
   s_shmdebugMutex->unlock();
#endif
}

shm_handle_t Shm::getHandleFromObject(Object::const_ptr object) const {

#ifdef NO_SHMEM
   return object->d();
#else
   try {
      return m_shm->get_handle_from_address(object->d());

   } catch (interprocess_exception &) { }

   return 0;
#endif
}

shm_handle_t Shm::getHandleFromObject(const Object *object) const {

#ifdef NO_SHMEM
   return object->d();
#else
   try {
      return m_shm->get_handle_from_address(object->d());

   } catch (interprocess_exception &) { }

   return 0;
#endif
}

shm_handle_t Shm::getHandleFromArray(const ShmData *array) const {

#ifdef NO_SHMEM
   return const_cast<ShmData *>(array);
#else
   try {
      return m_shm->get_handle_from_address(array);

   } catch (interprocess_exception &) { }

   return 0;
#endif
}

Object::const_ptr Shm::getObjectFromHandle(const shm_handle_t & handle) const {
    
    Object::const_ptr ret;
    lockObjects();
    Object::Data *od = getObjectDataFromHandle(handle);
    if (od) {
        od->ref();
        unlockObjects();
        ret.reset(Object::create(od));
        od->unref();
    } else {
        unlockObjects();
    }
    return ret;
}

ObjectData *Shm::getObjectDataFromName(const std::string &name) const {

#ifdef NO_SHMEM
    return static_cast<Object::Data *>(vistle::shm<void>::find(name));
#else
    return static_cast<Object::Data *>(static_cast<void *>(vistle::shm<char>::find(name)));
#endif
}

ObjectData *Shm::getObjectDataFromHandle(const shm_handle_t &handle) const
{
#ifdef NO_SHMEM
    Object::Data *od = static_cast<Object::Data *>(handle);
    return od;
#else
    try {
        Object::Data *od = static_cast<Object::Data *>(m_shm->get_address_from_handle(handle));
        assert(od->shmtype == ShmData::OBJECT);
        return od;
    } catch (interprocess_exception &) {
        std::cerr << "Shm::getObjectDataFromHandle: invalid handle " << handle << std::endl;
    }
    
    return nullptr;
#endif
    
}

Object::const_ptr Shm::getObjectFromName(const std::string &name, bool onlyComplete) const {

   if (name.empty())
       return Object::const_ptr();

   lockObjects();
   auto od = getObjectDataFromName(name);
   if (od) {
      if (od->isComplete() || !onlyComplete) {
          od->ref();
          unlockObjects();
          Object::const_ptr obj(Object::create(od));
          od->unref();
          return obj;
      }
      unlockObjects();
      std::cerr << "Shm::getObjectFromName: " << name << " not complete" << std::endl;
   } else {
       unlockObjects();
       //std::cerr << "Shm::getObjectFromName: did not find " << name << std::endl;
   }

   return Object::const_ptr();
}

V_DEFINE_SHMREF(char)
V_DEFINE_SHMREF(signed char)
V_DEFINE_SHMREF(unsigned char)
V_DEFINE_SHMREF(int32_t)
V_DEFINE_SHMREF(uint32_t)
V_DEFINE_SHMREF(int64_t)
V_DEFINE_SHMREF(uint64_t)
V_DEFINE_SHMREF(float)
V_DEFINE_SHMREF(double)

} // namespace vistle
