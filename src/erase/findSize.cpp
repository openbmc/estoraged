#include "verifyGeometry.hpp"

#define BLKGETSIZE64 _IOR(0x12,114,size_t) /* return device size in bytes (u64 *arg) */


extern int findSizeOfBlockDevice(std::string devPath, unsigned long &bytes)
{
   //blocking open
   int fd =  open(devPath.c_str(), O_RDONLY, O_NDELAY );
   if (fd <= 0){
     sd_journal_send("MESSAGE=%s", "eStorageD erase unable to open blockdev",
                     "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                     "REDFISH_MESSAGE_ARGS=%d,%s", fd, devPath, NULL);
      return fd;
    } 
    // get block size
    int rc = ioctl(fd, BLKGETSIZE64, &bytes);
    if (rc <= 0){
     sd_journal_send("MESSAGE=%s", "eStorageD erase unable to get size of device",
                     "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                     "REDFISH_MESSAGE_ARGS=%d,%s", rc, devPath, NULL);
    } 
    return rc;
}


