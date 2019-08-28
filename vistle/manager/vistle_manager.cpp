/*
 * Visualization Testing Laboratory for Exascale Computing (VISTLE)
 */

#include <mpi.h>

#include <iostream>
#include <fstream>
#include <exception>
#include <cstdlib>
#include <sstream>

#include <util/directory.h>
#include <core/objectmeta.h>
#include <core/object.h>
#include "executor.h"
#include "vistle_manager.h"
#include <util/hostname.h>
#include <control/hub.h>
#include <boost/mpi.hpp>


#ifdef COVER_ON_MAINTHREAD
#include <thread>
#include <functional>
#include <deque>
#include <condition_variable>

#if defined(HAVE_QT) && defined(MODULE_THREAD)
#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
#include <xcb/xcb.h>
#include <X11/ICE/ICElib.h>
#endif
#endif
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
namespace {
void iceIOErrorHandler(IceConn conn)
{
    (void)conn;
    std::cerr << "Vistle: ignoring ICE IO error" << std::endl;
}
}
#endif


using namespace vistle;
namespace dir = vistle::directory;

class Vistle: public Executor {
   public:
   Vistle(int argc, char *argv[]) : Executor(argc, argv) {}
   bool config(int argc, char *argv[]) {

      std::string prefix = dir::prefix(argc, argv);
      setModuleDir(dir::module(prefix));
      return true;
   }
};

#ifdef COVER_ON_MAINTHREAD
static std::mutex main_thread_mutex;
static std::condition_variable main_thread_cv;
static std::deque<std::function<void()>> main_func;
static bool main_done = false;

void run_on_main_thread(std::function<void()> &func) {

    {
       std::unique_lock<std::mutex> lock(main_thread_mutex);
       main_func.emplace_back(func);
    }
    main_thread_cv.notify_all();
    std::unique_lock<std::mutex> lock(main_thread_mutex);
    main_thread_cv.wait(lock, []{ return main_done || main_func.empty(); });
}
#endif

int main(int argc, char *argv[])
{
#ifdef MODULE_THREAD
   int provided = MPI_THREAD_SINGLE;
   MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
   if (provided != MPI_THREAD_MULTIPLE) {
      std::cerr << "insufficient thread support in MPI: MPI_THREAD_MULTIPLE is required (maybe set MPICH_MAX_THREAD_SAFETY=multiple?)" << std::endl;
      exit(1);
   }
#else
   MPI_Init(&argc, &argv);
#endif
   int rank = -1;
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);

   std::unique_ptr<std::thread> hubthread;

   bool fromVistle = false;
   if (argc > 1) {
       std::string arg1(argv[1]);
       fromVistle = arg1=="-from-vistle";
   }

   std::vector<std::string> args;
   args.push_back(argv[0]);
   if (fromVistle) {
       // skip -from-vistle
       for (int c=2; c<argc; ++c) {
           args.push_back(argv[c]);
       }
   }

#ifdef MODULE_THREAD
   std::string rank0;
   unsigned short port=0, dataPort=0;
   if (!fromVistle && rank == 0) {
       std::cerr << "not called from vistle, creating hub in a thread" << std::endl;
       auto hub = new Hub(true);
       hub->init(argc, argv);
       port = hub->port();
       dataPort = hub->dataPort();
       rank0 = hostname();
       hubthread.reset(new std::thread([hub, argc, argv](){
           hub->run();
           std::cerr << "Hub exited..." << std::endl;
           delete hub;
       }));

   }

   if (!fromVistle) {
       boost::mpi::broadcast(boost::mpi::communicator(), rank0, 0);
       boost::mpi::broadcast(boost::mpi::communicator(), port, 0);
       boost::mpi::broadcast(boost::mpi::communicator(), dataPort, 0);

       args.push_back(rank0);
       args.push_back(std::to_string(port));
       args.push_back(std::to_string(dataPort));
   }
#else
   if (!fromVistle) {
      std::cerr << "should be called from vistle, expecting 1st argument to be -from-vistle" << std::endl;
      exit(1);
   }
#endif

#ifdef COVER_ON_MAINTHREAD
#if defined(HAVE_QT) && defined(MODULE_THREAD)
   if (!qApp) {
       std::cerr << "early creation of QApplication object" << std::endl;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
       if (xcb_connection_t *xconn = xcb_connect(nullptr, nullptr))
       {
           if (!xcb_connection_has_error(xconn)) {
               std::cerr << "X11 connection!" << std::endl;
               IceSetIOErrorHandler(&iceIOErrorHandler);
               auto app = new QApplication(argc, argv);
           }
           xcb_disconnect(xconn);
       }
#else
       auto app = new QApplication(argc, argv);
#endif
       if (qApp) {
           qApp->setAttribute(Qt::AA_MacDontSwapCtrlAndMeta);
           QIcon icon(":/icons/vistle.png");
           qApp->setWindowIcon(icon);
       }
   }
#endif
   auto t = std::thread([args](){
#endif

   try {
      vistle::registerTypes();
      std::vector<char *> argv;
      for (auto &a: args) {
          argv.push_back(const_cast<char *>(a.data()));
          std::cerr << "arg: " << a << std::endl;
      }
      Vistle(argv.size(), argv.data()).run();
   } catch(vistle::exception &e) {

      std::cerr << "fatal exception: " << e.what() << std::endl << e.where() << std::endl;
      exit(1);
   } catch(std::exception &e) {

      std::cerr << "fatal exception: " << e.what() << std::endl;
      exit(1);
   }

#ifdef COVER_ON_MAINTHREAD
   std::unique_lock<std::mutex> lock(main_thread_mutex);
   main_done = true;
   main_thread_cv.notify_all();
   });


   for (;;) {
      std::unique_lock<std::mutex> lock(main_thread_mutex);
      main_thread_cv.wait(lock, []{ return main_done || !main_func.empty(); });
      if (main_done) {
         //main_thread_cv.notify_all();
         break;
      }

      std::function<void()> f;
      if (!main_func.empty()) {
         std::cerr << "executing on main thread..." << std::endl;
         f = main_func.front();
         if (f)
            f();
         main_func.pop_front();
      }
      lock.unlock();
      main_thread_cv.notify_all();
   }

   t.join();
#endif

   MPI_Finalize();
   return 0;
}
