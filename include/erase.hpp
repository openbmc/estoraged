#include <string>

class erase
{
  public:
    erase(std::string inDevPath) : devPath(std::move(inDevPath))
    {}
    uint64_t findSizeOfBlockDevice();

  protected:
    std::string devPath;
};
