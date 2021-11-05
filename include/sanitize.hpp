#pragma once

#include "erase.hpp"

#include <linux/mmc/ioctl.h>
#include <sys/types.h>

#include <stdplus/fd/managed.hpp>

#include <cstddef>
#include <span>
#include <string_view>

#define EXT_CSD_ERASE_GROUP_DEF 175
#define EXT_CSD_ERASE_TIMEOUT_MULT 223
#define EXT_CSD_HC_ERASE_GRP_SIZE 224

using stdplus::fd::ManagedFd;

class IOCTLWrapperInterface
{
  public:
    virtual int doIoctl(ManagedFd& fd, unsigned long request,
                        struct mmc_ioc_cmd data) = 0;
    virtual int doIoctlMulti(ManagedFd& fd, unsigned long request,
                             struct mmc_io_multi_cmd_erase data) = 0;
    virtual ~IOCTLWrapperInterface()
    {}
};

// mockIOCTLWapper also inherits from IOCTLWrapperInterface
class IOCTLWrapperImpl : public IOCTLWrapperInterface
{
  public:
    int doIoctl(ManagedFd& fd, unsigned long request,
                struct mmc_ioc_cmd data) override;
    int doIoctlMulti(ManagedFd& fd, unsigned long request,
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

// can't use the real mmc_io_multi_cmd b/c of zero lenght array
// see uapi/linux/mmc/ioctl.h
struct mmc_io_multi_cmd_erase
{
    uint64_t num_of_cmds;
    struct mmc_ioc_cmd cmds[3];
};
