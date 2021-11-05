
#include "estoraged.hpp"

#include "cryptsetupInterface.hpp"
#include "sanitize.hpp"
#include "verifyDriveGeometry.hpp"

#include <libcryptsetup.h>
#include <openssl/rand.h>
#include <stdlib.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;

namespace estoraged
{

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EncryptionError;
using sdbusplus::xyz::openbmc_project::eStoraged::Error::FilesystemError;

void eStoraged::format(std::vector<uint8_t> password)
{
    std::string msg = "OpenBMC.0.1.DriveFormat";
    lg2::info("Starting format", "REDFISH_MESSAGE_ID", msg);

    struct crypt_device* cryptDev;
    CryptHandle cryptHandle(&cryptDev, devPath.c_str());
    if (*cryptHandle.handle == nullptr)
    {
        lg2::error("Failed to initialize crypt device", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FormatFail"));
        throw EncryptionError();
    }

    formatLuksDev(cryptDev, password);
    activateLuksDev(cryptDev, password);

    createFilesystem();
    mountFilesystem();
}

void eStoraged::erase(std::vector<uint8_t>, EraseMethod inEraseMethod)
{
    std::cerr << "Erasing encrypted eMMC" << std::endl;
    lg2::info("Starting erase", "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DriveErase"));
    switch (inEraseMethod)
    {
        case EraseMethod::CryptoErase:
        {
            break;
        }
        case EraseMethod::VerifyGeometry:
        {
            VerifyDriveGeometry myVerifyGeometry(devPath);
            uint64_t size = myVerifyGeometry.findSizeOfBlockDevice();
            myVerifyGeometry.geometryOkay(size);
            break;
        }
        case EraseMethod::LogicalOverWrite:
        {
            break;
        }
        case EraseMethod::LogicalVerify:
        {
            break;
        }
        case EraseMethod::VendorSanitize:
        {
            SanitizeImpl mySanitize(devPath);
            mySanitize.doSanitize();
            break;
        }
        case EraseMethod::ZeroOverWrite:
        {
            break;
        }
        case EraseMethod::ZeroVerify:
        {
            break;
        }
        case EraseMethod::SecuredLocked:
        {
            break;
        }
    }
}

void eStoraged::lock(std::vector<uint8_t>)
{
    std::string msg = "OpenBMC.0.1.DriveLock";
    lg2::info("Starting lock", "REDFISH_MESSAGE_ID", msg);

    unmountFilesystem();
    deactivateLuksDev();
}

void eStoraged::unlock(std::vector<uint8_t> password)
{
    std::string msg = "OpenBMC.0.1.DriveUnlock";
    lg2::info("Starting unlock", "REDFISH_MESSAGE_ID", msg);

    struct crypt_device* cryptDev;
    CryptHandle cryptHandle(&cryptDev, devPath.c_str());
    if (*cryptHandle.handle == nullptr)
    {
        lg2::error("Failed to initialize crypt device", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.UnlockFail"));
        throw EncryptionError();
    }

    activateLuksDev(cryptDev, password);
    mountFilesystem();
}

void eStoraged::changePassword(std::vector<uint8_t>, std::vector<uint8_t>)
{
    std::cerr << "Changing password for encrypted eMMC" << std::endl;
    lg2::info("Starting change password", "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DrivePasswordChanged"));
}

bool eStoraged::isLocked() const
{
    return locked();
}

std::string_view eStoraged::getMountPoint() const
{
    return mountPoint;
}

void eStoraged::formatLuksDev(struct crypt_device* cd,
                              std::vector<uint8_t> password)
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
        throw EncryptionError();
    }
    /* Format the LUKS encrypted device. */
    int retval =
        cryptIface->cryptFormat(cd, CRYPT_LUKS2, "aes", "xts-plain64", nullptr,
                                reinterpret_cast<const char*>(volumeKey.data()),
                                volumeKey.size(), nullptr);
    if (retval < 0)
    {
        lg2::error("Failed to format encrypted device: {RETVAL}", "RETVAL",
                   retval, "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FormatLuksDevFail"));
        throw EncryptionError();
    }

    /* Device is now encrypted. */
    locked(true);

    /* Set the password. */
    retval = cryptIface->cryptKeyslotAddByVolumeKey(
        cd, CRYPT_ANY_SLOT, nullptr, 0,
        reinterpret_cast<const char*>(password.data()), password.size());

    if (retval < 0)
    {
        lg2::error("Failed to set encryption password", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FormatLuksDevFail"));
        throw EncryptionError();
    }

    lg2::info("Encrypted device {DEV} successfully formatted", "DEV", devPath,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.FormatLuksDevSuccess"));
}

void eStoraged::activateLuksDev(struct crypt_device* cd,
                                std::vector<uint8_t> password)
{
    lg2::info("Activating LUKS dev {DEV}", "DEV", devPath, "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.ActivateLuksDev"));

    int retval = cryptIface->cryptLoad(cd, CRYPT_LUKS2, nullptr);
    if (retval < 0)
    {
        lg2::error("Failed to load LUKS header: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.ActivateLuksDevFail"));
        throw EncryptionError();
    }

    retval = cryptIface->cryptActivateByPassphrase(
        cd, containerName.c_str(), CRYPT_ANY_SLOT,
        reinterpret_cast<const char*>(password.data()), password.size(), 0);

    if (retval < 0)
    {
        lg2::error("Failed to activate LUKS dev: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.ActivateLuksDevFail"));
        throw EncryptionError();
    }

    /* Device is now unlocked. */
    locked(false);

    lg2::info("Successfully activated LUKS dev {DEV}", "DEV", devPath,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.ActivateLuksDevSuccess"));
}

void eStoraged::createFilesystem()
{
    /* Run the command to create the filesystem. */
    int retval = fsIface->runMkfs(containerName);
    if (retval)
    {
        lg2::error("Failed to create filesystem: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.CreateFilesystemFail"));
        throw FilesystemError();
    }
    lg2::info("Successfully created filesystem for /dev/mapper/{CONTAINER}",
              "CONTAINER", containerName, "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.CreateFilesystemSuccess"));
}

void eStoraged::mountFilesystem()
{
    /* Create directory for the filesystem. */
    bool success = fsIface->createDirectory(std::filesystem::path(mountPoint));
    if (!success)
    {
        lg2::error("Failed to create mount point: {DIR}", "DIR", mountPoint,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.MountFilesystemFail"));
        throw FilesystemError();
    }

    /* Run the command to mount the filesystem. */
    std::string luksContainer("/dev/mapper/" + containerName);
    int retval = fsIface->doMount(luksContainer.c_str(), mountPoint.c_str(),
                                  "ext4", 0, nullptr);
    if (retval)
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
        throw FilesystemError();
    }

    lg2::info("Successfully mounted filesystem at {DIR}", "DIR", mountPoint,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.MountFilesystemSuccess"));
}

void eStoraged::unmountFilesystem()
{
    int retval = fsIface->doUnmount(mountPoint.c_str());
    if (retval)
    {
        lg2::error("Failed to unmount filesystem: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.UnmountFilesystemFail"));
        throw FilesystemError();
    }

    /* Remove the mount point. */
    bool success = fsIface->removeDirectory(std::filesystem::path(mountPoint));
    if (!success)
    {
        lg2::error("Failed to remove mount point {DIR}", "DIR", mountPoint,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.UnmountFilesystemFail"));
        throw FilesystemError();
    }

    lg2::info("Successfully unmounted filesystem at {DIR}", "DIR", mountPoint,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.MountFilesystemSuccess"));
}

void eStoraged::deactivateLuksDev()
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
        throw EncryptionError();
    }

    /* Device is now locked. */
    locked(true);

    lg2::info("Successfully deactivated LUKS device {DEV}", "DEV", devPath,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DeactivateLuksDevSuccess"));
}

} // namespace estoraged
