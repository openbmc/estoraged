#include <string>

class Erase
{
  public:
    Erase(std::string inDevPath) : devPath(std::move(inDevPath))
    {}
    uint64_t findSizeOfBlockDevice();

  protected:
    std::string devPath;
};
