#include "vec.h"
#include "scalars.h"

#include <boost/mpl/vector.hpp>
#include <boost/mpl/vector_c.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/find.hpp>

namespace vistle {

bool DataBase::isEmpty() const {

   return Base::isEmpty();
}

bool DataBase::checkImpl() const {

   return true;
}

Object::Type DataBase::type()  {

    return DATABASE;
}

DataBase::Data::Data(Type id, const std::string &name,
      const Meta &meta)
   : DataBase::Base::Data(id, name, meta)
{
}

DataBase::Data::Data(const DataBase::Data &o, const std::string &n, Type id)
: DataBase::Base::Data(o, n, id)
{
}


DataBase::Data::Data(const DataBase::Data &o, const std::string &n)
: DataBase::Base::Data(o, n)
{
}

DataBase::Data *DataBase::Data::create(Type id, const Meta &meta) {

   assert("should never be called" == NULL);

   return NULL;
}

Index DataBase::getSize() const {

   assert("should never be called" == NULL);

   return 0;
}

void DataBase::setSize(Index size ) {

   assert("should never be called" == NULL);
}

V_SERIALIZERS(DataBase);


V_SERIALIZERS4(Vec<T,Dim>, template<class T,int Dim>);

namespace mpl = boost::mpl;

namespace {

template<int Dim>
struct instantiator {
   template <typename V> void operator()(V) {
      auto v = new Vec<V, Dim>(0, Meta());
      v->setSize(1);
   }
};

template<int Dim>
struct registrator {
   template <typename S> void operator()(S) {
      typedef Vec<S,Dim> V;
      typedef typename mpl::find<Scalars, S>::type iter;
      ObjectTypeRegistry::registerType<V>(V::type());
      boost::serialization::void_cast_register<V, typename V::Base>(
            static_cast<V *>(NULL), static_cast<typename V::Base *>(NULL)
            );

   }
};

struct DoIt {
   DoIt() {
      mpl::for_each<Scalars>(registrator<1>());
      mpl::for_each<Scalars>(registrator<3>());
   }
};

static DoIt doit;

}

void inst_vecs() {

   mpl::for_each<Scalars>(instantiator<1>());
   mpl::for_each<Scalars>(instantiator<3>());
}

} // namespace vistle
