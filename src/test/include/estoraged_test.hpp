
#include "cryptErase.hpp"
#include "cryptsetupInterface.hpp"
#include "estoraged.hpp"
#include "filesystemInterface.hpp"

#include <unistd.h>

#include <exception>
#include <filesystem>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace estoraged_test
{

class MockFilesystemInterface : public estoraged::FilesystemInterface
{
  public:
    MOCK_METHOD(int, runMkfs, (const std::string& logicalVolume), (override));

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
};

class MockCryptsetupInterface : public estoraged::CryptsetupInterface
{
  public:
    MOCK_METHOD(int, cryptFormat,
                (struct crypt_device * cd, const char* type, const char* cipher,
                 const char* cipher_mode, const char* uuid,
                 const char* volume_key, size_t volume_key_size, void* params),
                (override));

    MOCK_METHOD(int, cryptKeyslotAddByVolumeKey,
                (struct crypt_device * cd, int keyslot, const char* volume_key,
                 size_t volume_key_size, const char* passphrase,
                 size_t passphrase_size),
                (override));

    MOCK_METHOD(int, cryptLoad,
                (struct crypt_device * cd, const char* requested_type,
                 void* params),
                (override));

    MOCK_METHOD(int, cryptKeyslotChangeByPassphrase,
                (struct crypt_device * cd, int keyslotOld, int keyslotNew,
                 const char* passphrase, size_t passphraseSize,
                 const char* newPassphrase, size_t newPassphraseSize),
                (override));

    MOCK_METHOD(int, cryptActivateByPassphrase,
                (struct crypt_device * cd, const char* name, int keyslot,
                 const char* passphrase, size_t passphrase_size,
                 uint32_t flags),
                (override));

    MOCK_METHOD(int, cryptDeactivate,
                (struct crypt_device * cd, const char* name), (override));

    MOCK_METHOD(int, cryptKeyslotDestroy,
                (struct crypt_device * cd, const int slot), (override));

    MOCK_METHOD(int, cryptKeySlotMax, (const char* type), (override));

    MOCK_METHOD(crypt_keyslot_info, cryptKeySlotStatus,
                (struct crypt_device * cd, int keyslot), (override));
};

} // namespace estoraged_test
