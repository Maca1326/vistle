#ifndef MODULE_IMPL_H
#define MODULE_IMPL_H

namespace vistle {

template<class T>
bool Module::setParameter(const std::string &name, const T &value, const message::SetParameter *inResponseTo) {

   auto p = boost::dynamic_pointer_cast<ParameterBase<T>>(findParameter(name));
   if (!p)
      return false;

   return setParameter(p, value, inResponseTo);
}

template<class T>
bool Module::setParameter(boost::shared_ptr<ParameterBase<T>> param, const T &value, const message::SetParameter *inResponseTo) {

   param->setValue(value);
   parameterChanged(*param);
   return updateParameter(param->getName(), param, inResponseTo);
}

template<class T>
bool Module::setParameterMinimum(boost::shared_ptr<ParameterBase<T>> param, const T &minimum) {

   return Module::setParameterRange(param, minimum, param->maximum());
}

template<class T>
bool Module::setParameterMaximum(boost::shared_ptr<ParameterBase<T>> param, const T &maximum) {

   return Module::setParameterRange(param, param->minimum(), maximum);
}

template<class T>
bool Module::setParameterRange(const std::string &name, const T &minimum, const T &maximum) {

   auto p = boost::dynamic_pointer_cast<ParameterBase<T>>(findParameter(name));
   if (!p)
      return false;

   return setParameterRange(p, minimum, maximum);
}

template<class T>
bool Module::setParameterRange(boost::shared_ptr<ParameterBase<T>> param, const T &minimum, const T &maximum) {

   bool ok = true;

   if (maximum > minimum) {
      param->setMinimum(minimum);
      param->setMaximum(maximum);
   } else {
      std::cerr << "range for parameter " << param->getName() << " swapped" << std::endl;
      param->setMinimum(maximum);
      param->setMaximum(minimum);
   }
   T value = param->getValue();
   if (value < param->minimum()) {
      std::cerr << "value for parameter " << param->getName() << " increased to minimum: " << param->minimum() << std::endl;
      param->setValue(param->minimum());
      ok &= updateParameter(param->getName(), param, nullptr);
   }
   if (value > param->maximum()) {
      std::cerr << "value for parameter " << param->getName() << " decreased to maximum: " << param->maximum() << std::endl;
      param->setValue(param->maximum());
      ok &= updateParameter(param->getName(), param, nullptr);
   }
   ok &= updateParameter(param->getName(), param, nullptr, Parameter::Minimum);
   ok &= updateParameter(param->getName(), param, nullptr, Parameter::Maximum);
   return ok;
}

template<class T>
bool Module::getParameter(const std::string &name, T &value) const {

   if (auto p = boost::dynamic_pointer_cast<ParameterBase<T>>(findParameter(name))) {
      value = p->getValue();
   } else {
      std::cerr << "Module::getParameter(" << name << "): type failure" << std::endl;
      vassert("dynamic_cast failed" == 0);
      return false;
   }

   return true;
}

} // namespace vistle
#endif
