#pragma once
#include <string>

namespace estoraged
{
/** @class Erase
 *  @brief Erase object provides a base class for the specific erase types
 */
class Erase
{
  public:
    virtual ~Erase() = default;

    /** @brief creates an erase object
     *  @param inDevPath the linux path for the block device
     */
    Erase(std::string_view inDevPath) : devPath(inDevPath) {}

    /** @brief returns the linux path for the block device */
    const std::string& getDevPath() const
    {
        return devPath;
    }

  private:
    /* The linux path for the block device */
    std::string devPath;
};

} // namespace estoraged
