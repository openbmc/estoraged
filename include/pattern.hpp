#pragma once

#include "erase.hpp"

#include <span>
#include <string>

class pattern : public Erase
{
  public:
    pattern(std::string_view inDevPath) : Erase(inDevPath)
    {}
    int writePattern(uint64_t driveSize);
    int verifyPattern(uint64_t driveSize);

  private:
    bool spanEq(std::span<std::byte>& firstSpan,
                std::span<std::byte>& secondSpan, std::size_t size);
};
