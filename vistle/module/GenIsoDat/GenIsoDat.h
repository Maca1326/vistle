#ifndef GENISODAT_H
#define GENISODAT_H

#include <module/module.h>

class GenIsoDat: public vistle::Module {

 public:
   GenIsoDat(const std::string &shmname, int rank, int size, int moduleID);
   ~GenIsoDat();

 private:
   virtual bool compute();

   boost::shared_ptr<vistle::IntParameter> m_cellTypeParam;
   boost::shared_ptr<vistle::IntParameter> m_caseNumParam;
};

#endif
