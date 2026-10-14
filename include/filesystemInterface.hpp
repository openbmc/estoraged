#pragma once

#include "runCommand.hpp"

#include <sys/mount.h>
#include <unistd.h>

#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

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
     *  @param[in] args - Arguments to pass into mkfs.
     *
     *  @returns 0 on success, nonzero on failure.
     */
    virtual int runMkfs(std::initializer_list<std::string_view> args) = 0;

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

    /** @brief Wrapper around rmdir().
     *  @details Used for mocking purposes.
     *
     *  @param[in] p - path to directory that should be removed.
     *
     *  @returns On success, zero is returned. On error, -1 is returned, and
     *    errno is set to indicate the error.
     */
    virtual int removeDirectory(const std::filesystem::path& p) = 0;

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
     *  @param[in] args - Arguments to pass into fsck.
     *
     *  @returns
     *    - -1 if no child process could be created.
     *    - 127 if fsck could not be started.
     *    - 128 + signal if fsck has been interrupted by a signal.
     *    - If fsck has been started, the sum of:
     *      - 0    - No errors
     *      - 1    - File system errors corrected
     *      - 2    - File system errors corrected, system should be rebooted
     *      - 4    - File system errors left uncorrected
     *      - 8    - Operational error
     *      - 16   - Usage or syntax error
     *      - 32   - Checking canceled by user request
     */
    virtual int runFsck(std::initializer_list<std::string_view> args) = 0;
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

    int runMkfs(std::initializer_list<std::string_view> args) override
    {
        std::vector<std::string_view> argv;
        argv.reserve(args.size() + 1);
        argv.emplace_back("mkfs.ext4");
        argv.insert(argv.end(), args.begin(), args.end());

        return runCommand(argv);
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
        std::error_code ec;
        return std::filesystem::create_directory(p, ec) && !ec;
    }

    int removeDirectory(const std::filesystem::path& p) override
    {
        return rmdir(p.c_str());
    }

    bool directoryExists(const std::filesystem::path& p) override
    {
        std::error_code ec;
        return std::filesystem::is_directory(p, ec) && !ec;
    }

    int runFsck(std::initializer_list<std::string_view> args) override
    {
        std::vector<std::string_view> argv;
        argv.reserve(args.size() + 1);
        argv.emplace_back("fsck");
        argv.insert(argv.end(), args.begin(), args.end());

        return runCommand(argv);
    }
};
} // namespace estoraged
