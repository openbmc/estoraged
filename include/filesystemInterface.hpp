#pragma once

#include <sys/mount.h>

#include <filesystem>
#include <string>

namespace estoraged
{

/** @class FilesystemInterface
 *  @brief Interface to the filesystem operations that eStoraged needs.
 *  @details This class is used to mock out the filesystem operations.
 */
class FilesystemInterface
{
  public:
    virtual ~FilesystemInterface() = default;

    /** @brief Runs the mkfs command to create the filesystem.
     *  @details Used for mocking purposes.
     *
     *  @param[in] logicalVolume - name of the mapped LUKS device.
     *
     *  @returns 0 on success, nonzero on failure.
     */
    virtual int runMkfs(const std::string& logicalVolume)
    {
        std::string mkfsCommand("mkfs.ext4 /dev/mapper/" + logicalVolume);
        return system(mkfsCommand.c_str());
    }

    /** @brief Wrapper around mount().
     *  @details Used for mocking purposes.
     *
     *  @param[in] source - device where the filesystem is located.
     *  @param[in] target - path to where the filesystem should be mounted.
     *  @param[in] filesystemType - (e.g. "ext4").
     *  @param[in] mountflags - flags bit mask (see mount() documentation).
     *  @param[in] data - options for specific filesystem type, can be NULL
     *    (see mount() documentation).
     *
     *  @returns On success, zero is returned.  On error, -1 is returned, and
     *    errno is set to indicate the error.
     */
    virtual int doMount(const char* source, const char* target,
                        const char* filesystemtype, unsigned long mountflags,
                        const void* data)
    {
        return mount(source, target, filesystemtype, mountflags, data);
    }

    /** @brief Wrapper around umount().
     *  @details Used for mocking purposes.
     *
     *  @param[in] target - path location where the filesystem is mounted.
     *
     *  @returns On success, zero is returned.  On error, -1 is returned, and
     *    errno is set to indicate the error.
     */
    virtual int doUnmount(const char* target)
    {
        return umount(target);
    }

    /** @brief Wrapper around std::filesystem::create_directory.
     *  @details Used for mocking purposes.
     *
     *  @param[in] p - path to directory that should be created.
     *
     *  @returns true on success, false otherwise.
     */
    virtual bool createDirectory(const std::filesystem::path& p)
    {
        return std::filesystem::create_directory(p);
    }

    /** @brief Wrapper around std::filesystem::remove.
     *  @details Used for mocking purposes.
     *
     *  @param[in] p - path to directory that should be removed.
     *
     *  @returns true on success, false otherwise.
     */
    virtual bool removeDirectory(const std::filesystem::path& p)
    {
        return std::filesystem::remove(p);
    }
};

} // namespace estoraged
