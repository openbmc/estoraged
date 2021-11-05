#include "sanitize.hpp"

#include "estoraged_conf.hpp"

#include <linux/mmc/ioctl.h>
#include <sys/ioctl.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <stdplus/handle/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

constexpr uint32_t mmcSwitch = 6;
constexpr uint32_t mmcSendExtCsd = 8;
constexpr uint32_t mmcSwitchModeWriteByte = 0x03;
constexpr uint32_t extCsdSanitizeStart = 165;
constexpr uint32_t extCsdCmdSetNormal = (1 << 0);

constexpr uint32_t mmcRspPresent = (1 << 0);
constexpr uint32_t mmcRspCrc = (1 << 2);
constexpr uint32_t mmcRspBusy = (1 << 3);
constexpr uint32_t mmcRspOpcode = (1 << 4);

constexpr uint32_t mmcCmdAc = (0 << 5);
constexpr uint32_t mmcCmdAdtc = (1 << 5);

constexpr uint32_t mmcRspSpiS1 = (1 << 7);
constexpr uint32_t mmcRspSpiBusy = (1 << 10);

constexpr uint32_t mmcRspSpiR1 = (mmcRspSpiS1);
constexpr uint32_t mmcRspSpiR1B = (mmcRspSpiS1 | mmcRspSpiBusy);

constexpr uint32_t mmcRspR1B =
    (mmcRspPresent | mmcRspCrc | mmcRspOpcode | mmcRspBusy);

constexpr uint32_t mmcRspR1 = (mmcRspPresent | mmcRspCrc | mmcRspOpcode);

constexpr uint32_t mmcEraseGroupStart = 35;
constexpr uint32_t mmcEraseGroupEnd = 36;
constexpr uint32_t mmcErase = 38;

namespace estoraged
{

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using stdplus::fd::ManagedFd;

void Sanitize::doSanitize(uint64_t driveSize)
{
    try
    {
        emmcErase(driveSize);
        emmcSanitize();
    }
    catch (...)
    {
        lg2::error("eStorageD erase sanitize failure", "REDFISH_MESSAGE_ID",
                   std::string("eStorageD.1.0.EraseFailure"));
        throw InternalFailure();
    }
    lg2::info("eStorageD successfully erase sanitize", "REDFISH_MESSAGE_ID",
              std::string("eStorageD.1.0.EraseSuccessful"));
}

// the eMMC specified erase
void Sanitize::emmcErase(uint64_t driveSize)
{

    uint64_t sectorSize = 0x200; // default value see 6.6.34, or eMMC spec
    struct mmc_io_multi_cmd_erase eraseCmd = {};

    eraseCmd.num_of_cmds = 3;
    eraseCmd.cmds[0].opcode = mmcEraseGroupStart;
    eraseCmd.cmds[0].arg = 0;
    eraseCmd.cmds[0].flags = mmcRspSpiR1 | mmcRspR1 | mmcCmdAc;
    eraseCmd.cmds[0].write_flag = 1;

    eraseCmd.cmds[1].opcode = mmcEraseGroupEnd;
    eraseCmd.cmds[1].arg = (driveSize / sectorSize) - 1;
    eraseCmd.cmds[1].flags = mmcRspSpiR1 | mmcRspR1 | mmcCmdAc;
    eraseCmd.cmds[1].write_flag = 1;

    /* Send Erase Command */
    eraseCmd.cmds[2].opcode = mmcErase;
    /*Secure Erase */
    eraseCmd.cmds[2].arg = 0x00000000;
    eraseCmd.cmds[2].cmd_timeout_ms = 0x0FFFFFFF;
    eraseCmd.cmds[2].flags = mmcRspSpiR1B | mmcRspR1B | mmcCmdAc;
    eraseCmd.cmds[2].write_flag = 1;

    if (ioctlWrapper->doIoctlMulti(devPath, MMC_IOC_MULTI_CMD, eraseCmd) != 0)
    {
        throw InternalFailure();
    }
}

// the eMMC specification defined "sanitize"
void Sanitize::emmcSanitize()
{

    struct mmc_ioc_cmd idata = {};
    idata.write_flag = 1;
    idata.opcode = mmcSwitch;
    idata.arg = (mmcSwitchModeWriteByte << 24) | (extCsdSanitizeStart << 16) |
                (1 << 8) | extCsdCmdSetNormal;
    idata.flags = mmcRspSpiR1B | mmcRspR1B | mmcCmdAc;

    // make the eMMC sanitize ioctl
    if (ioctlWrapper->doIoctl(devPath, MMC_IOC_CMD, idata) != 0)
    {
        throw InternalFailure();
    }
}

int IOCTLWrapperImpl::doIoctl(std::string_view devPath, unsigned long request,
                              struct mmc_ioc_cmd data)

{
    ManagedFd fd = stdplus::fd::open(std::string(devPath).c_str(),
                                     stdplus::fd::OpenAccess::ReadOnly);

    return fd.ioctl(request, static_cast<void*>(&data));
}

int IOCTLWrapperImpl::doIoctlMulti(std::string_view devPath,
                                   unsigned long request,
                                   struct mmc_io_multi_cmd_erase data)
{
    ManagedFd fd = stdplus::fd::open(std::string(devPath).c_str(),
                                     stdplus::fd::OpenAccess::ReadOnly);

    return fd.ioctl(request, static_cast<void*>(&data));
}

} // namespace estoraged
