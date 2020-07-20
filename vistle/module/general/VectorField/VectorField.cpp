#include <sstream>
#include <iomanip>

#include <vistle/core/object.h>
#include <vistle/core/lines.h>
#include <vistle/core/coords.h>
#include <vistle/util/math.h>

#include "VectorField.h"

DEFINE_ENUM_WITH_STRING_CONVERSIONS(AttachmentPoint,
      (Bottom)
      (Middle)
      (Top)
)

MODULE_MAIN(VectorField)

using namespace vistle;

VectorField::VectorField(const std::string &name, int moduleID, mpi::communicator comm)
   : Module("transform mapped vectors into lines", name, moduleID, comm) {

   createInputPort("grid_in");
   createOutputPort("grid_out");

   auto MaxLength = std::numeric_limits<Scalar>::max();

   m_scale = addFloatParameter("scale", "scale factor for vector length", 1.);
   setParameterMinimum(m_scale, (Float)0.);
   m_attachmentPoint = addIntParameter("attachment_point", "where to attach line to carrying point", (Integer)Bottom, Parameter::Choice);
   V_ENUM_SET_CHOICES(m_attachmentPoint, AttachmentPoint);
   m_range = addVectorParameter("range", "allowed length range (before scaling)", ParamVector(0., MaxLength));
   setParameterMinimum(m_range, ParamVector(0., 0.));
}

VectorField::~VectorField() {

}

bool VectorField::compute() {

   auto vecs = expect<Vec<Scalar, 3>>("grid_in");
   if (!vecs) {
      sendError("no vector input");
      return true;
   }
   auto grid = vecs->grid();
   if (!grid) {
      sendError("vectors without grid");
      return true;
   }
   auto coords = Coords::as(grid);
   if  (!coords) {
      sendError("grid does not contain coordinates");
      return true;
   }
   if (vecs->guessMapping() != DataBase::Vertex) {
      sendError("no per-vertex mapping");
      return true;
   }
   Index numPoints = coords->getNumCoords();
   if (vecs->getSize() != numPoints) {
      sendError("geometry size does not match array size: #points=%d, but #vecs=%d", numPoints, vecs->getSize());
      return true;
   }

   Scalar minLen = m_range->getValue()[0];
   Scalar maxLen = m_range->getValue()[1];
   Scalar scale = m_scale->getValue();

   const AttachmentPoint att = (AttachmentPoint)m_attachmentPoint->getValue();

   Lines::ptr lines(new Lines(numPoints, 2*numPoints, 2*numPoints));
   const auto px = &coords->x()[0], py = &coords->y()[0], pz = &coords->z()[0];
   const auto vx = &vecs->x()[0], vy = &vecs->y()[0], vz = &vecs->z()[0];
   auto lx = lines->x().data(), ly = lines->y().data(), lz = lines->z().data();
   auto el = lines->el().data(), cl = lines->cl().data();

   el[0] = 0;
   for (Index i=0; i<numPoints; ++i) {
      Vector v(vx[i], vy[i], vz[i]);
      Scalar l = v.norm();
      if (l < minLen && l>0) {
         v *= minLen/l;
      } else if (l > maxLen) {
         v *= maxLen/l;
      }
      v *= scale;

      Vector p(px[i], py[i], pz[i]);
      Vector p0, p1;
      switch(att) {
         case Bottom:
            p0 = p;
            p1 = p+v;
            break;
         case Middle:
            p0 = p-0.5*v;
            p1 = p+0.5*v;
            break;
         case Top:
            p0 = p-v;
            p1 = p;
            break;
      }

      lx[2*i] = p0[0];
      ly[2*i] = p0[1];
      lz[2*i] = p0[2];
      lx[2*i+1] = p1[0];
      ly[2*i+1] = p1[1];
      lz[2*i+1] = p1[2];
      cl[2*i] = 2*i;
      cl[2*i+1] = 2*i+1;
      el[i+1] = 2*(i+1);
   }

   lines->setMeta(vecs->meta());
   lines->copyAttributes(coords);
   lines->copyAttributes(vecs);
   addObject("grid_out", lines);

   return true;
}
