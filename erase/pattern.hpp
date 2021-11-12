#pragma once

#include <string>

class pattern{
  public:
   pattern(std::string devName);
   int writePattern();
   int verifyPattern();

  private:
    std::string m_devName;
    int openAndGetSize();
    int m_fd;
    unsigned long m_driveSize;
};
