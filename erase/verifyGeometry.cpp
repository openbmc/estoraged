#include "verifyGeometry.hpp"
#include "estoraged_conf.hpp"
#include <unistd.h>

int verifyGeometry::GeometryOkay(int(*findSize)(std::string, unsigned long& bytes)){
   unsigned long bytes;
   if (findSize(m_devPath, bytes) != 0){
       return false;
   }
   if (bytes > ERASE_MAX_GEOMETRY){
     sd_journal_send("MESSAGE=%s", "eStorageD erase verify Geometry to large",
                     "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                     "REDFISH_MESSAGE_ARGS=%d >= %d", bytes, ERASE_MAX_GEOMETRY, NULL);
      return -1;
   }
   else if (bytes < ERASE_MIN_GEOMETRY){
      sd_journal_send("MESSAGE=%s", "eStorageD erase verify Geometry to small",
                     "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                     "REDFISH_MESSAGE_ARGS=%d <= %d", bytes, ERASE_MIN_GEOMETRY, NULL);
      return -1;
   }
   else{
    sd_journal_send("MESSAGE=%s", "eStorageD erase verify Geometry in range",
                     "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseSuccess",
                     "REDFISH_MESSAGE_ARGS=%d <= %d", bytes, ERASE_MIN_GEOMETRY, NULL);
   }
      return 0;
}
