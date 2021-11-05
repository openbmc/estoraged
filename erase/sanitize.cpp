#include <linux/mmc/ioctl.h>
#include "sanitize.hpp"

// ioctl uses a variadic; wrapper_ioctl is easy to pass a std::function
int wrapper_ioctl(int fd, unsigned long request, struct mmc_ioc_cmd data){
  return ioctl(fd,request, data);
}


sanitize::sanitize(std::string devPath, unsigned int delaySec, std::function<int(int, long unsigned, struct mmc_ioc_cmd)> inIOctl )
  {
       int fd = open(devPath.c_str(), O_RDONLY, O_NDELAY);
       std::chrono::seconds delaytime(delaySec);
       if (fd < 0){
          sd_journal_send("MESSAGE=%s", "eStorageD eras sanitize failed to open block",
                          "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                          "REDFISH_MESSAGE_ARGS=%d", fd, NULL);
          return;
       }
       //write the extcsd
       struct mmc_ioc_cmd idata;
       memset(&idata, 0, sizeof(idata));
       idata.write_flag = 1;
       idata.opcode = MMC_SWITCH;
       idata.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                       (EXT_CSD_SANITIZE_START << 16) |
                       (1 << 8) |
                       EXT_CSD_CMD_SET_NORMAL;
       idata.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
       //make the ioctl
       int ret = inIOctl(fd, MMC_IOC_CMD, idata);

       if (ret != 0){
          sd_journal_send("MESSAGE=%s", "eStorageD erase sanitize Failure to set extcsd",
                                "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                                "REDFISH_MESSAGE_ARGS=%d", ret, NULL);
          return;
       }
       {

         //TODO eMMC spec allow sansize to take 20 mintues TODO double check
         std::this_thread::sleep_for(delaytime);

       sd_journal_send("MESSAGE=%s", "eStorageD successfully erase keys",
                     "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseSuccessfull", NULL);
       }
    }

