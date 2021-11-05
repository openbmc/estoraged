#pragma once

#include "erase.hpp"

#include <linux/mmc/ioctl.h>
#include <sys/types.h>

#include <stdplus/fd/managed.hpp>

#include <string_view>

using stdplus::fd::ManagedFd;

class IOCTLWrapperInterface
{
  public:
    virtual int doIoctl(ManagedFd& fd, unsigned long request,
                        struct mmc_ioc_cmd data) = 0;
    virtual ~IOCTLWrapperInterface()
    {}
};

// mockIOCTLWapper also inherits from IOCTLWrapperInterface
class IOCTLWrapperImpl : public IOCTLWrapperInterface
{
  public:
    int doIoctl(ManagedFd& fd, unsigned long request,
                struct mmc_ioc_cmd data) override;
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
    void doSanitize();

  private:
    std::unique_ptr<IOCTLWrapperInterface> ioctlWrapper;
};
