#include "ngons.h"
#include "triangles.h"
#include "quads.h"
#include "celltree_impl.h"
#include <cassert>

namespace vistle {

template<int N>
Ngons<N>::Ngons(const Index numCorners, const Index numCoords,
                     const Meta &meta)
   : Ngons::Base(Ngons::Data::create(numCorners, numCoords,
            meta)) {
    refreshImpl();
}

template<int N>
void Ngons<N>::refreshImpl() const {
    const Data *d = static_cast<Data *>(m_data);
    m_cl = (d && d->cl.valid()) ? d->cl->data() : nullptr;
    m_numCorners = (d && d->cl.valid()) ? d->cl->size() : 0;
}

template<int N>
bool Ngons<N>::isEmpty() {

   return getNumCoords()==0;
}

template<int N>
bool Ngons<N>::isEmpty() const {

   return getNumCoords()==0;
}

template<int N>
bool Ngons<N>::checkImpl() const {

   V_CHECK (d()->cl->check());
   if (getNumCorners() > 0) {
      V_CHECK (cl()[0] < getNumVertices());
      V_CHECK (cl()[getNumCorners()-1] < getNumVertices());
      V_CHECK (getNumCorners() % N == 0);
   } else {
      V_CHECK (getNumCoords() % N == 0);
   }
   return true;
}

template<int N>
std::pair<Vector, Vector> Ngons<N>::elementBounds(Index elem) const {
   const Index *cl = nullptr;
   if (getNumCorners() > 0)
       cl = &this->cl()[0];
   const Index begin = elem*N, end = begin+N;
   const Scalar *x[3] = { &this->x()[0], &this->y()[0], &this->z()[0] };

   const Scalar smax = std::numeric_limits<Scalar>::max();
   Vector min(smax, smax, smax), max(-smax, -smax, -smax);
   for (Index i=begin; i<end; ++i) {
       Index v=i;
       if (cl)
           v = cl[i];
       for (int c=0; c<3; ++c) {
           min[c] = std::min(min[c], x[c][v]);
           max[c] = std::max(max[c], x[c][v]);
       }
   }
   return std::make_pair(min, max);
}

template<int N>
std::vector<Index> Ngons<N>::cellVertices(Index elem) const {
    std::vector<Index> result;
    result.reserve(N);
    const Index *cl = nullptr;
    if (getNumCorners() > 0)
        cl = &this->cl()[0];
    const Index begin = elem*N, end = begin+N;
    for (Index i=begin; i<end; ++i) {
        Index v=i;
        if (cl)
            v = cl[i];
        result.emplace_back(v);
    }
    return result;
}

template<int N>
bool Ngons<N>::hasCelltree() const {
   if (m_celltree)
       return true;

   return hasAttachment("celltree");
}

template<int N>
Ngons<N>::Celltree::const_ptr Ngons<N>::getCelltree() const {

   if (m_celltree)
       return m_celltree;

   typename Data::attachment_mutex_lock_type lock(d()->attachment_mutex);
   if (!hasAttachment("celltree")) {
      refresh();
      const Index *corners = nullptr;
      if (getNumCorners() > 0)
          corners = &cl()[0];
      createCelltree(getNumElements(), corners);
   }

   m_celltree = Celltree::as(getAttachment("celltree"));
   assert(m_celltree);
   return m_celltree;
}

template<int N>
void Ngons<N>::createCelltree(Index nelem, const Index *cl) const {

   if (hasCelltree())
      return;

   const Scalar *coords[3] = {
      &x()[0],
      &y()[0],
      &z()[0]
   };
   const Scalar smax = std::numeric_limits<Scalar>::max();
   Vector vmin, vmax;
   vmin.fill(-smax);
   vmax.fill(smax);

   std::vector<Vector> min(nelem, vmax);
   std::vector<Vector> max(nelem, vmin);

   Vector gmin=vmax, gmax=vmin;
   for (Index i=0; i<nelem; ++i) {
      const Index start = i*N, end = start+N;
      for (Index c = start; c<end; ++c) {
         Index v = c;
         if (cl)
             v = cl[c];
         for (int d=0; d<3; ++d) {
            min[i][d] = std::min(min[i][d], coords[d][v]);
#if 0
            if (min[i][d] > coords[d][v]) {
               min[i][d] = coords[d][v];
               if (gmin[d] > min[i][d])
                  gmin[d] = min[i][d];
            }
#endif
            max[i][d] = std::max(max[i][d], coords[d][v]);
#if 0
            if (max[i][d] < coords[d][v]) {
               max[i][d] = coords[d][v];
               if (gmax[d] < max[i][d])
                  gmax[d] = max[i][d];
            }
#endif
         }
      }
      for (int d=0; d<3; ++d) {
         gmin[d] = std::min(gmin[d], min[i][d]);
         gmax[d] = std::max(gmax[d], max[i][d]);
      }
   }

   typename Celltree::ptr ct(new Celltree(nelem));
   ct->init(min.data(), max.data(), gmin, gmax);
   addAttachment("celltree", ct);
#ifndef NDEBUG
   if (!validateCelltree()) {
       std::cerr << "ERROR: Celltree validation failed." << std::endl;
   }
#endif
}

template<int N>
struct CellBoundsFunctor: public Ngons<N>::Celltree::CellBoundsFunctor {

   CellBoundsFunctor(const Ngons<N> *ngons)
      : m_ngons(ngons)
      {}

   bool operator()(Index elem, Vector *min, Vector *max) const {

      auto b = m_ngons->elementBounds(elem);
      *min = b.first;
      *max = b.second;
      return true;
   }

   const Ngons<N> *m_ngons;
};

template<int N>
bool Ngons<N>::validateCelltree() const {

   if (!hasCelltree())
      return false;

   CellBoundsFunctor<N> boundFunc(this);
   auto ct = getCelltree();
   if (!ct->validateTree(boundFunc)) {
       std::cerr << "Ngons<N=" << N << ">: Celltree validation failed with " << getNumElements() << " elements total, bounds: " << getBounds().first << "-" << getBounds().second << std::endl;
       return false;
   }
   return true;
}


template<int N>
void Ngons<N>::Data::initData() {
}

template<int N>
Ngons<N>::Data::Data(const Ngons::Data &o, const std::string &n)
: Ngons::Base::Data(o, n)
, cl(o.cl)
{
   initData();
}

template<int N>
Ngons<N>::Data::Data(const Index numCorners, const Index numCoords,
                     const std::string & name,
                     const Meta &meta)
    : Base::Data(numCoords,
                 N == 3 ? Object::TRIANGLES : Object::QUADS, name,
         meta)
{
   initData();
   cl.construct(numCorners);
}


template<int N>
typename Ngons<N>::Data * Ngons<N>::Data::create(const Index numCorners,
                              const Index numCoords,
                              const Meta &meta) {

   const std::string name = Shm::the().createObjectId();
   Data *t = shm<Data>::construct(name)(numCorners, numCoords, name, meta);
   publish(t);

   return t;
}

template<int N>
Index Ngons<N>::getNumElements() {

    return getNumCorners()>0 ? getNumCorners()/N : getNumCoords()/N;
}

template<int N>
Index Ngons<N>::getNumElements() const {

    return getNumCorners()>0 ? getNumCorners()/N : getNumCoords()/N;
}

template<int N>
Index Ngons<N>::getNumCorners() {

   return d()->cl->size();
}

template<int N>
Index Ngons<N>::getNumCorners() const {

   return m_numCorners;
}

template <int N>
Ngons<N>::Ngons(Data *data)
    : Ngons::Base(data)
{
    refreshImpl();
}

#if 0
V_OBJECT_CREATE_NAMED(Vec<T,Dim>)
#else
template<int N>
Ngons<N>::Data::Data(Object::Type id, const std::string &name, const Meta &m)
: Ngons<N>::Base::Data(id, name, m)
{
   initData();
}

template<int N>
typename Ngons<N>::Data *Ngons<N>::Data::createNamed(Object::Type id, const std::string &name) {
   Data *t = shm<Data>::construct(name)(id, name);
   publish(t);
   return t;
}
#endif
//V_OBJECT_CTOR(Triangles)

template<int N>
Object::Type Ngons<N>::type() {
    return N == 3 ? Object::TRIANGLES : Object::QUADS;
}

template class Ngons<3>;
template class Ngons<4>;


#if 0
//V_OBJECT_TYPE(Triangles, Object::TRIANGLES)

//V_OBJECT_TYPE(Quads, Object::QUADS)
V_OBJECT_CTOR(Quads)
#endif

} // namespace vistle
