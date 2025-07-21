
#include "estoraged.hpp"

#include "cryptErase.hpp"
#include "cryptsetupInterface.hpp"
#include "pattern.hpp"
#include "sanitize.hpp"
#include "verifyDriveGeometry.hpp"
#include "zero.hpp"

#include <libcryptsetup.h>
#include <openssl/rand.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace estoraged
{

using Association = std::tuple<std::string, std::string, std::string>;
using sdbusplus::asio::PropertyPermission;
using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using sdbusplus::xyz::openbmc_project::Common::Error::UnsupportedRequest;
using sdbusplus::xyz::openbmc_project::Inventory::Item::server::Drive;
using sdbusplus::xyz::openbmc_project::Inventory::Item::server::Volume;

EStoraged::EStoraged(
    sdbusplus::asio::object_server& server, const std::string& configPath,
    const std::string& devPath, const std::string& luksName, uint64_t size,
    uint8_t lifeTime, const std::string& partNumber,
    const std::string& serialNumber, const std::string& locationCode,
    uint64_t eraseMaxGeometry, uint64_t eraseMinGeometry,
    const std::string& driveType, const std::string& driveProtocol,
    std::unique_ptr<CryptsetupInterface> cryptInterface,
    std::unique_ptr<FilesystemInterface> fsInterface) :
    devPath(devPath), containerName(luksName),
    mountPoint("/mnt/" + luksName + "_fs"), eraseMaxGeometry(eraseMaxGeometry),
    eraseMinGeometry(eraseMinGeometry), cryptIface(std::move(cryptInterface)),
    fsIface(std::move(fsInterface)),
    cryptDevicePath(cryptIface->cryptGetDir() + "/" + luksName),
    objectServer(server)
{
    /* Get the filename of the device (without "/dev/"). */
    std::string deviceName = std::filesystem::path(devPath).filename().string();
    /* DBus object path */
    std::string objectPath =
        "/xyz/openbmc_project/inventory/storage/" + deviceName;

    /* Add Volume interface. */
    volumeInterface = objectServer.add_interface(
        objectPath, "xyz.openbmc_project.Inventory.Item.Volume");
    volumeInterface->register_method(
        "FormatLuks", [this](const std::vector<uint8_t>& password,
                             Volume::FilesystemType type) {
            this->formatLuks(password, type);
        });
    volumeInterface->register_method(
        "Erase",
        [this](Volume::EraseMethod eraseType) { this->erase(eraseType); });
    volumeInterface->register_method("Lock", [this]() { this->lock(); });
    volumeInterface->register_method(
        "Unlock",
        [this](std::vector<uint8_t>& password) { this->unlock(password); });
    volumeInterface->register_method(
        "ChangePassword", [this](const std::vector<uint8_t>& oldPassword,
                                 const std::vector<uint8_t>& newPassword) {
            this->changePassword(oldPassword, newPassword);
        });
    volumeInterface->register_property_r(
        "Locked", lockedProperty, sdbusplus::vtable::property_::emits_change,
        [this](bool& value) {
            value = this->isLocked();
            return value;
        });

    /* Add Drive interface. */
    driveInterface = objectServer.add_interface(
        objectPath, "xyz.openbmc_project.Inventory.Item.Drive");
    driveInterface->register_property("Capacity", size);
    /* The lifetime property is read/write only for testing purposes. */
    driveInterface->register_property("PredictedMediaLifeLeftPercent",
                                      lifeTime, PropertyPermission::readWrite);
    driveInterface->register_property(
        "Type",
        "xyz.openbmc_project.Inventory.Item.Drive.DriveType." + driveType);
    driveInterface->register_property(
        "Protocol", "xyz.openbmc_project.Inventory.Item.Drive.DriveProtocol." +
                        driveProtocol);
    /* This registers the Locked property for the Drives interface.
     * Now it is the same as the volume Locked property */
    driveInterface->register_property_r(
        "Locked", lockedProperty, sdbusplus::vtable::property_::emits_change,
        [this](bool& value) {
            value = this->isLocked();
            return value;
        });

    driveInterface->register_property_r(
        "EncryptionStatus", encryptionStatus,
        sdbusplus::vtable::property_::emits_change,
        [this](Drive::DriveEncryptionState& value) {
            value = this->findEncryptionStatus();
            return value;
        });

    embeddedLocationInterface = objectServer.add_interface(
        objectPath, "xyz.openbmc_project.Inventory.Connector.Embedded");

    if (!locationCode.empty())
    {
        locationCodeInterface = objectServer.add_interface(
            objectPath, "xyz.openbmc_project.Inventory.Decorator.LocationCode");
        locationCodeInterface->register_property("LocationCode", locationCode);
        locationCodeInterface->initialize();
    }

    /* Add Asset interface. */
    assetInterface = objectServer.add_interface(
        objectPath, "xyz.openbmc_project.Inventory.Decorator.Asset");
    assetInterface->register_property("PartNumber", partNumber);
    assetInterface->register_property("SerialNumber", serialNumber);

    volumeInterface->initialize();
    driveInterface->initialize();
    embeddedLocationInterface->initialize();
    assetInterface->initialize();

    /* Set up the association between chassis and drive. */
    association = objectServer.add_interface(
        objectPath, "xyz.openbmc_project.Association.Definitions");

    std::vector<Association> associations;
    associations.emplace_back("chassis", "drive",
                              std::filesystem::path(configPath).parent_path());
    association->register_property("Associations", associations);
    association->initialize();
}

EStoraged::~EStoraged()
{
    objectServer.remove_interface(volumeInterface);
    objectServer.remove_interface(driveInterface);
    objectServer.remove_interface(embeddedLocationInterface);
    objectServer.remove_interface(assetInterface);
    objectServer.remove_interface(association);

    if (locationCodeInterface != nullptr)
    {
        objectServer.remove_interface(locationCodeInterface);
    }
}

void EStoraged::formatLuks(const std::vector<uint8_t>& password,
                           Volume::FilesystemType type)
{
    std::string msg = "OpenBMC.0.1.DriveFormat";
    lg2::info("Starting format", "REDFISH_MESSAGE_ID", msg);

    if (type != Volume::FilesystemType::ext4)
    {
        lg2::error("Only ext4 filesystems are supported currently",
                   "REDFISH_MESSAGE_ID", std::string("OpenBMC.0.1.FormatFail"));
        throw UnsupportedRequest();
    }

    formatLuksDev(password);
    activateLuksDev(password);

    createFilesystem();
    mountFilesystem();
}

void EStoraged::erase(Volume::EraseMethod inEraseMethod)
{
    std::cerr << "Erasing encrypted eMMC" << std::endl;
    lg2::info("Starting erase", "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DriveErase"));
    switch (inEraseMethod)
    {
        case Volume::EraseMethod::CryptoErase:
        {
            CryptErase myCryptErase(devPath);
            myCryptErase.doErase();
            break;
        }
        case Volume::EraseMethod::VerifyGeometry:
        {
            VerifyDriveGeometry myVerifyGeometry(devPath);
            myVerifyGeometry.geometryOkay(eraseMaxGeometry, eraseMinGeometry);
            break;
        }
        case Volume::EraseMethod::LogicalOverWrite:
        {
            Pattern myErasePattern(devPath);
            myErasePattern.writePattern();
            break;
        }
        case Volume::EraseMethod::LogicalVerify:
        {
            Pattern myErasePattern(devPath);
            myErasePattern.verifyPattern();
            break;
        }
        case Volume::EraseMethod::VendorSanitize:
        {
            Sanitize mySanitize(devPath);
            mySanitize.doSanitize();
            break;
        }
        case Volume::EraseMethod::ZeroOverWrite:
        {
            Zero myZero(devPath);
            myZero.writeZero();
            break;
        }
        case Volume::EraseMethod::ZeroVerify:
        {
            Zero myZero(devPath);
            myZero.verifyZero();
            break;
        }
        case Volume::EraseMethod::SecuredLocked:
        {
            if (!isLocked())
            {
                lock();
            }
            // TODO: implement hardware locking
            // Until that is done, we can lock using eStoraged::lock()
            break;
        }
    }
}

void EStoraged::lock()
{
    std::string msg = "OpenBMC.0.1.DriveLock";
    lg2::info("Starting lock", "REDFISH_MESSAGE_ID", msg);

    unmountFilesystem();
    deactivateLuksDev();
}

void EStoraged::unlock(std::vector<uint8_t> password)
{
    std::string msg = "OpenBMC.0.1.DriveUnlock";
    lg2::info("Starting unlock", "REDFISH_MESSAGE_ID", msg);

    activateLuksDev(std::move(password));
    mountFilesystem();
}

void EStoraged::changePassword(const std::vector<uint8_t>& oldPassword,
                               const std::vector<uint8_t>& newPassword)
{
    lg2::info("Starting change password", "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DrivePasswordChanged"));

    CryptHandle cryptHandle = loadLuksHeader();

    int retval = cryptIface->cryptKeyslotChangeByPassphrase(
        cryptHandle.get(), CRYPT_ANY_SLOT, CRYPT_ANY_SLOT,
        reinterpret_cast<const char*>(oldPassword.data()), oldPassword.size(),
        reinterpret_cast<const char*>(newPassword.data()), newPassword.size());
    if (retval < 0)
    {
        lg2::error("Failed to change password", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.DrivePasswordChangeFail"));
        throw InternalFailure();
    }

    lg2::info("Successfully changed password for {DEV}", "DEV", devPath,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DrivePasswordChangeSuccess"));
}

bool EStoraged::isLocked() const
{
    /*
     * Check if the mapped virtual device exists. If it exists, the LUKS volume
     * is unlocked.
     */
    try
    {
        std::filesystem::path mappedDevicePath(cryptDevicePath);
        return (std::filesystem::exists(mappedDevicePath) == false);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to query locked status: {EXCEPT}", "EXCEPT",
                   e.what(), "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.IsLockedFail"));
        /* If we couldn't query the filesystem path, assume unlocked. */
        return false;
    }
}

std::string_view EStoraged::getMountPoint() const
{
    return mountPoint;
}

void EStoraged::formatLuksDev(std::vector<uint8_t> password)
{
    lg2::info("Formatting device {DEV}", "DEV", devPath, "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.FormatLuksDev"));

    /* Generate the volume key. */
    const std::size_t keySize = 64;
    std::vector<uint8_t> volumeKey(keySize);
    if (RAND_bytes(volumeKey.data(), keySize) != 1)
    {
        lg2::error("Failed to create volume key", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FormatLuksDevFail"));
        throw InternalFailure();
    }

    /* Create the handle. */
    CryptHandle cryptHandle(devPath);

    /* Format the LUKS encrypted device. */
    int retval = cryptIface->cryptFormat(
        cryptHandle.get(), CRYPT_LUKS2, "aes", "xts-plain64", nullptr,
        reinterpret_cast<const char*>(volumeKey.data()), volumeKey.size(),
        nullptr);
    if (retval < 0)
    {
        lg2::error("Failed to format encrypted device: {RETVAL}", "RETVAL",
                   retval, "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FormatLuksDevFail"));
        throw InternalFailure();
    }

    /* Set the password. */
    retval = cryptIface->cryptKeyslotAddByVolumeKey(
        cryptHandle.get(), CRYPT_ANY_SLOT, nullptr, 0,
        reinterpret_cast<const char*>(password.data()), password.size());

    if (retval < 0)
    {
        lg2::error("Failed to set encryption password", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FormatLuksDevFail"));
        throw InternalFailure();
    }

    lg2::info("Encrypted device {DEV} successfully formatted", "DEV", devPath,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.FormatLuksDevSuccess"));
}

CryptHandle EStoraged::loadLuksHeader()
{
    CryptHandle cryptHandle(devPath);

    int retval = cryptIface->cryptLoad(cryptHandle.get(), CRYPT_LUKS2, nullptr);
    if (retval < 0)
    {
        lg2::error("Failed to load LUKS header: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.ActivateLuksDevFail"));
        throw InternalFailure();
    }
    return cryptHandle;
}

Drive::DriveEncryptionState EStoraged::findEncryptionStatus()
{
    try
    {
        loadLuksHeader();
        return Drive::DriveEncryptionState::Encrypted;
    }
    catch (...)
    {
        return Drive::DriveEncryptionState::Unencrypted;
    }
}

void EStoraged::activateLuksDev(std::vector<uint8_t> password)
{
    lg2::info("Activating LUKS dev {DEV}", "DEV", devPath, "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.ActivateLuksDev"));

    /* Create the handle. */
    CryptHandle cryptHandle = loadLuksHeader();

    int retval = cryptIface->cryptActivateByPassphrase(
        cryptHandle.get(), containerName.c_str(), CRYPT_ANY_SLOT,
        reinterpret_cast<const char*>(password.data()), password.size(), 0);

    if (retval < 0)
    {
        lg2::error("Failed to activate LUKS dev: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.ActivateLuksDevFail"));
        throw InternalFailure();
    }

    lg2::info("Successfully activated LUKS dev {DEV}", "DEV", devPath,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.ActivateLuksDevSuccess"));
}

void EStoraged::createFilesystem()
{
    /* Run the command to create the filesystem. */
    int retval = fsIface->runMkfs(cryptDevicePath);
    if (retval != 0)
    {
        lg2::error("Failed to create filesystem: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.CreateFilesystemFail"));
        throw InternalFailure();
    }
    lg2::info("Successfully created filesystem for {CONTAINER}", "CONTAINER",
              cryptDevicePath, "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.CreateFilesystemSuccess"));
}

void EStoraged::mountFilesystem()
{
    /*
     * Create directory for the filesystem, if it's not already present. It
     * might already exist if, for example, the BMC reboots after creating the
     * directory.
     */
    if (!fsIface->directoryExists(std::filesystem::path(mountPoint)))
    {
        bool success =
            fsIface->createDirectory(std::filesystem::path(mountPoint));
        if (!success)
        {
            lg2::error("Failed to create mount point: {DIR}", "DIR", mountPoint,
                       "REDFISH_MESSAGE_ID",
                       std::string("OpenBMC.0.1.MountFilesystemFail"));
            throw InternalFailure();
        }
    }

    /* Run the command to mount the filesystem. */
    int retval = fsIface->doMount(cryptDevicePath.c_str(), mountPoint.c_str(),
                                  "ext4", 0, nullptr);
    if (retval != 0)
    {
        lg2::error("Failed to mount filesystem: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.MountFilesystemFail"));
        bool removeSuccess =
            fsIface->removeDirectory(std::filesystem::path(mountPoint));
        if (!removeSuccess)
        {
            lg2::error("Failed to remove mount point: {DIR}", "DIR", mountPoint,
                       "REDFISH_MESSAGE_ID",
                       std::string("OpenBMC.0.1.MountFilesystemFail"));
        }
        throw InternalFailure();
    }

    lg2::info("Successfully mounted filesystem at {DIR}", "DIR", mountPoint,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.MountFilesystemSuccess"));
}

void EStoraged::unmountFilesystem()
{
    int retval = fsIface->doUnmount(mountPoint.c_str());
    if (retval != 0)
    {
        lg2::error("Failed to unmount filesystem: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.UnmountFilesystemFail"));
        throw InternalFailure();
    }

    /* Remove the mount point. */
    bool success = fsIface->removeDirectory(std::filesystem::path(mountPoint));
    if (!success)
    {
        lg2::error("Failed to remove mount point {DIR}", "DIR", mountPoint,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.UnmountFilesystemFail"));
        throw InternalFailure();
    }

    lg2::info("Successfully unmounted filesystem at {DIR}", "DIR", mountPoint,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.MountFilesystemSuccess"));
}

void EStoraged::deactivateLuksDev()
{
    lg2::info("Deactivating LUKS device {DEV}", "DEV", devPath,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DeactivateLuksDev"));

    int retval = cryptIface->cryptDeactivate(nullptr, containerName.c_str());
    if (retval < 0)
    {
        lg2::error("Failed to deactivate crypt device: {RETVAL}", "RETVAL",
                   retval, "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.DeactivateLuksDevFail"));
        throw InternalFailure();
    }

    lg2::info("Successfully deactivated LUKS device {DEV}", "DEV", devPath,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DeactivateLuksDevSuccess"));
}

std::string_view EStoraged::getCryptDevicePath() const
{
    return cryptDevicePath;
}

} // namespace estoraged
