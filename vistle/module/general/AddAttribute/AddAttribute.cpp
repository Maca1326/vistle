#include <vistle/module/module.h>
#include <vistle/core/object.h>
#include <sstream>

const int NumAttributes = 3;

using namespace vistle;

class AddAttribute: public vistle::Module {

 public:
   AddAttribute(const std::string &name, int moduleID, mpi::communicator comm);
   ~AddAttribute();

 private:
   virtual bool compute();

   StringParameter *p_name[NumAttributes];
   StringParameter *p_value[NumAttributes];
};

using namespace vistle;

AddAttribute::AddAttribute(const std::string &name, int moduleID, mpi::communicator comm)
: Module("add attributes", name, moduleID, comm)
{

   Port *din = createInputPort("data_in", "input data", Port::MULTI);
   Port *dout = createOutputPort("data_out", "output data", Port::MULTI);
   din->link(dout);

   for (int i=0; i<NumAttributes; ++i) {
      std::stringstream name;
      name << "name" << i;
      p_name[i] = addStringParameter(name.str(), "attribute name", "");

      std::stringstream value;
      value << "value" << i;
      p_value[i] = addStringParameter(value.str(), "attribute value", "");
   }
}

AddAttribute::~AddAttribute() {

}

bool AddAttribute::compute() {

   Object::const_ptr obj = expect<Object>("data_in");
   if (!obj)
      return true;

   Object::ptr out = obj->clone();

   for (int i=0; i<NumAttributes; ++i) {
      if (!p_name[i]->getValue().empty()) {
         out->addAttribute(p_name[i]->getValue(), p_value[i]->getValue());
      }
   }

   addObject("data_out", out);

   return true;
}

MODULE_MAIN(AddAttribute)

