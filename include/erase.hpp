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
    /** @brief creates an erase object
     *  @param inDevPath the linux path for the block device
     */
    Erase(std::string_view inDevPath) : devPath(inDevPath)
    {}
    virtual ~Erase() = default;

    /** @brief finds the size of the linux block device in bytes
     *  @return size of a block device using the devPath
     */
    uint64_t findSizeOfBlockDevice();

  protected:
    /* The linux path for the block device */
    std::string devPath;
};

} // namespace estoraged
