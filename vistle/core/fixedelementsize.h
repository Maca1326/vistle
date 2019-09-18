#ifndef VISTLE_FIXEDELEMENTSIZE_H
#define VISTLE_FIXEDELEMENTSIZE_H

#include "shm.h"
#include "coords.h"
#include "geometry.h"

namespace vistle {

class V_COREEXPORT FixedElementSizeBase: public Coords, virtual public ElementInterface {
   V_OBJECT(FixedElementSizeBase);

 public:
   typedef Coords Base;

   Ngons(const Index numCorners, const Index numCoords,
             const Meta &meta=Meta());

   Index getNumElements() override;
   Index getNumElements() const override;
   Index getNumCorners();
   Index getNumCorners() const;

   shm<Index>::array &cl() { return *d()->cl; }
   const Index *cl() const { return m_cl; }

 private:
   mutable const Index *m_cl;
   mutable Index m_numCorners = 0;

   V_DATA_BEGIN(FixedElementSizeBase);
      ShmVector<Index> cl;

      Data(const Index numCorners = 0, const Index numCoords = 0,
            const std::string & name = "",
            const Meta &meta=Meta());
      static Data *create(const Index numCorners = 0, const Index numCoords = 0,
            const Meta &meta=Meta());
   V_DATA_END(FixedElementSizeBase);

   static_assert (N==3 || N==4, "only usable for triangles and quads");
};


template<int type>
class V_COREEXPORT FixedElementSize: public FixedElementSizeBase
   V_OBJECT(FixedElementSize);

 public:
   static constexpr int num_corners = N;
   typedef Coords Base;

   Ngons(const Index numCorners, const Index numCoords,
             const Meta &meta=Meta());

   Index getNumElements() override;
   Index getNumElements() const override;
   Index getNumCorners();
   Index getNumCorners() const;

   shm<Index>::array &cl() { return *d()->cl; }
   const Index *cl() const { return m_cl; }

 private:
   mutable const Index *m_cl;
   mutable Index m_numCorners = 0;

   V_DATA_BEGIN(Ngons);
      ShmVector<Index> cl;

      Data(const Index numCorners = 0, const Index numCoords = 0,
            const std::string & name = "",
            const Meta &meta=Meta());
      static Data *create(const Index numCorners = 0, const Index numCoords = 0,
            const Meta &meta=Meta());
   V_DATA_END(Ngons);

   static_assert (N==3 || N==4, "only usable for triangles and quads");
};

} // namespace vistle
#endif

#ifdef VISTLE_IMPL
#include "fixedelementsize_impl.h"
#endif
