#pragma once

#include "cryptsetupInterface.hpp"
#include "filesystemInterface.hpp"

#include <libcryptsetup.h>

#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/server/object.hpp>
#include <util.hpp>
#include <xyz/openbmc_project/Inventory/Item/Drive/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Volume/server.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace estoraged
{
using estoraged::Cryptsetup;
using estoraged::Filesystem;
using sdbusplus::xyz::openbmc_project::Inventory::Item::server::Volume;

/** @class eStoraged
 *  @brief eStoraged object to manage a LUKS encrypted storage device.
 */
class EStoraged
{
  public:
    /** @brief Constructor for eStoraged
     *
     *  @param[in] objectServer - sdbusplus asio object server
     *  @param[in] devPath - path to device file, e.g. /dev/mmcblk0
     *  @param[in] luksName - name for the LUKS container
     *  @param[in] cryptInterface - (optional) pointer to CryptsetupInterface
     *    object
     *  @param[in] fsInterface - (optional) pointer to FilesystemInterface
     *    object
     */
    EStoraged(sdbusplus::asio::object_server& objectServer,
              const std::string& devPath, const std::string& luksName,
              uint64_t size,
              std::unique_ptr<CryptsetupInterface> cryptInterface =
                  std::make_unique<Cryptsetup>(),
              std::unique_ptr<FilesystemInterface> fsInterface =
                  std::make_unique<Filesystem>());

    /** @brief Destructor for eStoraged. */
    ~EStoraged();

    EStoraged& operator=(const EStoraged&) = delete;
    EStoraged(const EStoraged&) = delete;
    EStoraged(EStoraged&&) = default;
    EStoraged& operator=(EStoraged&&) = default;

    /** @brief Format the LUKS encrypted device and create empty filesystem.
     *
     *  @param[in] password - password to set for the LUKS device.
     *  @param[in] type - filesystem type, e.g. ext4
     */
    void formatLuks(const std::vector<uint8_t>& password,
                    Volume::FilesystemType type);

    /** @brief Erase the contents of the storage device.
     *
     *  @param[in] eraseType - type of erase operation.
     */
    void erase(Volume::EraseMethod eraseType);

    /** @brief Unmount filesystem and lock the LUKS device.
     */
    void lock();

    /** @brief Unlock device and mount the filesystem.
     *
     *  @param[in] password - password for the LUKS device.
     */
    void unlock(std::vector<uint8_t> password);

    /** @brief Change the password for the LUKS device.
     *
     *  @param[in] oldPassword - old password for the LUKS device.
     *  @param[in] newPassword - new password for the LUKS device.
     */
    void changePassword(const std::vector<uint8_t>& oldPassword,
                        const std::vector<uint8_t>& newPassword);

    /** @brief Check if the LUKS device is currently locked. */
    bool isLocked() const;

    /** @brief Get the mount point for the filesystem on the LUKS device. */
    std::string_view getMountPoint() const;

  private:
    /** @brief D-Bus object server. */
    sdbusplus::asio::object_server& objectServer;

    /** @brief D-Bus interface for the logical volume. */
    std::shared_ptr<sdbusplus::asio::dbus_interface> volumeInterface;

    /** @brief D-Bus interface for the physical drive. */
    std::shared_ptr<sdbusplus::asio::dbus_interface> driveInterface;

    /** @brief Full path of the device file, e.g. /dev/mmcblk0. */
    std::string devPath;

    /** @brief Name of the LUKS container. */
    std::string containerName;

    /** @brief Mount point for the filesystem. */
    std::string mountPoint;

    /** @brief Indicates whether the LUKS device is currently locked. */
    bool lockedProperty;

    /** @brief Capacity of the physical device in bytes. */
    uint64_t capacityProperty;

    /** @brief Pointer to cryptsetup interface object.
     *  @details This is used to mock out the cryptsetup functions.
     */
    std::unique_ptr<CryptsetupInterface> cryptIface;

    /** @brief Pointer to filesystem interface object.
     *  @details This is used to mock out filesystem operations.
     */
    std::unique_ptr<FilesystemInterface> fsIface;

    /** @brief Format LUKS encrypted device.
     *
     *  @param[in] cd - initialized crypt_device struct for the device.
     *  @param[in] password - password to set for the LUKS device.
     */
    void formatLuksDev(struct crypt_device* cd, std::vector<uint8_t> password);

    /** @brief Unlock the device.
     *
     *  @param[in] cd - initialized crypt_device struct for the device.
     *  @param[in] password - password to activate the LUKS device.
     */
    void activateLuksDev(struct crypt_device* cd,
                         std::vector<uint8_t> password);

    /** @brief Create the filesystem on the LUKS device.
     *  @details The LUKS device should already be activated, i.e. unlocked.
     */
    void createFilesystem();

    /** @brief Deactivate the LUKS device.
     *  @details The filesystem is assumed to be unmounted already.
     */
    void deactivateLuksDev();

    /** @brief Mount the filesystem.
     *  @details The filesystem should already exist and the LUKS device should
     *  be unlocked already.
     */
    void mountFilesystem();

    /** @brief Unmount the filesystem. */
    void unmountFilesystem();

    /** @brief Set the locked property */
    void locked(bool newValue);
};

} // namespace estoraged
