
#include "estoraged.hpp"

#include "cryptsetupInterface.hpp"
#include "pattern.hpp"
#include "sanitize.hpp"
#include "verifyDriveGeometry.hpp"
#include "zero.hpp"

#include <libcryptsetup.h>
#include <openssl/rand.h>
#include <stdlib.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

namespace estoraged
{

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;
using sdbusplus::xyz::openbmc_project::Common::Error::UnsupportedRequest;

void eStoraged::formatLuks(std::vector<uint8_t> password, FilesystemType type)
{
    std::string msg = "OpenBMC.0.1.DriveFormat";
    lg2::info("Starting format", "REDFISH_MESSAGE_ID", msg);

    if (type != FilesystemType::ext4)
    {
        lg2::error("Only ext4 filesystems are supported currently",
                   "REDFISH_MESSAGE_ID", std::string("OpenBMC.0.1.FormatFail"));
        throw UnsupportedRequest();
    }

    CryptHandle cryptHandle(devPath.c_str());
    if (cryptHandle.get() == nullptr)
    {
        lg2::error("Failed to initialize crypt device", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FormatFail"));
        throw ResourceNotFound();
    }

    formatLuksDev(cryptHandle.get(), password);
    activateLuksDev(cryptHandle.get(), password);

    createFilesystem();
    mountFilesystem();
}

void eStoraged::erase(EraseMethod inEraseMethod)
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
            Pattern myErasePattern(devPath);
            ManagedFd drivefd =
                stdplus::fd::open(devPath, stdplus::fd::OpenAccess::WriteOnly);
            myErasePattern.writePattern(myErasePattern.findSizeOfBlockDevice(),
                                        drivefd);
            break;
        }
        case EraseMethod::LogicalVerify:
        {
            Pattern myErasePattern(devPath);
            ManagedFd drivefd =
                stdplus::fd::open(devPath, stdplus::fd::OpenAccess::ReadOnly);
            myErasePattern.verifyPattern(myErasePattern.findSizeOfBlockDevice(),
                                         drivefd);
            break;
        }
        case EraseMethod::VendorSanitize:
        {
            Sanitize mySanitize(devPath);
            uint64_t size = mySanitize.findSizeOfBlockDevice();
            std::byte _ExtCsd[512] = {};
            std::span<std::byte> extCsd(_ExtCsd);
            mySanitize.readExtCsd(extCsd);
            mySanitize.doSanitize(size, extCsd);
            break;
        }
        case EraseMethod::ZeroOverWrite:
        {
            Zero myZero(devPath);
            ManagedFd drivefd =
                stdplus::fd::open(devPath, stdplus::fd::OpenAccess::WriteOnly);
            myZero.writeZero(myZero.findSizeOfBlockDevice(), drivefd);
            break;
        }
        case EraseMethod::ZeroVerify:
        {
            Zero myZero(devPath);
            ManagedFd drivefd =
                stdplus::fd::open(devPath, stdplus::fd::OpenAccess::ReadOnly);
            myZero.verifyZero(myZero.findSizeOfBlockDevice(), drivefd);
            break;
        }
        case EraseMethod::SecuredLocked:
        {
            break;
        }
    }
}

void eStoraged::lock()
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

    CryptHandle cryptHandle(devPath.c_str());
    if (cryptHandle.get() == nullptr)
    {
        lg2::error("Failed to initialize crypt device", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.UnlockFail"));
        throw ResourceNotFound();
    }

    activateLuksDev(cryptHandle.get(), password);
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
        throw InternalFailure();
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
        throw InternalFailure();
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
        throw InternalFailure();
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
        throw InternalFailure();
    }

    retval = cryptIface->cryptActivateByPassphrase(
        cd, containerName.c_str(), CRYPT_ANY_SLOT,
        reinterpret_cast<const char*>(password.data()), password.size(), 0);

    if (retval < 0)
    {
        lg2::error("Failed to activate LUKS dev: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.ActivateLuksDevFail"));
        throw InternalFailure();
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
        throw InternalFailure();
    }
    lg2::info("Successfully created filesystem for /dev/mapper/{CONTAINER}",
              "CONTAINER", containerName, "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.CreateFilesystemSuccess"));
}

void eStoraged::mountFilesystem()
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
        throw InternalFailure();
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
        throw InternalFailure();
    }

    /* Device is now locked. */
    locked(true);

    lg2::info("Successfully deactivated LUKS device {DEV}", "DEV", devPath,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DeactivateLuksDevSuccess"));
}

} // namespace estoraged
