#include <string>

/** @class Erase
 *  @brief Erase object provides a base class for the specific erase types
 */
class Erase
{
  public:
    /** @brief creates an erase object
     *  @param inDevPath the linux path for the block device
     */
    Erase(std::string inDevPath) : devPath(std::move(inDevPath))
    {}

    /** @brief finds the size of the linux block device in bytes
     *  @return size of a block device using the devPath
     */
    uint64_t findSizeOfBlockDevice();

  protected:
    /* The linux path for the block device */
    std::string devPath;
};
