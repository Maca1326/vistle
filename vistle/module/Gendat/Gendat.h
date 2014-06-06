#ifndef GENDAT_H
#define GENDAT_H

#include <module/module.h>

class Gendat: public vistle::Module {

 public:
   Gendat(const std::string &shmname, int rank, int size, int moduleID);
   ~Gendat();

 private:
   virtual bool compute();

   boost::shared_ptr<vistle::IntParameter> m_geoMode;
   boost::shared_ptr<vistle::IntParameter> m_dataMode;
};

#endif
