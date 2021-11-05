#pragma once

#include "erase.hpp"

#include <linux/mmc/ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <stdplus/fd/managed.hpp>

#include <cstddef>
#include <span>
#include <string_view>

constexpr uint32_t EXT_CSD_ERASE_GROUP_DEF = 175;
constexpr uint32_t EXT_CSD_ERASE_TIMEOUT_MULT = 223;
constexpr uint32_t EXT_CSD_HC_ERASE_GRP_SIZE = 224;

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

    virtual ~IOCTLWrapperInterface()
    {}
};

// mockIOCTLWapper also inherits from IOCTLWrapperInterface
class IOCTLWrapperImpl : public IOCTLWrapperInterface
{
  public:
    int doIoctl(std::string_view devPath, unsigned long request,
                struct mmc_ioc_cmd data) override;
    int doIoctlMulti(std::string_view devPath, unsigned long request,
                     struct mmc_io_multi_cmd_erase) override;
    ~IOCTLWrapperImpl();
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
    ~Sanitize()
    {}

    /** @brief sanitize the drive, using eMMC specifed erase commands
     *
     * param[in] driveSize - size of the drive in bytes
     * param[in] extCsd - a span holding an eMMC CSD
     */
    void doSanitize(uint64_t driveSize, std::span<std::byte>& extCsd);

    /** @brief reads the extended Device-Specific Data(extCsd)
     *
     * param[out] extCsd
     */
    void readExtCsd(std::span<std::byte>& extCsd);

  private:
    /* Wrapper for ioctl*/
    std::unique_ptr<IOCTLWrapperInterface> ioctlWrapper;

    /** @brief uses the eMMC defined sanitize command, it is not the same as
     * vendor_sanitize  */
    void emmcSanitize();

    /** @brief uses the eMMC defined erase command
     *
     * param[in] driveSize - size of the drive in bytes
     * param[in] extCsd - the extended Device-Specific Data(extCsd)
     */
    void emmcErase(uint64_t driveSize, std::span<std::byte>& extCsd);
};

// can't use the real mmc_ioc_multi_cmd b/c of zero length array
// see uapi/linux/mmc/ioctl.h
struct mmc_io_multi_cmd_erase
{
    uint64_t num_of_cmds;
    struct mmc_ioc_cmd cmds[3];
};

} // namespace estoraged
