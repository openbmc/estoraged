#include "sanitize.hpp"

#include <linux/mmc/ioctl.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <stdplus/handle/managed.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

#include <iostream>

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;
using stdplus::fd::ManagedFd;

Sanitize::Sanitize(std::string_view inDevPath) : Erase(inDevPath)
{}

void Sanitize::doSanitize()
{
    ManagedFd fd = stdplus::fd::open(std::string(devPath).c_str(),
                                     stdplus::fd::OpenAccess::ReadOnly);
    if (fd.get() < 0)
    {
        lg2::error("eStorageD eras sanitize failed to open block",
                   "REDFISH_MESSAGE_ID",
                   std::string("eStorageD.1.0.EraseFailure"),
                   "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
        throw EraseError();
    }
    // write the extcsd
    struct mmc_ioc_cmd idata;
    memset(&idata, 0, sizeof(idata));
    idata.write_flag = 1;
    idata.opcode = MMC_SWITCH;
    idata.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                (EXT_CSD_SANITIZE_START << 16) | (1 << 8) |
                EXT_CSD_CMD_SET_NORMAL;
    idata.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

    // make the ioctl
    int ret = wrapperIOCTL(fd.get(), MMC_IOC_CMD, idata);

    if (ret != 0)
    {
        lg2::error("eStorageD erase sanitize Failure to set extcsd",
                   "REDFISH_MESSAGE_ID",
                   std::string("eStorageD.1.0.EraseFailure"),
                   "REDFISH_MESSAGE_ARGS", std::to_string(ret));
        throw EraseError();
    }
    // TODO eMMC spec allow sansize to take 20 mintues TODO double check
    lg2::error("eStorageD successfully erase keys", "REDFISH_MESSAGE_ID",
               std::string("eStorageD.1.0.EraseSuccessfull"));
}

SanitizeImp::SanitizeImp(std::string_view inDevPath) : Sanitize(inDevPath)
{}

int SanitizeImp::wrapperIOCTL(int fd, unsigned long request,
                              struct mmc_ioc_cmd data)
{
    return ioctl(fd, request, data);
}
