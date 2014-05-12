#ifndef VISTLE_DIRECTORY_H
#define VISTLE_DIRECTORY_H

#include <string>
#include <map>

#include "export.h"

#define SCAN_MODULES_ON_HUB

namespace vistle {

const int ModuleNameLength = 50;

struct AvailableModule {

   int hub;
   std::string name;
   std::string path;

   AvailableModule(): hub(-1) {}
};

typedef std::map<std::string, AvailableModule> AvailableMap;

V_UTILEXPORT bool scanModules(const std::string &directory, AvailableMap &available);

}

#endif
