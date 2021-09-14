#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/eStoraged/server.hpp>

#include <string>
#include <vector>

namespace estoraged
{
using eStoragedInherit = sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::server::eStoraged>;

/** @class eStoraged
 *  @brief eStoraged object to manage a LUKS encrypted storage device.
 */
class eStoraged : eStoragedInherit
{
  public:
    eStoraged(sdbusplus::bus::bus& bus, const char* path, std::string devPath,
              std::string containerName) :
      eStoragedInherit(bus, path), devPath(devPath),
      containerName(containerName)
    {}

    /** @brief Format the LUKS encrypted device and create empty filesystem.
     *
     *  @param[in] password - password to set for the LUKS device.
     */
    void format(std::vector<uint8_t> password) override;

    /** @brief Erase the contents of the storage device.
     *
     *  @param[in] password - password for the LUKS device.
     *  @param[in] eraseType - type of erase operation.
     */
    void erase(std::vector<uint8_t> password,
               EraseMethod eraseType) override;

    /** @brief Unmount filesystem and lock the LUKS device.
     *
     *  @param[in] password - password for the LUKS device.
     */
    void lock(std::vector<uint8_t> password) override;

    /** @brief Unlock device and mount the filesystem.
     *
     *  @param[in] password - password for the LUKS device.
     */
    void unlock(std::vector<uint8_t> password) override;

    /** @brief Change the password for the LUKS device.
     *
     *  @param[in] oldPassword - old password for the LUKS device.
     *  @param[in] newPassword - new password for the LUKS device.
     */
    void changePassword(
        std::vector<uint8_t> oldPassword,
        std::vector<uint8_t> newPassword) override;

  private:

    /* Full path of the device file, e.g. /dev/mmcblk0 */
    std::string devPath;

    /* Name of the LUKS container. */
    std::string containerName;
};

} // namespace openbmc
