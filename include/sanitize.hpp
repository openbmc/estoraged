#pragma once

#include "erase.hpp"

#include <linux/mmc/ioctl.h>
#include <stdarg.h>
#include <sys/types.h>

#include <stdplus/fd/managed.hpp>

using stdplus::fd::ManagedFd;

class Sanitize : public Erase
{
  public:
    Sanitize(std::string_view inDevPath);
    void doSanitize();
    ~Sanitize()
    {}

  private:
    // for testing wrapper_ioctl is overriden
    virtual int wrapperIOCTL(ManagedFd& fd, unsigned long request,
                             struct mmc_ioc_cmd data) = 0;
};

class SanitizeImp : public Sanitize
{
  public:
    SanitizeImp(std::string_view inDevPath);
    ~SanitizeImp()
    {}
    int wrapperIOCTL(ManagedFd& fd, unsigned long request,
                     struct mmc_ioc_cmd data) override;
};
