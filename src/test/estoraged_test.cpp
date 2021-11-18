
#include "estoraged.hpp"

#include <unistd.h>

#include <sdbusplus/bus.hpp>
#include <sdbusplus/test/sdbus_mock.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

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

class MockeStoraged : public estoraged::eStoraged
{
  public:
    explicit MockeStoraged(sdbusplus::bus::bus& bus, const char* path,
                           const std::string& devPath,
                           const std::string& containerName) :
        eStoraged(bus, path, devPath, containerName)
    {}

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

    MOCK_METHOD(int, runCommand, (std::string command), (override));

    MOCK_METHOD(int, cryptInitByName,
                (struct crypt_device * *cd, const char* name), (override));

    MOCK_METHOD(int, cryptDeactivate,
                (struct crypt_device * cd, const char* name), (override));
};

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EncryptionError;
using sdbusplus::xyz::openbmc_project::eStoraged::Error::FilesystemError;
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
    std::unique_ptr<MockeStoraged> esObject;
    static constexpr auto TEST_PATH = "/test/openbmc_project/storage/test_dev";
    static constexpr auto ESTORAGED_INTERFACE = "xyz.openbmc_project.eStoraged";
    sdbusplus::bus::bus bus;
    std::string passwordString;
    std::vector<uint8_t> password;

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

        esObject = std::make_unique<MockeStoraged>(
            bus, TEST_PATH, std::string(testFileName),
            std::string(testLuksDevName));
    }

    void TearDown() override
    {
        EXPECT_EQ(0, unlink(testFileName));
        std::cerr << "Test done" << std::endl;
    }
};

/* Test case to format and then lock the LUKS device. */
TEST_F(eStoragedTest, FormatPass)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*esObject, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("mkfs.*/dev/mapper/")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("mount.*/dev/mapper/")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("umount")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, cryptInitByName(_, _)).Times(1);

    EXPECT_CALL(*esObject, cryptDeactivate(_, _)).Times(1);

    /* Format the encrypted device. */
    esObject->format(password);
    EXPECT_FALSE(esObject->is_locked());

    esObject->lock(password);
    EXPECT_TRUE(esObject->is_locked());
}

/* Test case where the device/file doesn't exist. */
TEST_F(eStoragedTest, FormatNoDeviceFail)
{
    /* Delete the test file. */
    EXPECT_EQ(0, unlink(testFileName));

    EXPECT_THROW(esObject->format(password), EncryptionError);
    EXPECT_FALSE(esObject->is_locked());

    /* Create the test file again, so that the TearDown function works. */
    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();
}

/* Test case where we fail to format the LUKS device. */
TEST_F(eStoragedTest, FormatFail)
{
    EXPECT_CALL(*esObject, cryptFormat(_, _, _, _, _, _, _, _))
        .WillOnce(Return(-1));

    EXPECT_THROW(esObject->format(password), EncryptionError);
    EXPECT_FALSE(esObject->is_locked());
}

/* Test case where we fail to set the password for the LUKS device. */
TEST_F(eStoragedTest, AddKeyslotFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*esObject, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .WillOnce(Return(-1));

    EXPECT_THROW(esObject->format(password), EncryptionError);
    EXPECT_TRUE(esObject->is_locked());
}

/* Test case where we fail to load the LUKS header. */
TEST_F(eStoragedTest, LoadLuksHeaderFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*esObject, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, cryptLoad(_, _, _)).WillOnce(Return(-1));

    EXPECT_THROW(esObject->format(password), EncryptionError);
    EXPECT_TRUE(esObject->is_locked());
}

/* Test case where we fail to activate the LUKS device. */
TEST_F(eStoragedTest, ActivateFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*esObject, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(Return(-1));

    EXPECT_THROW(esObject->format(password), EncryptionError);
    EXPECT_TRUE(esObject->is_locked());
}

/* Test case where we fail to create the filesystem. */
TEST_F(eStoragedTest, CreateFilesystemFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*esObject, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("mkfs.*/dev/mapper/")))
        .WillOnce(Return(-1));

    EXPECT_THROW(esObject->format(password), FilesystemError);
    EXPECT_FALSE(esObject->is_locked());
}

/* Test case where we fail to mount the filesystem. */
TEST_F(eStoragedTest, MountFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*esObject, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("mkfs.*/dev/mapper/")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("mount.*/dev/mapper/")))
        .WillOnce(Return(-1));

    EXPECT_THROW(esObject->format(password), FilesystemError);
    EXPECT_FALSE(esObject->is_locked());
}

/* Test case where we fail to unmount the filesystem. */
TEST_F(eStoragedTest, UnmountFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*esObject, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("mkfs.*/dev/mapper/")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("mount.*/dev/mapper/")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("umount")))
        .WillOnce(Return(-1));

    esObject->format(password);
    EXPECT_FALSE(esObject->is_locked());

    EXPECT_THROW(esObject->lock(password), FilesystemError);
    EXPECT_FALSE(esObject->is_locked());

    /* Remove the mount point we created earlier. */
    EXPECT_TRUE(std::filesystem::remove(
        std::filesystem::path(esObject->getMountPoint())));
}

/* Test case where we fail to remove the mount point. */
TEST_F(eStoragedTest, RemoveMountPointFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*esObject, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("mkfs.*/dev/mapper/")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("mount.*/dev/mapper/")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("umount")))
        .WillOnce(Return(0));

    esObject->format(password);
    EXPECT_FALSE(esObject->is_locked());

    /* Remove the mount point we created earlier. */
    EXPECT_TRUE(std::filesystem::remove(
        std::filesystem::path(esObject->getMountPoint())));

    /* This will fail to remove the mount point. */
    EXPECT_THROW(esObject->lock(password), FilesystemError);
    EXPECT_FALSE(esObject->is_locked());
}

/* Test case where we fail to initialize the crypt_device struct. */
TEST_F(eStoragedTest, CryptInitFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*esObject, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("mkfs.*/dev/mapper/")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("mount.*/dev/mapper/")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("umount")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, cryptInitByName(_, _)).WillOnce(Return(-1));

    esObject->format(password);
    EXPECT_FALSE(esObject->is_locked());

    EXPECT_THROW(esObject->lock(password), EncryptionError);
    EXPECT_FALSE(esObject->is_locked());
}

/* Test case where we fail to deactivate the LUKS device. */
TEST_F(eStoragedTest, DeactivateFail)
{
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(TEST_PATH), StrEq(ESTORAGED_INTERFACE), _))
        .WillRepeatedly(Return(0));

    EXPECT_CALL(*esObject, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*esObject, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("mkfs.*/dev/mapper/")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("mount.*/dev/mapper/")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, runCommand(ContainsRegex("umount")))
        .WillOnce(Return(0));

    EXPECT_CALL(*esObject, cryptInitByName(_, _)).Times(1);

    EXPECT_CALL(*esObject, cryptDeactivate(_, _)).WillOnce(Return(-1));

    /* Format the encrypted device. */
    esObject->format(password);
    EXPECT_FALSE(esObject->is_locked());

    EXPECT_THROW(esObject->lock(password), EncryptionError);
    EXPECT_FALSE(esObject->is_locked());
}

} // namespace estoraged_test
