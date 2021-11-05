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

void Sanitize::doSanitize(uint64_t driveSize, std::array<std::byte, 512> extCsd)
{
    try
    {
        emmcErase(driveSize, extCsd);
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
void Sanitize::emmcErase(uint64_t driveSize, std::array<std::byte, 512> extCsd)
{

    // One popular eMMC does not appear to follow the spec
    // extCsdEraseGroupDef is 1, indicating high capacity erase is
    // supported. However, the erase call only does low capacity erases

    // The twiddle was added as a way to keep our code spec compliant and work
    // with a device that is not
    uint64_t eraseSize = 0;
    if (writeVerifyExtCsd(extCsdEraseGroupDef, 0))
    {
        eraseSize = 0x200; // 512; // default value see 6.6.34, or eMMC spec
    }
    else
    {
        /* eMMC erase does not use block size, but erase group size
         * erase groups size is represented in 512Kbyte increments
         * see eMMC 7.4.44 */
        eraseSize = static_cast<uint64_t>(
            0x80000 * static_cast<uint64_t>(extCsd[extCsdHcEraseGrpSize]));
    }
    // build the erase message
    struct mmc_io_multi_cmd_erase eraseCmd = {};

    eraseCmd.num_of_cmds = 3;
    eraseCmd.cmds[0].opcode = mmcEraseGroupStart;
    eraseCmd.cmds[0].arg = 0;
    eraseCmd.cmds[0].flags = mmcRspSpiR1 | mmcRspR1 | mmcCmdAc;
    eraseCmd.cmds[0].write_flag = 1;

    eraseCmd.cmds[1].opcode = mmcEraseGroupEnd;
    eraseCmd.cmds[1].arg = (driveSize / eraseSize);
    if (twiddleextCsdEraseGroupDef)
    {
        eraseCmd.cmds[1].arg--;
    }
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

bool Sanitize::writeVerifyExtCsd(uint8_t index, uint8_t value)
{
    writeExtCsd(index, value);
    return (readExtCsdIndex(index) == value);
}

// always gets fresh data
uint8_t Sanitize::readExtCsdIndex(uint8_t index)
{
    std::array<std::byte, 512> extCsd = readExtCsd();
    return to_integer<uint8_t>(extCsd[index]);
}

std::array<std::byte, 512> Sanitize::readExtCsd()
{
    std::array<std::byte, 512> extCsd{};
    try
    {
        ManagedFd fd = stdplus::fd::open(std::string(devPath).c_str(),
                                         stdplus::fd::OpenAccess::ReadOnly);
        struct mmc_ioc_cmd idata = {};
        idata.write_flag = 0;
        idata.opcode = mmcSendExtCsd;
        idata.arg = 0;
        idata.flags = mmcRspSpiR1 | mmcRspR1 | mmcCmdAdtc;
        idata.blksz = extCsd.size();
        idata.blocks = 1;
        mmc_ioc_cmd_set_data( // NOLINT (linux function make linter unhappy)
            idata, extCsd.data());
        (void)ioctlWrapper->doIoctl(devPath, MMC_IOC_CMD, idata);
    }
    catch (...)
    {
        lg2::error("eStorageD erase sanitize Failure to read extcsd",
                   "REDFISH_MESSAGE_ID",
                   std::string("eStorageD.1.0.EraseFailure"));
        throw InternalFailure();
    }
    return extCsd;
}

void Sanitize::writeExtCsd(uint8_t index, uint8_t value)
{
    try
    {
        ManagedFd fd = stdplus::fd::open(std::string(devPath).c_str(),
                                         stdplus::fd::OpenAccess::ReadOnly);

        struct mmc_ioc_cmd idata = {};
        idata.write_flag = 1;
        idata.opcode = mmcSwitch;
        idata.arg = (mmcSwitchModeWriteByte << 24) | index << 16 | value << 8 |
                    extCsdCmdSetNormal;
        idata.flags = mmcRspSpiR1B | mmcRspR1B | mmcCmdAc;
        (void)ioctlWrapper->doIoctl(devPath, MMC_IOC_CMD, idata);
    }
    catch (...)
    {
        lg2::error("eStorageD erase sanitize Failure to write extcsd",
                   "REDFISH_MESSAGE_ID",
                   std::string("eStorageD.1.0.EraseFailure"));
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
