#include "sanitize.hpp"

#include <linux/mmc/ioctl.h>
#include <sys/ioctl.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <stdplus/handle/managed.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

#include <string>
#include <string_view>

#define MMC_SWITCH 6
#define MMC_SWITCH_MODE_WRITE_BYTE 0x03
#define EXT_CSD_SANITIZE_START 165
#define EXT_CSD_CMD_SET_NORMAL (1 << 0)

#define MMC_RSP_PRESENT (1 << 0)
#define MMC_RSP_CRC (1 << 2)
#define MMC_RSP_BUSY (1 << 3)
#define MMC_RSP_OPCODE (1 << 4)

#define MMC_CMD_AC (0 << 5)

#define MMC_RSP_SPI_S1 (1 << 7)
#define MMC_RSP_SPI_BUSY (1 << 10)

#define MMC_RSP_SPI_R1 (MMC_RSP_SPI_S1)
#define MMC_RSP_SPI_R1B (MMC_RSP_SPI_S1 | MMC_RSP_SPI_BUSY)

#define MMC_RSP_R1B                                                            \
    (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE | MMC_RSP_BUSY)

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;
using stdplus::fd::ManagedFd;

void Sanitize::doSanitize()
{
    ManagedFd fd = stdplus::fd::open(std::string(devPath).c_str(),
                                     stdplus::fd::OpenAccess::ReadOnly);

    // write the extcsd
    struct mmc_ioc_cmd idata = {};
    idata.write_flag = 1;
    idata.opcode = MMC_SWITCH;
    idata.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                (EXT_CSD_SANITIZE_START << 16) | (1 << 8) |
                EXT_CSD_CMD_SET_NORMAL;
    idata.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

    // make the ioctl
    int ret = ioctlWrapper->doIoctl(fd, MMC_IOC_CMD, idata);

    if (ret != 0)
    {
        lg2::error("eStorageD erase sanitize Failure to set extcsd",
                   "REDFISH_MESSAGE_ID",
                   std::string("eStorageD.1.0.EraseFailure"),
                   "REDFISH_MESSAGE_ARGS", std::to_string(ret));
        throw EraseError();
    }
    lg2::info("eStorageD successfully erase keys", "REDFISH_MESSAGE_ID",
              std::string("eStorageD.1.0.EraseSuccessfull"));
}

IOCTLWrapperImpl::~IOCTLWrapperImpl()
{}

int IOCTLWrapperImpl::doIoctl(ManagedFd& fd, unsigned long request,
                              struct mmc_ioc_cmd data)
{
    return fd.ioctl(request, static_cast<void*>(&data));
}
