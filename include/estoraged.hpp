#pragma once

#include "cryptsetupInterface.hpp"
#include "filesystemInterface.hpp"
#include "util.hpp"

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
using sdbusplus::xyz::openbmc_project::Inventory::Item::server::Drive;
using sdbusplus::xyz::openbmc_project::Inventory::Item::server::Volume;

/** @class eStoraged
 *  @brief eStoraged object to manage a LUKS encrypted storage device.
 */
class EStoraged
{
  public:
    /** @brief Constructor for eStoraged
     *
     *  @param[in] server - sdbusplus asio object server
     *  @param[in] configPath - path of the config object from Entity Manager
     *  @param[in] devPath - path to device file, e.g. /dev/mmcblk0
     *  @param[in] luksName - name for the LUKS container
     *  @param[in] size - size of the drive in bytes
     *  @param[in] lifeTime - percent of lifetime remaining for a drive
     *  @param[in] partNumber - part number for the storage device
     *  @param[in] serialNumber - serial number for the storage device
     *  @param[in] locationCode - location code for the storage device
     *  @param[in] eraseMaxGeometry - max geometry to erase if it's specified
     *  @param[in] eraseMinGeometry - min geometry to erase if it's specified
     *  @param[in] driveType - type of drive, e.g. HDD vs SSD
     *  @param[in] driveProtocol - protocol used to communicate with drive
     *  @param[in] cryptInterface - (optional) pointer to CryptsetupInterface
     *    object
     *  @param[in] fsInterface - (optional) pointer to FilesystemInterface
     *    object
     */
    EStoraged(sdbusplus::asio::object_server& server,
              const std::string& configPath, const std::string& devPath,
              const std::string& luksName, uint64_t size, uint8_t lifeTime,
              const std::string& partNumber, const std::string& serialNumber,
              const std::string& locationCode, uint64_t eraseMaxGeometry,
              uint64_t eraseMinGeometry, const std::string& driveType,
              const std::string& driveProtocol,
              std::unique_ptr<CryptsetupInterface> cryptInterface =
                  std::make_unique<Cryptsetup>(),
              std::unique_ptr<FilesystemInterface> fsInterface =
                  std::make_unique<Filesystem>());

    /** @brief Destructor for eStoraged. */
    ~EStoraged();

    EStoraged& operator=(const EStoraged&) = delete;
    EStoraged(const EStoraged&) = delete;
    EStoraged(EStoraged&&) = default;
    EStoraged& operator=(EStoraged&&) = delete;

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

    /** @brief Get the path to the mapped crypt device. */
    std::string_view getCryptDevicePath() const;

  private:
    /** @brief Full path of the device file, e.g. /dev/mmcblk0. */
    std::string devPath;

    /** @brief Name of the LUKS container. */
    std::string containerName;

    /** @brief Mount point for the filesystem. */
    std::string mountPoint;

    /** @brief Max geometry to erase. */
    uint64_t eraseMaxGeometry;

    /** @brief Min geometry to erase. */
    uint64_t eraseMinGeometry;

    /** @brief Indicates whether the LUKS device is currently locked. */
    bool lockedProperty{false};

    /** @brief Pointer to cryptsetup interface object.
     *  @details This is used to mock out the cryptsetup functions.
     */
    std::unique_ptr<CryptsetupInterface> cryptIface;

    /** @brief Pointer to filesystem interface object.
     *  @details This is used to mock out filesystem operations.
     */
    std::unique_ptr<FilesystemInterface> fsIface;

    /** @brief Path where the mapped crypt device gets created. */
    const std::string cryptDevicePath;

    /** @brief D-Bus object server. */
    sdbusplus::asio::object_server& objectServer;

    /** @brief D-Bus interface for the logical volume. */
    std::shared_ptr<sdbusplus::asio::dbus_interface> volumeInterface;

    /** @brief D-Bus interface for the physical drive. */
    std::shared_ptr<sdbusplus::asio::dbus_interface> driveInterface;

    /** @brief D-Bus interface for the location type of the drive. */
    std::shared_ptr<sdbusplus::asio::dbus_interface> embeddedLocationInterface;

    /** @brief D-Bus interface for the location code of the drive. */
    std::shared_ptr<sdbusplus::asio::dbus_interface> locationCodeInterface;

    /** @brief D-Bus interface for the asset information. */
    std::shared_ptr<sdbusplus::asio::dbus_interface> assetInterface;

    /** @brief Association between chassis and drive. */
    std::shared_ptr<sdbusplus::asio::dbus_interface> association;

    /** @brief Indicates whether the LUKS header is on the disk. */
    Drive::DriveEncryptionState encryptionStatus{
        Drive::DriveEncryptionState::Unknown};

    /** @brief Format LUKS encrypted device.
     *
     *  @param[in] password - password to set for the LUKS device.
     */
    void formatLuksDev(std::vector<uint8_t> password);

    /** @brief check the LUKS header, for devPath
     *
     *  @returns a CryptHandle to the LUKS drive
     */
    CryptHandle loadLuksHeader();

    /** @brief Unlock the device.
     *
     *  @param[in] password - password to activate the LUKS device.
     */

    Drive::DriveEncryptionState findEncryptionStatus();

    void activateLuksDev(std::vector<uint8_t> password);

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
};

} // namespace estoraged
