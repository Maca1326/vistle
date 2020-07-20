#ifndef TEXTURE1D_H
#define TEXTURE1D_H


#include "scalar.h"
#include "shm.h"
#include "object.h"
#include "vec.h"

namespace vistle {

class V_COREEXPORT Texture1D: public Vec<Scalar> {
   V_OBJECT(Texture1D);

 public:
   typedef Vec<Scalar> Base;

   Texture1D(const Index width,
         const Scalar min, const Scalar max,
         const Meta &meta=Meta());

   Index getWidth() const;
   Scalar getMin() const;
   Scalar getMax() const;
   Index getNumCoords() { return getSize(); }
   Index getNumCoords() const { return getSize(); }
   shm<unsigned char>::array &pixels() { return *d()->pixels; }
   const shm<unsigned char>::array &pixels() const { return *d()->pixels; }
   shm<Scalar>::array &coords() { return x(); }
   const Scalar *coords() const  { return x(); }

   V_DATA_BEGIN(Texture1D);
      Scalar range[2];

      ShmVector<unsigned char> pixels;

      static Data *create(const Index width = 0,
            const Scalar min = 0, const Scalar max = 0,
            const Meta &m=Meta());
      Data(const std::string &name = "", const Index size = 0,
            const Scalar min = 0, const Scalar max = 0,
            const Meta &m=Meta());

   V_DATA_END(Texture1D);
};

} // namespace vistle
#endif

#include "texture1d_impl.h"
