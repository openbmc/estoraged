#include <string>

class erase
{
  public:
    erase(std::string inDevPath) : devPath(std::move(inDevPath))
    {}

  protected:
    std::string devPath;
};
