#ifndef UNSTRUCTUREDGRID_H
#define UNSTRUCTUREDGRID_H


#include "shm.h"
#include "indexed.h"
#include "grid.h"
#include "celltypes.h"
#include <util/enum.h>

namespace vistle {

class V_COREEXPORT UnstructuredGrid: public Indexed, virtual public GridInterface {
   V_OBJECT(UnstructuredGrid);

 public:
   typedef Indexed Base;
   enum Type {
       GHOST_BIT   = cell::GHOST_BIT,
       CONVEX_BIT  = cell::CONVEX_BIT,
       TYPE_MASK   = cell::TYPE_MASK,

       NONE        = cell::NONE,
       BAR         = cell::BAR,
       TRIANGLE    = cell::TRIANGLE,
       QUAD        = cell::QUAD,
       TETRAHEDRON = cell::TETRAHEDRON,
       PYRAMID     = cell::PYRAMID,
       PRISM       = cell::PRISM,
       HEXAHEDRON  = cell::HEXAHEDRON,
       POLYGON     = cell::POLYGON,
       POINT       = cell::POINT,
       POLYHEDRON  = cell::POLYHEDRON,
   };

   static const Index MaxNumVertices = 4;
   static const Index MaxNumFaces = 6;

   static const int Dimensionality[POLYHEDRON+1];
   static const int NumVertices[POLYHEDRON+1];
   static const int NumFaces[POLYHEDRON+1];
   static const unsigned FaceSizes[UnstructuredGrid::POLYHEDRON+1][MaxNumFaces];
   static const int FaceVertices[UnstructuredGrid::POLYHEDRON+1][MaxNumFaces][MaxNumVertices];

   UnstructuredGrid(const Index numElements,
         const Index numCorners,
         const Index numVertices,
         const Meta &meta=Meta());

   void resetElements() override;

   shm<unsigned char>::array &tl() { return *d()->tl; }
   const unsigned char *tl() const { return m_tl; }

   bool isConvex(Index elem) const;
   bool isGhostCell(Index elem) const override;
   std::pair<Vector, Vector> cellBounds(Index elem) const override;
   Index findCell(const Vector &point, Index hint=InvalidIndex, int flags=NoFlags) const override;
   bool insideConvex(Index elem, const Vector &point) const;
   bool inside(Index elem, const Vector &point) const override;
   Index checkConvexity(); //< return number of non-convex cells
   Scalar exitDistance(Index elem, const Vector &point, const Vector &dir) const override;

   Interpolator getInterpolator(Index elem, const Vector &point, Mapping mapping=Vertex, InterpolationMode mode=Linear) const override;
   std::pair<Vector, Vector> elementBounds(Index elem) const override;
   std::vector<Index> cellVertices(Index elem) const override;
   Scalar cellDiameter(Index elem) const override;
   Vector cellCenter(Index elem) const override;
   std::vector<Index> getNeighborElements(Index elem) const override;

 private:
   mutable const unsigned char *m_tl;

   V_DATA_BEGIN(UnstructuredGrid);
      ShmVector<unsigned char> tl;

      Data(const Index numElements = 0, const Index numCorners = 0,
                    const Index numVertices = 0, const std::string & name = "",
                    const Meta &meta=Meta());
      static Data *create(const Index numElements = 0,
                                    const Index numCorners = 0,
                                    const Index numVertices = 0,
                                    const Meta &meta=Meta());

   V_DATA_END(UnstructuredGrid);

};

} // namespace vistle
#endif

#ifdef VISTLE_IMPL
#include "unstr_impl.h"
#endif
