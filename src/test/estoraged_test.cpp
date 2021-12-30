
#include "cryptsetupInterface.hpp"
#include "estoraged.hpp"
#include "filesystemInterface.hpp"

#include <unistd.h>

#include <sdbusplus/bus.hpp>
#include <sdbusplus/test/sdbus_mock.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

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
};

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;
using sdbusplus::xyz::openbmc_project::Inventory::Item::server::Volume;
using std::filesystem::path;
using ::testing::_;
using ::testing::ContainsRegex;
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

/* Test case to format and then lock the LUKS device. */
TEST_F(eStoragedTest, FormatPass)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockFsIface,
                doMount(ContainsRegex("/dev/mapper/"),
                        StrEq(esObject->getMountPoint()), _, _, _))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, doUnmount(StrEq(esObject->getMountPoint())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, removeDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _)).Times(1);

    /* Format the encrypted device. */
    esObject->formatLuks(password, Volume::FilesystemType::ext4);
    EXPECT_FALSE(esObject->isLocked());

    esObject->lock();
    EXPECT_TRUE(esObject->isLocked());
}

/* Test case where the device/file doesn't exist. */
TEST_F(eStoragedTest, FormatNoDeviceFail)
{
    /* Delete the test file. */
    EXPECT_EQ(0, unlink(testFileName));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 ResourceNotFound);
    EXPECT_FALSE(esObject->isLocked());

    /* Create the test file again, so that the TearDown function works. */
    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();
}

/* Test case where we fail to format the LUKS device. */
TEST_F(eStoragedTest, FormatFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _))
        .WillOnce(Return(-1));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_FALSE(esObject->isLocked());
}

/* Test case where we fail to set the password for the LUKS device. */
TEST_F(eStoragedTest, AddKeyslotFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .WillOnce(Return(-1));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_TRUE(esObject->isLocked());
}

/* Test case where we fail to load the LUKS header. */
TEST_F(eStoragedTest, LoadLuksHeaderFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).WillOnce(Return(-1));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_TRUE(esObject->isLocked());
}

/* Test case where we fail to activate the LUKS device. */
TEST_F(eStoragedTest, ActivateFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(Return(-1));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_TRUE(esObject->isLocked());
}

/* Test case where we fail to create the filesystem. */
TEST_F(eStoragedTest, CreateFilesystemFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(-1));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_FALSE(esObject->isLocked());
}

/* Test case where we fail to create the mount point. */
TEST_F(eStoragedTest, CreateMountPointFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_FALSE(esObject->isLocked());
}

/* Test case where we fail to mount the filesystem. */
TEST_F(eStoragedTest, MountFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockFsIface,
                doMount(ContainsRegex("/dev/mapper/"),
                        StrEq(esObject->getMountPoint()), _, _, _))
        .WillOnce(Return(-1));

    EXPECT_CALL(*mockFsIface, removeDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_FALSE(esObject->isLocked());
}

/* Test case where we fail to unmount the filesystem. */
TEST_F(eStoragedTest, UnmountFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockFsIface,
                doMount(ContainsRegex("/dev/mapper/"),
                        StrEq(esObject->getMountPoint()), _, _, _))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, doUnmount(StrEq(esObject->getMountPoint())))
        .WillOnce(Return(-1));

    esObject->formatLuks(password, Volume::FilesystemType::ext4);
    EXPECT_FALSE(esObject->isLocked());

    EXPECT_THROW(esObject->lock(), InternalFailure);
    EXPECT_FALSE(esObject->isLocked());
}

/* Test case where we fail to remove the mount point. */
TEST_F(eStoragedTest, RemoveMountPointFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockFsIface,
                doMount(ContainsRegex("/dev/mapper/"),
                        StrEq(esObject->getMountPoint()), _, _, _))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, doUnmount(StrEq(esObject->getMountPoint())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, removeDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    esObject->formatLuks(password, Volume::FilesystemType::ext4);
    EXPECT_FALSE(esObject->isLocked());

    /* This will fail to remove the mount point. */
    EXPECT_THROW(esObject->lock(), InternalFailure);
    EXPECT_FALSE(esObject->isLocked());
}

/* Test case where we fail to deactivate the LUKS device. */
TEST_F(eStoragedTest, DeactivateFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockFsIface,
                doMount(ContainsRegex("/dev/mapper/"),
                        StrEq(esObject->getMountPoint()), _, _, _))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, doUnmount(StrEq(esObject->getMountPoint())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, removeDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _)).WillOnce(Return(-1));

    /* Format the encrypted device. */
    esObject->formatLuks(password, Volume::FilesystemType::ext4);
    EXPECT_FALSE(esObject->isLocked());

    EXPECT_THROW(esObject->lock(), InternalFailure);
    EXPECT_FALSE(esObject->isLocked());
}

} // namespace estoraged_test
