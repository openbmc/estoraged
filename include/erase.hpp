#include <string>

/** @class Erase
 *  @brief Erase object provides a base class for the specific erase types
 */
class Erase
{
  public:
    Erase(std::string inDevPath) : devPath(std::move(inDevPath))
    {}
    /** @brief creates an erase object
     *  @param inDevPath the linux path for the block device
     */

    uint64_t findSizeOfBlockDevice();
    /** @brief finds the size of the linux block device in bytes
     *  @return size of a block device using the devPath
     */

  protected:
    std::string devPath; /**< The linux path for the block device
                          */
};
