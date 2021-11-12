#pragma once

#include "erase.hpp"

#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>

#include <span>
#include <string>

using stdplus::fd::ManagedFd;

class Pattern : public Erase
{
  public:
    Pattern(std::string_view inDevPath) : Erase(inDevPath)
    {}
    int writePattern(uint64_t driveSize, ManagedFd& fd);
    int verifyPattern(uint64_t driveSize, ManagedFd& fd);
    //    ManagedFd open(const char* devPath, stdplus::fd::OpenAccess);

  private:
    bool spanEq(std::span<std::byte>& firstSpan,
                std::span<std::byte>& secondSpan, std::size_t size);
};
