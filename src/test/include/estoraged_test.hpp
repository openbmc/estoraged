
#include "cryptErase.hpp"
#include "cryptsetupInterface.hpp"
#include "estoraged.hpp"
#include "filesystemInterface.hpp"

#include <unistd.h>

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdbusplus/test/sdbus_mock.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Inventory/Item/Volume/client.hpp>
#include <xyz/openbmc_project/Inventory/Item/Volume/server.hpp>

#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

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

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;
using sdbusplus::xyz::openbmc_project::Inventory::Item::server::Volume;
using std::filesystem::path;
using ::testing::_;
using ::testing::ContainsRegex;
using ::testing::Ge;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::StrEq;

/*
 * This sdbus mock object gets used in the destructor of one of the parent
 * classes for the MockeStoraged object, so this can't be part of the
 * eStoragedTest class.
 */
sdbusplus::SdBusMock sdbusMock;

class eStoragedTest : public testing::Test
{
  public:
    static constexpr char testFileName[] = "testfile";
    static constexpr char testLuksDevName[] = "testfile_luksDev";
    std::ofstream testFile;
    std::unique_ptr<estoraged::eStoraged> esObject;
    static constexpr auto TEST_PATH = "/test/openbmc_project/storage/test_dev";
    static constexpr auto ESTORAGED_INTERFACE =
        "xyz.openbmc_project.Inventory.Item.Volume";
    sdbusplus::bus::bus bus;
    std::string passwordString;
    std::vector<uint8_t> password;
    MockCryptsetupInterface* mockCryptIface;
    MockFilesystemInterface* mockFsIface;

    eStoragedTest() :
        bus(sdbusplus::get_mocked_new(&sdbusMock)), passwordString("password"),
        password(passwordString.begin(), passwordString.end())
    {}

    void SetUp() override
    {
        /* Create an empty file that we'll pretend is a 'storage device'. */
        testFile.open(testFileName,
                      std::ios::out | std::ios::binary | std::ios::trunc);
        testFile.close();
        if (testFile.fail())
        {
            throw std::runtime_error("Failed to open test file");
        }

        EXPECT_CALL(sdbusMock,
                    sd_bus_add_object_vtable(IsNull(), _, StrEq(TEST_PATH),
                                             StrEq(ESTORAGED_INTERFACE), _, _))
            .WillRepeatedly(Return(0));

        EXPECT_CALL(sdbusMock,
                    sd_bus_emit_object_added(IsNull(), StrEq(TEST_PATH)))
            .WillRepeatedly(Return(0));

        EXPECT_CALL(sdbusMock,
                    sd_bus_emit_object_removed(IsNull(), StrEq(TEST_PATH)))
            .WillRepeatedly(Return(0));

        std::unique_ptr<MockCryptsetupInterface> cryptIface =
            std::make_unique<MockCryptsetupInterface>();
        mockCryptIface = cryptIface.get();
        std::unique_ptr<MockFilesystemInterface> fsIface =
            std::make_unique<MockFilesystemInterface>();
        mockFsIface = fsIface.get();

        esObject = std::make_unique<estoraged::eStoraged>(
            bus, TEST_PATH, std::string(testFileName),
            std::string(testLuksDevName), std::move(cryptIface),
            std::move(fsIface));
    }

    void TearDown() override
    {
        EXPECT_EQ(0, unlink(testFileName));
    }
};

} // namespace estoraged_test
