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
    virtual int runMkfs(const std::string& logicalVolume) = 0;

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
                        const void* data) = 0;

    /** @brief Wrapper around umount().
     *  @details Used for mocking purposes.
     *
     *  @param[in] target - path location where the filesystem is mounted.
     *
     *  @returns On success, zero is returned.  On error, -1 is returned, and
     *    errno is set to indicate the error.
     */
    virtual int doUnmount(const char* target) = 0;

    /** @brief Wrapper around std::filesystem::create_directory.
     *  @details Used for mocking purposes.
     *
     *  @param[in] p - path to directory that should be created.
     *
     *  @returns true on success, false otherwise.
     */
    virtual bool createDirectory(const std::filesystem::path& p) = 0;

    /** @brief Wrapper around std::filesystem::remove.
     *  @details Used for mocking purposes.
     *
     *  @param[in] p - path to directory that should be removed.
     *
     *  @returns true on success, false otherwise.
     */
    virtual bool removeDirectory(const std::filesystem::path& p) = 0;

    /** @brief Wrapper around std::filesystem::is_directory
     *
     *  @param[in] p - path to directory that we want to query.
     *
     *  @returns true if the path exists and represents a directory.
     */
    virtual bool directoryExists(const std::filesystem::path& p) = 0;
};

/** @class Filesystem
 *  @brief Implements FilesystemInterface
 */
class Filesystem : public FilesystemInterface
{
  public:
    ~Filesystem() override = default;

    int runMkfs(const std::string& logicalVolume) override
    {
        std::string mkfsCommand("mkfs.ext4 /dev/mapper/" + logicalVolume);
        return system(mkfsCommand.c_str());
    }

    int doMount(const char* source, const char* target,
                const char* filesystemtype, unsigned long mountflags,
                const void* data) override
    {
        return mount(source, target, filesystemtype, mountflags, data);
    }

    int doUnmount(const char* target) override
    {
        return umount(target);
    }

    bool createDirectory(const std::filesystem::path& p) override
    {
        return std::filesystem::create_directory(p);
    }

    bool removeDirectory(const std::filesystem::path& p) override
    {
        return std::filesystem::remove(p);
    }

    bool directoryExists(const std::filesystem::path& p) override
    {
        return std::filesystem::is_directory(std::filesystem::status(p));
    }
};
} // namespace estoraged
