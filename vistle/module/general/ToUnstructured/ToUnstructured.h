//-------------------------------------------------------------------------
// TO UNSTRUCTURED H
// * converts a grid to an unstructured grid
// *
// * Sever Topan, 2016
//-------------------------------------------------------------------------

#ifndef TO_UNSTRUCTURED_H
#define TO_UNSTRUCTURED_H

#include <vistle/core/object.h>
#include <vistle/core/rectilineargrid.h>
#include <vistle/core/structuredgrid.h>
#include <vistle/core/structuredgridbase.h>
#include <vistle/core/uniformgrid.h>
#include <vistle/core/unstr.h>
#include <vistle/module/module.h>

#include "Cartesian3.h"


//-------------------------------------------------------------------------
// STRUCTURED TO UNSTRUCTURED CLASS DECLARATION
//-------------------------------------------------------------------------
class ToUnstructured : public vistle::Module {
 public:


   ToUnstructured(const std::string &name, int moduleID, mpi::communicator comm);
   ~ToUnstructured();

 private:
   // overriden functions
   virtual bool compute() override;

   // private helper functions
   void compute_uniformVecs(vistle::UniformGrid::const_ptr obj, vistle::UnstructuredGrid::ptr unstrGridOut,
                               const Cartesian3<vistle::Index> numVertices);
   void compute_rectilinearVecs(vistle::RectilinearGrid::const_ptr obj, vistle::UnstructuredGrid::ptr unstrGridOut,
                                   const Cartesian3<vistle::Index> numVertices);

};



#endif /* TO_UNSTRUCTURED_H */
