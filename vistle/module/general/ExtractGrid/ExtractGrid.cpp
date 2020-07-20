#include <vistle/core/object.h>
#include <vistle/core/vec.h>
#include <vistle/core/coords.h>

#include "ExtractGrid.h"

using namespace vistle;

MODULE_MAIN(ExtractGrid)

ExtractGrid::ExtractGrid(const std::string &name, int moduleID, mpi::communicator comm)
   : Module("ExtractGrid", name, moduleID, comm) {

   m_dataIn = createInputPort("data_in");

   m_gridOut = createOutputPort("grid_out");
   m_normalsOut = createOutputPort("normals_out");
}

ExtractGrid::~ExtractGrid() {
}

bool ExtractGrid::compute() {

   auto data = expect<DataBase>(m_dataIn);
   Coords::const_ptr out;
   if (data->grid()) {
      out = Coords::as(data->grid());
   } else if (auto grid = Coords::as(Object::as(data))) {
      out = grid;
   }

   if (out) {
      passThroughObject(m_gridOut, out);
      if (out->normals())
         passThroughObject(m_normalsOut, out->normals());
   }

   return true;
}
