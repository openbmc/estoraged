#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/eStoraged/server.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace estoraged
{
using eStoragedInherit = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::server::eStoraged>;

/** @class eStoraged
 *  @brief eStoraged object to manage a LUKS encrypted storage device.
 */
class eStoraged : eStoragedInherit
{
  public:
    eStoraged(sdbusplus::bus::bus& bus, const char* path,
              const std::string& devPath, const std::string& containerName) :
        eStoragedInherit(bus, path),
        devPath(devPath), containerName(containerName),
        mountPoint("/mnt/" + containerName + "_fs")
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
    void erase(std::vector<uint8_t> password, EraseMethod eraseType) override;

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
    void changePassword(std::vector<uint8_t> oldPassword,
                        std::vector<uint8_t> newPassword) override;

    /** @brief Check if the LUKS device is currently locked. */
    bool is_locked(void) const;

    /** @brief Get the mount point for the filesystem on the LUKS device. */
    std::string_view getMountPoint(void) const;

  private:
    /** @brief Full path of the device file, e.g. /dev/mmcblk0. */
    std::string devPath;

    /** @brief Name of the LUKS container. */
    std::string containerName;

    /** @brief Mount point for the filesystem. */
    std::string mountPoint;

    /** @brief Format LUKS encrypted device. */
    void formatLuksDev(std::vector<uint8_t> password);

    /** @brief Unlock the device. */
    void activateLuksDev(std::vector<uint8_t> password);

    /** @brief Create the filesystem on the LUKS device.
     *  @details The LUKS device should already be activated, i.e. unlocked.
     */
    void createFilesystem();

    /** @brief Deactivate the LUKS device.
     *  @details The filesystem is assumed to unmounted already.
     */
    void deactivateLuksDev();

    /** @brief Mount the filesystem.
     *  @details The filesystem should already exist and the LUKS device should
     *  be unlocked already.
     */
    void mountFilesystem();

    /** @brief Unmount the filesystem. */
    void unmountFilesystem();
};

} // namespace estoraged
