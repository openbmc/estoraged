#pragma once

#include "erase.hpp"

#include <linux/mmc/ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <stdplus/fd/managed.hpp>

#include <cstddef>
#include <span>
#include <string_view>

#define EXT_CSD_ERASE_GROUP_DEF 175
#define EXT_CSD_ERASE_TIMEOUT_MULT 223
#define EXT_CSD_HC_ERASE_GRP_SIZE 224

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
    Sanitize(std::string_view inDevPath,
             std::unique_ptr<IOCTLWrapperInterface> inIOCTL =
                 std::make_unique<IOCTLWrapperImpl>()) :
        Erase(inDevPath),
        ioctlWrapper(std::move(inIOCTL))
    {}
    ~Sanitize()
    {}
    void doSanitize(uint64_t driveSize, std::span<std::byte>& extCsd);
    void readExtCsd(std::span<std::byte>& extCsd);

  private:
    std::unique_ptr<IOCTLWrapperInterface> ioctlWrapper;
};

// can't use the real mmc_ioc_multi_cmd b/c of zero length array
// see uapi/linux/mmc/ioctl.h
struct mmc_io_multi_cmd_erase
{
    uint64_t num_of_cmds;
    struct mmc_ioc_cmd cmds[3];
};

} // namespace estoraged
