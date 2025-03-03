#pragma once

#include "erase.hpp"

#include <linux/mmc/ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <stdplus/fd/managed.hpp>
#include <util.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace estoraged
{

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
                             struct MmcIoMultiCmdErase data) = 0;

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
                     struct MmcIoMultiCmdErase data) override;
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
    /** @brief Creates a sanitize erase object
     *
     * @param[in] inDevPath - the linux device path for the block device.
     * @param[in] IOCTLWrapper - This is a ioctl wrapper, it can be used for
     * testing
     */
    Sanitize(std::string_view inDevPath,
             std::unique_ptr<IOCTLWrapperInterface> inIOCTL =
                 std::make_unique<IOCTLWrapperImpl>()) :
        Erase(inDevPath), ioctlWrapper(std::move(inIOCTL))
    {}

    /** @brief sanitize the drive, using eMMC specified erase commands
     *
     * param[in] driveSize - size of the drive in bytes
     */
    void doSanitize(uint64_t driveSize);

    /** @brief sanitize the drive, using eMMC specified erase commands
     *   This function uses the built in utils to call sanitize
     */
    void doSanitize()
    {
        doSanitize(util::findSizeOfBlockDevice(devPath));
    }

  private:
    /* Wrapper for ioctl*/
    std::unique_ptr<IOCTLWrapperInterface> ioctlWrapper;

    /** @brief uses the eMMC defined sanitize command, it is not the same as
     * vendor_sanitize  */
    void emmcSanitize();

    /** @brief uses the eMMC defined erase command
     *
     * param[in] driveSize - size of the drive in bytes
     */
    void emmcErase(uint64_t driveSize);
};

// can't use the real mmc_ioc_multi_cmd b/c of zero length array
// see uapi/linux/mmc/ioctl.h
struct MmcIoMultiCmdErase
{
    uint64_t num_of_cmds;
    struct mmc_ioc_cmd cmds[3]; // NOLINT (c arrays usage)
};

} // namespace estoraged
