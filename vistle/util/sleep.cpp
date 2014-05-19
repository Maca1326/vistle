#include "sleep.h"
#include <map>

#ifdef _WIN32
#else
#include <unistd.h>
#endif

namespace vistle {

bool adaptive_wait(bool work, const void *client) {

   static std::map<const void *, long> idleMap;
   const long Sec = 1000000; // 1 s
   const long MinDelay = 10000; // 10 ms
   const long MaxDelay = Sec;

   auto it = idleMap.find(client);
   if (it == idleMap.end())
      it = idleMap.insert(std::make_pair(client, 0)).first;

   auto &idle = it->second;

   if (work) {
      idle = 0;
      return false;
   }

   auto delay = idle;
   if (delay < MinDelay)
      delay = MinDelay;
   if (delay > MaxDelay)
      delay = MaxDelay;

   idle += delay;

   if (delay < Sec)
      usleep(delay);
   else
      sleep(delay/Sec);

   return true;
}

}
