#pragma once

#include "erase.hpp"

#include <string>

class pattern : public Erase
{
  public:
    pattern(std::string_view inDevPath) : Erase(inDevPath)
    {}
    int writePattern();
    int verifyPattern();

    //    std::string m_devName;
    //    int openAndGetSize();
    //    int m_fd;
    //    unsigned long m_driveSize;
};
