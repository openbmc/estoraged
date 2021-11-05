#include "sanitize.hpp"

#include "estoraged_conf.hpp"

#include <linux/mmc/ioctl.h>
#include <sys/ioctl.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <stdplus/handle/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

constexpr uint32_t MMC_SWITCH = 6;
constexpr uint32_t MMC_SEND_EXT_CSD = 8;
constexpr uint32_t MMC_SWITCH_MODE_WRITE_BYTE = 0x03;
constexpr uint32_t EXT_CSD_SANITIZE_START = 165;
constexpr uint32_t EXT_CSD_CMD_SET_NORMAL = (1 << 0);

constexpr uint32_t MMC_RSP_PRESENT = (1 << 0);
constexpr uint32_t MMC_RSP_CRC = (1 << 2);
constexpr uint32_t MMC_RSP_BUSY = (1 << 3);
constexpr uint32_t MMC_RSP_OPCODE = (1 << 4);

constexpr uint32_t MMC_CMD_AC = (0 << 5);
constexpr uint32_t MMC_CMD_ADTC = (1 << 5);

constexpr uint32_t MMC_RSP_SPI_S1 = (1 << 7);
constexpr uint32_t MMC_RSP_SPI_BUSY = (1 << 10);

constexpr uint32_t MMC_RSP_SPI_R1 = (MMC_RSP_SPI_S1);
constexpr uint32_t MMC_RSP_SPI_R1B = (MMC_RSP_SPI_S1 | MMC_RSP_SPI_BUSY);

#define MMC_RSP_R1B                                                            \
    (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE | MMC_RSP_BUSY)

#define MMC_RSP_R1 (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)

constexpr uint32_t MMC_ERASE_GROUP_START = 35;
constexpr uint32_t MMC_ERASE_GROUP_END = 36;
constexpr uint32_t MMC_ERASE = 38;

namespace estoraged
{

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using stdplus::fd::ManagedFd;

void Sanitize::doSanitize(uint64_t driveSize, std::span<std::byte>& extCsd)
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
void Sanitize::emmcErase(uint64_t driveSize, std::span<std::byte>& extCsd)
{
    // One popular eMMC does not appear to follow the spec
    // extCsdEraseGroupDef is 1, indicating high capacity erase is
    // supported. However, the erase call only does low capacity erases

    // The twiddle was added as a way to keep our code spec compliant and work
    // with a device that is not
    uint64_t eraseSize;
    // twiddleextCsdEraseGroupDef
    if (writeVerifyExtCsd(extCsdEraseGroupDef, 0x1))
    {
        eraseSize =
            524288 * static_cast<uint32_t>(extCsd[extCsdHcEraseGrpSize]);
    }
    else
    {
        eraseSize = 512; // default value see 6.6.34, or eMMC spec
    }
    // build the erase message
    struct mmc_io_multi_cmd_erase erase_cmd = {};

    erase_cmd.num_of_cmds = 3;
    /* Set erase start address */
    erase_cmd.cmds[0].opcode = MMC_ERASE_GROUP_START;
    /* start erase addr */
    erase_cmd.cmds[0].arg = 0;
    erase_cmd.cmds[0].flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;
    erase_cmd.cmds[0].write_flag = 1;

    /* Set erase end address */
    erase_cmd.cmds[1].opcode = MMC_ERASE_GROUP_END;
    /* eMMC erase does not use block size, but erase group size
     * erase groups size is represented in 512Kbyte increments
     * see eMMC 7.4.44 */
    erase_cmd.cmds[1].arg = (driveSize / eraseSize);
    static_cast<uint32_t>(extCsd[extCsdHcEraseGrpSize]);

    /*if (twiddleextCsdEraseGroupDef)
    {
        erase_cmd.cmds[1].arg--;
    }
    */
    erase_cmd.cmds[1].flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;
    erase_cmd.cmds[1].write_flag = 1;

    /* Send Erase Command */
    erase_cmd.cmds[2].opcode = MMC_ERASE;
    /*Secure Erase */
    erase_cmd.cmds[2].arg = 0x00000000;
    erase_cmd.cmds[2].cmd_timeout_ms =
        300 * static_cast<unsigned int>(extCsd[extCsdEraseTimeoutMult]) * 255;
    erase_cmd.cmds[2].flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
    erase_cmd.cmds[2].write_flag = 1;

    if (ioctlWrapper->doIoctlMulti(devPath, MMC_IOC_MULTI_CMD, erase_cmd) != 0)
    {
        throw InternalFailure();
    }
}

// the eMMC specification defined "sanitize"
void Sanitize::emmcSanitize()
{

    struct mmc_ioc_cmd idata = {};
    idata.write_flag = 1;
    idata.opcode = MMC_SWITCH;
    idata.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                (EXT_CSD_SANITIZE_START << 16) | (1 << 8) |
                EXT_CSD_CMD_SET_NORMAL;
    idata.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

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
    std::byte _ExtCsd[512] = {};
    std::span<std::byte> extCsd(_ExtCsd);

    readExtCsd(extCsd);
    return to_integer<uint8_t>(extCsd[index]);
}

void Sanitize::readExtCsd(std::span<std::byte>& extCsd)
{
    try
    {
        ManagedFd fd = stdplus::fd::open(std::string(devPath).c_str(),
                                         stdplus::fd::OpenAccess::ReadOnly);
        struct mmc_ioc_cmd idata = {};
        idata.write_flag = 0;
        idata.opcode = MMC_SEND_EXT_CSD;
        idata.arg = 0;
        idata.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
        idata.blksz = extCsd.size();
        idata.blocks = 1;
        mmc_ioc_cmd_set_data(idata, extCsd.data());
        (void)ioctlWrapper->doIoctl(devPath, MMC_IOC_CMD, idata);
    }
    catch (...)
    {
        lg2::error("eStorageD erase sanitize Failure to read extcsd",
                   "REDFISH_MESSAGE_ID",
                   std::string("eStorageD.1.0.EraseFailure"));
        throw InternalFailure();
    }
}

void Sanitize::writeExtCsd(uint8_t index, uint8_t value)
{
    try
    {
        ManagedFd fd = stdplus::fd::open(std::string(devPath).c_str(),
                                         stdplus::fd::OpenAccess::ReadOnly);

        struct mmc_ioc_cmd idata = {};
        idata.write_flag = 1;
        idata.opcode = MMC_SWITCH;
        idata.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) | index << 16 |
                    value << 8 | EXT_CSD_CMD_SET_NORMAL;
        idata.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
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
