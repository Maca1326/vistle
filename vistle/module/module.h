#ifndef MODULE_H
#define MODULE_H

#if 0
#ifndef MPICH_IGNORE_CXX_SEEK
#define MPICH_IGNORE_CXX_SEEK
#endif
#include <mpi.h>
#else
#include <boost/mpi.hpp>
#endif

#include <iostream>
#include <list>
#include <map>
#include <exception>

#include <core/paramvector.h>
#include <core/object.h>
#include <core/objectcache.h>
#include <core/parameter.h>
#include <core/port.h>

#include <module/export.h>

namespace vistle {

class StateTracker;

namespace message {
class Message;
class AddParameter;
class SetParameter;
class MessageQueue;
}

class V_MODULEEXPORT Module {

 public:
   Module(const std::string &name, const std::string &shmname,
          const unsigned int rank, const unsigned int size, const int moduleID);
   virtual ~Module();
   void initDone(); // to be called from MODULE_MAIN after module ctor has run

   virtual bool dispatch();

   const std::string &name() const;
   unsigned int rank() const;
   unsigned int size() const;
   int id() const;

   ObjectCache::CacheMode setCacheMode(ObjectCache::CacheMode mode, bool update=true);
   ObjectCache::CacheMode cacheMode(ObjectCache::CacheMode mode) const;

   Port *createInputPort(const std::string &name, const std::string &description="", const int flags=0);
   Port *createOutputPort(const std::string &name, const std::string &description="", const int flags=0);

   //! set group for all subsequently added parameters, reset with empty group
   void setCurrentParameterGroup(const std::string &group);
   const std::string &currentParameterGroup() const;

   boost::shared_ptr<Parameter> addParameterGeneric(const std::string &name, boost::shared_ptr<Parameter> parameter);
   bool updateParameter(const std::string &name, const boost::shared_ptr<Parameter> parameter, const message::SetParameter *inResponseTo, Parameter::RangeType rt=Parameter::Value);

   template<class T>
   boost::shared_ptr<Parameter> addParameter(const std::string &name, const std::string &description, const T &value, Parameter::Presentation presentation=Parameter::Generic);
   template<class T>
   bool setParameter(const std::string &name, const T &value, const message::SetParameter *inResponseTo);
   template<class T>
   bool setParameter(boost::shared_ptr<ParameterBase<T>> param, const T &value, const message::SetParameter *inResponseTo);
   template<class T>
   bool setParameterMinimum(boost::shared_ptr<ParameterBase<T>> param, const T &minimum);
   template<class T>
   bool setParameterMaximum(boost::shared_ptr<ParameterBase<T>> param, const T &maximum);
   template<class T>
   bool setParameterRange(const std::string &name, const T &minimum, const T &maximum);
   template<class T>
   bool setParameterRange(boost::shared_ptr<ParameterBase<T>> param, const T &minimum, const T &maximum);
   template<class T>
   bool getParameter(const std::string &name, T &value) const;
   void setParameterChoices(const std::string &name, const std::vector<std::string> &choices);
   void setParameterChoices(boost::shared_ptr<Parameter> param, const std::vector<std::string> &choices);

   boost::shared_ptr<StringParameter> addStringParameter(const std::string & name, const std::string &description, const std::string & value, Parameter::Presentation p=Parameter::Generic);
   bool setStringParameter(const std::string & name, const std::string & value, const message::SetParameter *inResponseTo=NULL);
   std::string getStringParameter(const std::string & name) const;

   boost::shared_ptr<FloatParameter> addFloatParameter(const std::string & name, const std::string &description, const Float value);
   bool setFloatParameter(const std::string & name, const Float value, const message::SetParameter *inResponseTo=NULL);
   Float getFloatParameter(const std::string & name) const;

   boost::shared_ptr<IntParameter> addIntParameter(const std::string & name, const std::string &description, const Integer value, Parameter::Presentation p=Parameter::Generic);
   bool setIntParameter(const std::string & name, const Integer value, const message::SetParameter *inResponseTo=NULL);
   Integer getIntParameter(const std::string & name) const;

   boost::shared_ptr<VectorParameter> addVectorParameter(const std::string & name, const std::string &description, const ParamVector & value);
   bool setVectorParameter(const std::string & name, const ParamVector & value, const message::SetParameter *inResponseTo=NULL);
   ParamVector getVectorParameter(const std::string & name) const;

   bool addObject(Port *port, vistle::Object::ptr object);
   bool addObject(const std::string &portName, vistle::Object::ptr object);
   bool passThroughObject(Port *port, vistle::Object::const_ptr object);
   bool passThroughObject(const std::string &portName, vistle::Object::const_ptr object);

   ObjectList getObjects(const std::string &portName);
   void removeObject(const std::string &portName, vistle::Object::const_ptr object);
   bool hasObject(const std::string &portName) const;
   vistle::Object::const_ptr takeFirstObject(const std::string &portName);

   void sendMessage(const message::Message &message) const;

   //! type should be a message::SendText::TextType
   void sendText(int type, const std::string &msg) const;

   /// send info message to UI - printf style
   void sendInfo(const char *fmt, ...) const
#ifdef __GNUC__
   __attribute__ ((format (printf, 2, 3)))
#endif
   ;

   /// send warning message to UI - printf style
   void sendWarning(const char *fmt, ...) const
#ifdef __GNUC__
   __attribute__ ((format (printf, 2, 3)))
#endif
   ;

   /// send error message to UI - printf style
   void sendError(const char *fmt, ...) const
#ifdef __GNUC__
   __attribute__ ((format (printf, 2, 3)))
#endif
   ;

   /// send response message to UI - printf style
   void sendError(const message::Message &msg, const char *fmt, ...) const
#ifdef __GNUC__
   __attribute__ ((format (printf, 3, 4)))
#endif
   ;

   /// send info message to UI - string style
   void sendInfo(const std::string &text) const;
   /// send warning message to UI - string style
   void sendWarning(const std::string &text) const;
   /// send error message to UI - string style
   void sendError(const std::string &text) const;
   /// send response message to UI - string style
   void sendError(const message::Message &msg, const std::string &text) const;

   int schedulingPolicy() const;
   void setSchedulingPolicy(int schedulingPolicy /*< really message::SchedulingPolicy::Schedule */); 

   int reducePolicy() const;
   void setReducePolicy(int reduceRequirement /*< really message::ReducePolicy::Reduce */);

protected:

   void setObjectReceivePolicy(int pol);
   int objectReceivePolicy() const;

   const std::string m_name;
   const unsigned int m_rank;
   const unsigned int m_size;
   const int m_id;

   int m_executionCount;

   void setDefaultCacheMode(ObjectCache::CacheMode mode);
   void updateMeta(vistle::Object::ptr object) const;

   message::MessageQueue *sendMessageQueue;
   message::MessageQueue *receiveMessageQueue;
   bool handleMessage(const message::Message *message);

   virtual bool addInputObject(const std::string & portName,
                               Object::const_ptr object);
   bool syncMessageProcessing() const;
   void setSyncMessageProcessing(bool sync);

   bool isConnected(const std::string &portname) const;

   std::string getModuleName(int id) const;

   virtual bool parameterChanged(const Parameter &p);

   int openmpThreads() const;
   void setOpenmpThreads(int, bool updateParam=true);

   void enableBenchmark(bool benchmark, bool updateParam=true);

   virtual bool prepare(); //< prepare execution - called on each rank individually
   virtual bool reduce(int timestep); //< do reduction for timestep (-1: global) - called on all ranks

 private:
   boost::shared_ptr<StateTracker> m_stateTracker;
   int m_receivePolicy;
   int m_schedulingPolicy;
   int m_reducePolicy;
   int m_executionDepth; //< number of input ports that have sent ExecutionProgress::Start

   boost::shared_ptr<Parameter> findParameter(const std::string &name) const;
   Port *findInputPort(const std::string &name) const;
   Port *findOutputPort(const std::string &name) const;

   virtual bool parameterAdded(const int senderId, const std::string &name, const message::AddParameter &msg, const std::string &moduleName);
   virtual bool parameterChanged(const int senderId, const std::string &name, const message::SetParameter &msg);

   virtual bool compute() = 0; //< do processing - called on each rank individually

   std::map<std::string, Port*> outputPorts;
   std::map<std::string, Port*> inputPorts;

   std::string m_currentParameterGroup;
   std::map<std::string, boost::shared_ptr<Parameter>> parameters;
   ObjectCache m_cache;
   ObjectCache::CacheMode m_defaultCacheMode;
   void updateCacheMode();
   bool m_syncMessageProcessing;

   void updateOutputMode();
   std::streambuf *m_origStreambuf, *m_streambuf;

   int m_traceMessages;
   bool m_benchmark;
   double m_benchmarkStart;
};

} // namespace vistle

#define MODULE_MAIN(X) \
   int main(int argc, char **argv) { \
      int provided = MPI_THREAD_SINGLE; \
      MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided); \
      if (provided == MPI_THREAD_SINGLE) { \
         std::cerr << "no thread support in MPI" << std::endl; \
         exit(1); \
      } \
      vistle::registerTypes(); \
      int rank=-1, size=-1; \
      try { \
         if (argc != 3) { \
            std::cerr << "module requires exactly 2 parameters" << std::endl; \
            MPI_Finalize(); \
            exit(1); \
         } \
         MPI_Comm_rank(MPI_COMM_WORLD, &rank); \
         MPI_Comm_size(MPI_COMM_WORLD, &size); \
         const std::string &shmname = argv[1]; \
         int moduleID = atoi(argv[2]); \
         {  \
            X module(shmname, rank, size, moduleID); \
            module.initDone(); \
            while (module.dispatch()) \
               ; \
         } \
         MPI_Barrier(MPI_COMM_WORLD); \
      } catch(vistle::exception &e) { \
         std::cerr << "[" << rank << "/" << size << "]: fatal exception: " << e.what() << std::endl; \
         std::cerr << "  info: " << e.info() << std::endl; \
         std::cerr << e.where() << std::endl; \
         MPI_Finalize(); \
         exit(0); \
      } catch(std::exception &e) { \
         std::cerr << "[" << rank << "/" << size << "]: fatal exception: " << e.what() << std::endl; \
         MPI_Finalize(); \
         exit(0); \
      } \
      MPI_Finalize(); \
      return 0; \
   }

#ifdef NDEBUG
#define MODULE_DEBUG(X)
#else
#define MODULE_DEBUG(X) \
   std::cerr << #X << ": PID " << getpid() << std::endl; \
   std::cerr << "   attach debugger within 10 s" << std::endl; \
   sleep(10); \
   std::cerr << "   continuing..." << std::endl;
#endif

#ifdef VISTLE_IMPL
#include "module_impl.h"
#endif
#endif
