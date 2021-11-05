#pragma once

#include "erase.hpp"

#include <linux/mmc/ioctl.h>
#include <sys/types.h>

#include <stdplus/fd/managed.hpp>

#include <string_view>

using stdplus::fd::ManagedFd;

class Sanitize : public Erase
{
  public:
    Sanitize(std::string_view inDevPath) : Erase(inDevPath)
    {}
    void doSanitize();

  protected:
    // for testing wrapper_ioctl is overriden
    virtual int wrapperIOCTL(ManagedFd& fd, unsigned long request,
                             struct mmc_ioc_cmd data) = 0;
};

class SanitizeImpl : public Sanitize
{
  public:
    SanitizeImpl(std::string_view inDevPath) : Sanitize(inDevPath)
    {}
    int wrapperIOCTL(ManagedFd& fd, unsigned long request,
                     struct mmc_ioc_cmd data) override;
};
