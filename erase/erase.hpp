#include <string>

class erase{
  public:
    erase(std::string in_devPath): m_devPath(in_devPath)
     {}
  protected:
     std::string m_devPath;
};
