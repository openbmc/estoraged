#pragma once

#include "erase.hpp"

#include <linux/mmc/ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <stdplus/fd/managed.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace estoraged
{

using stdplus::fd::ManagedFd;

class IOCTLWrapperInterface
{
  public:
    /** @brief Wrapper around ioctl
     *  @details Used for mocking purposes.
     *
     * @param[in] devPath - File name of block device
     * @param[in] request - Device-dependent request code
     * @param[in] mmc_ioc_cmd - eMMC cmd
     */
    virtual int doIoctl(std::string_view devPath, unsigned long request,
                        struct mmc_ioc_cmd data) = 0;

    /** @brief Wrapper around ioctl
     *  @details Used for mocking purposes.
     *
     * @param[in] devPath - File name of block device
     * @param[in] request - Device-dependent request code
     * @param[in] mmc_io_mutli_cmd - many eMMC cmd
     */
    virtual int doIoctlMulti(std::string_view devPath, unsigned long request,
                             struct mmc_io_multi_cmd_erase data) = 0;

    virtual ~IOCTLWrapperInterface() = default;
    IOCTLWrapperInterface() = default;
    IOCTLWrapperInterface(const IOCTLWrapperInterface&) = delete;
    IOCTLWrapperInterface& operator=(const IOCTLWrapperInterface&) = delete;

    IOCTLWrapperInterface(IOCTLWrapperInterface&&) = delete;
    IOCTLWrapperInterface& operator=(IOCTLWrapperInterface&&) = delete;
};

// mockIOCTLWapper also inherits from IOCTLWrapperInterface
class IOCTLWrapperImpl : public IOCTLWrapperInterface
{
  public:
    int doIoctl(std::string_view devPath, unsigned long request,
                struct mmc_ioc_cmd data) override;
    int doIoctlMulti(std::string_view devPath, unsigned long request,
                     struct mmc_io_multi_cmd_erase data) override;
    ~IOCTLWrapperImpl() override = default;
    IOCTLWrapperImpl() = default;

    IOCTLWrapperImpl(const IOCTLWrapperImpl&) = delete;
    IOCTLWrapperImpl& operator=(const IOCTLWrapperImpl&) = delete;

    IOCTLWrapperImpl(IOCTLWrapperImpl&&) = delete;
    IOCTLWrapperImpl& operator=(IOCTLWrapperImpl&&) = delete;
};

class Sanitize : public Erase
{
  public:
    /** @breif Creates a sanitize erase object
     *
     * @param[in] inDevPath - the linux device path for the block device.
     * @param[in] IOCTLWrapper - This is a ioctl wrapper, it can be used for
     * testing
     */
    Sanitize(std::string_view inDevPath,
             std::unique_ptr<IOCTLWrapperInterface> inIOCTL =
                 std::make_unique<IOCTLWrapperImpl>()) :
        Erase(inDevPath),
        ioctlWrapper(std::move(inIOCTL))
    {}

    /** @brief sanitize the drive, using eMMC specifed erase commands
     *
     * param[in] driveSize - size of the drive in bytes
     * param[in] extCsd - a span holding an eMMC CSD
     */
    void doSanitize(uint64_t driveSize, std::array<std::byte, 512> extCsd);

    /** @brief reads the extended Device-Specific Data(extCsd)
     *
     * param[out] extCsd
     */
    std::array<std::byte, 512> readExtCsd();

  private:
    const uint32_t extCsdEraseGroupDef = 175;
    const uint32_t extCsdEraseTimeoutMult = 223;
    const uint32_t extCsdHcEraseGrpSize = 224;

    /* Wrapper for ioctl*/
    std::unique_ptr<IOCTLWrapperInterface> ioctlWrapper;

    /** @brief uses the eMMC defined sanitize command, it is not the same as
     * vendor_sanitize  */
    void emmcSanitize();

    /** @breif writes the csd and returns a bool.
     * true if the write worked, and false the write did not work
     */
    bool writeVerifyExtCsd(uint8_t index, uint8_t value);

    /** @breif reads the extcsd and returns the value at the index.
     * For many ext lookups use the readExtCsd function for better performance.
     *
     * param[in] index, the index to be written
     * param[in] value, the value that will be written
     * return true if the value was written and read to be the same, otherwise
     * false
     */
    uint8_t readExtCsdIndex(uint8_t index);
    /** @brief uses the eMMC defined erase command
     *
     * param[in] driveSize - size of the drive in bytes
     * param[in] extCsd - the extended Device-Specific Data(extCsd)
     */
    void emmcErase(uint64_t driveSize, std::array<std::byte, 512> extCsd);

    /** @breif writes a value at index in the csd
     *
     * param[in] index, the index to be written
     * param[in] value, the value that will be written
     */
    void writeExtCsd(uint8_t index, uint8_t value);
};

// can't use the real mmc_ioc_multi_cmd b/c of zero length array
// see uapi/linux/mmc/ioctl.h
struct mmc_io_multi_cmd_erase
{
    uint64_t num_of_cmds;
    struct mmc_ioc_cmd cmds[3]; // NOLINT (c arrays usage)
};

} // namespace estoraged
