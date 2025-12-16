#pragma once

#include <sys/mount.h>

#include <filesystem>
#include <initializer_list>
#include <numeric>
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

    FilesystemInterface() = default;
    FilesystemInterface(const FilesystemInterface&) = delete;
    FilesystemInterface& operator=(const FilesystemInterface&) = delete;

    FilesystemInterface(FilesystemInterface&&) = delete;
    FilesystemInterface& operator=(FilesystemInterface&&) = delete;

    /** @brief Runs the mkfs command to create the filesystem.
     *  @details Used for mocking purposes.
     *
     *  @param[in] logicalVolumePath - path to the mapped LUKS device.
     *
     *  @returns 0 on success, nonzero on failure.
     */
    virtual int runMkfs(const std::string& logicalVolumePath,
                        std::initializer_list<std::string> options = {}) = 0;

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

    /** @brief Wrapper around fsck.
     *  @details Used for mocking purposes.
     *
     *  @param[in] logicalVolumePath - path to device with filesystem
     *  @param[in] options - Other options to pass into fsck
     *
     *  @returns 0 on success, nonzero on failure.
     */
    virtual int runFsck(const std::string& logicalVolumePath,
                        const std::string& options) = 0;
};

/** @class Filesystem
 *  @brief Implements FilesystemInterface
 */
class Filesystem : public FilesystemInterface
{
  public:
    ~Filesystem() override = default;
    Filesystem() = default;
    Filesystem(const Filesystem&) = delete;
    Filesystem& operator=(const Filesystem&) = delete;

    Filesystem(Filesystem&&) = delete;
    Filesystem& operator=(Filesystem&&) = delete;

    int runMkfs(const std::string& logicalVolumePath,
                std::initializer_list<std::string> options) override
    {
        std::string mkfsCommand("mkfs.ext4 ");
        mkfsCommand.reserve(
            mkfsCommand.size() + logicalVolumePath.size() + options.size() +
            std::accumulate(options.begin(), options.end(), 0,
                            [](size_t sum, const std::string& s) {
                                return sum + s.size();
                            }));

        for (const std::string& s : options)
        {
            mkfsCommand.append(s);
            mkfsCommand.push_back(' ');
        }

        mkfsCommand.append(logicalVolumePath);

        // calling 'system' uses a command processor //NOLINTNEXTLINE
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

    int runFsck(const std::string& logicalVolumePath,
                const std::string& options) override
    {
        std::string fsckCommand("fsck " + logicalVolumePath + " " + options);
        // calling 'system' uses a command processor //NOLINTNEXTLINE
        return system(fsckCommand.c_str());
    }
};
} // namespace estoraged
