#pragma once

#include "cryptErase.hpp"
#include "cryptsetupInterface.hpp"
#include "estoraged.hpp"
#include "filesystemInterface.hpp"

#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <system_error>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace estoraged_test
{

/*
 * Temporary scratch directory created with mkdtemp() so that tests do not
 * leave stray files in the working directory or interfere with concurrent test
 * runs. The directory and its contents are removed on destruction.
 */
class TestTmpDir
{
  public:
    TestTmpDir()
    {
        std::filesystem::path tmplPath =
            std::filesystem::temp_directory_path() / "estoraged_test.XXXXXX";
        std::string tmpl = tmplPath.string();
        if (mkdtemp(tmpl.data()) == nullptr)
        {
            throw std::system_error(errno, std::generic_category(),
                                    "mkdtemp(" + tmpl + ")");
        }
        dir = tmpl;
    }

    ~TestTmpDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    TestTmpDir(const TestTmpDir&) = delete;
    TestTmpDir& operator=(const TestTmpDir&) = delete;
    TestTmpDir(TestTmpDir&&) = delete;
    TestTmpDir& operator=(TestTmpDir&&) = delete;

    const std::filesystem::path& get() const
    {
        return dir;
    }

  private:
    std::filesystem::path dir;
};

class MockFilesystemInterface : public estoraged::FilesystemInterface
{
  public:
    MOCK_METHOD(int, runMkfs, (std::initializer_list<std::string_view> args),
                (override));

    MOCK_METHOD(int, doMount,
                (const char* source, const char* target,
                 const char* filesystemtype, unsigned long mountflags,
                 const void* data),
                (override));

    MOCK_METHOD(int, doUnmount, (const char* target), (override));

    MOCK_METHOD(bool, createDirectory, (const std::filesystem::path& p),
                (override));

    MOCK_METHOD(bool, removeDirectory, (const std::filesystem::path& p),
                (override));

    MOCK_METHOD(bool, directoryExists, (const std::filesystem::path& p),
                (override));

    MOCK_METHOD(int, runFsck, (std::initializer_list<std::string_view> args),
                (override));
};

class MockCryptsetupInterface : public estoraged::CryptsetupInterface
{
  public:
    MOCK_METHOD(int, cryptFormat,
                (struct crypt_device * cd, const char* type, const char* cipher,
                 const char* cipherMode, const char* uuid,
                 const char* volumeKey, size_t volumeKeySize, void* params),
                (override));

    MOCK_METHOD(int, cryptKeyslotAddByVolumeKey,
                (struct crypt_device * cd, int keyslot, const char* volumeKey,
                 size_t volumeKeySize, const char* passphrase,
                 size_t passphraseSize),
                (override));

    MOCK_METHOD(int, cryptLoad,
                (struct crypt_device * cd, const char* requestedType,
                 void* params),
                (override));

    MOCK_METHOD(int, cryptKeyslotChangeByPassphrase,
                (struct crypt_device * cd, int keyslotOld, int keyslotNew,
                 const char* passphrase, size_t passphraseSize,
                 const char* newPassphrase, size_t newPassphraseSize),
                (override));

    MOCK_METHOD(int, cryptActivateByPassphrase,
                (struct crypt_device * cd, const char* name, int keyslot,
                 const char* passphrase, size_t passphraseSize, uint32_t flags),
                (override));

    MOCK_METHOD(int, cryptDeactivate,
                (struct crypt_device * cd, const char* name), (override));

    MOCK_METHOD(int, cryptKeyslotDestroy,
                (struct crypt_device * cd, const int slot), (override));

    MOCK_METHOD(int, cryptKeySlotMax, (const char* type), (override));

    MOCK_METHOD(crypt_keyslot_info, cryptKeySlotStatus,
                (struct crypt_device * cd, int keyslot), (override));

    MOCK_METHOD(std::string, cryptGetDir, (), (override));
};

} // namespace estoraged_test
