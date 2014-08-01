#ifndef ISOSURFACE_H
#define ISOSURFACE_H

#include <module/module.h>

class IsoSurface: public vistle::Module {

 public:
   IsoSurface(const std::string &shmname, int rank, int size, int moduleID);
   ~IsoSurface();

 private:
   vistle::Object::ptr generateIsoSurface(vistle::Object::const_ptr grid,
                                       vistle::Object::const_ptr data,
                                       const vistle::Scalar isoValue,
                                          int processorType);

   virtual bool compute();
   virtual bool prepare();
   virtual bool reduce(int timestep);

   boost::shared_ptr<vistle::FloatParameter> m_isovalue;
   boost::shared_ptr<vistle::StringParameter> m_shader, m_shaderParams;
   boost::shared_ptr<vistle::IntParameter> m_processortype;

   vistle::Scalar min, max;
};

#endif
