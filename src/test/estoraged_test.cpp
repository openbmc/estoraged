#include "estoraged_test.hpp"

#include "estoraged.hpp"

#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Inventory/Item/Volume/client.hpp>

#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace estoraged_test
{

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;
using sdbusplus::xyz::openbmc_project::Inventory::Item::server::Volume;
using std::filesystem::path;
using ::testing::_;
using ::testing::ContainsRegex;
using ::testing::Return;
using ::testing::StrEq;

class EStoragedTest : public testing::Test
{
  public:
    const char* testFileName = "testfile";
    const char* testLuksDevName = "testfile_luksDev";
    const std::string testConfigPath =
        "/xyz/openbmc_project/inventory/system/board/test_board/test_emmc";
    const uint64_t testSize = 24;
    const uint8_t testLifeTime = 25;
    std::ofstream testFile;
    std::string passwordString;
    std::vector<uint8_t> password;
    MockCryptsetupInterface* mockCryptIface{};
    MockFilesystemInterface* mockFsIface{};
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> conn;
    std::unique_ptr<sdbusplus::asio::object_server> objectServer;
    std::unique_ptr<estoraged::EStoraged> esObject;

    EStoragedTest() :
        passwordString("password"),
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

        std::unique_ptr<MockCryptsetupInterface> cryptIface =
            std::make_unique<MockCryptsetupInterface>();
        mockCryptIface = cryptIface.get();
        std::unique_ptr<MockFilesystemInterface> fsIface =
            std::make_unique<MockFilesystemInterface>();
        mockFsIface = fsIface.get();

        conn = std::make_shared<sdbusplus::asio::connection>(io);
        // request D-Bus server name.
        conn->request_name("xyz.openbmc_project.eStoraged.test");
        objectServer = std::make_unique<sdbusplus::asio::object_server>(conn);

        esObject = std::make_unique<estoraged::EStoraged>(
            *objectServer, testConfigPath, testFileName, testLuksDevName,
            testSize, testLifeTime, std::move(cryptIface), std::move(fsIface));
    }

    void TearDown() override
    {
        EXPECT_EQ(0, unlink(testFileName));
    }
};

/* Test case to format and then lock the LUKS device. */
TEST_F(EStoragedTest, FormatPass)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

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

/*
 * Test case where the mount point directory already exists, so it shouldn't
 * try to create it.
 */
TEST_F(EStoragedTest, MountPointExistsPass)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .Times(0);

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
TEST_F(EStoragedTest, FormatNoDeviceFail)
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
TEST_F(EStoragedTest, FormatFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _))
        .WillOnce(Return(-1));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_FALSE(esObject->isLocked());
}

/* Test case where we fail to set the password for the LUKS device. */
TEST_F(EStoragedTest, AddKeyslotFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .WillOnce(Return(-1));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_TRUE(esObject->isLocked());
}

/* Test case where we fail to load the LUKS header. */
TEST_F(EStoragedTest, LoadLuksHeaderFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).WillOnce(Return(-1));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_TRUE(esObject->isLocked());
}

/* Test case where we fail to activate the LUKS device. */
TEST_F(EStoragedTest, ActivateFail)
{
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
TEST_F(EStoragedTest, CreateFilesystemFail)
{
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
TEST_F(EStoragedTest, CreateMountPointFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_FALSE(esObject->isLocked());
}

/* Test case where we fail to mount the filesystem. */
TEST_F(EStoragedTest, MountFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

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
TEST_F(EStoragedTest, UnmountFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

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
TEST_F(EStoragedTest, RemoveMountPointFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

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
TEST_F(EStoragedTest, DeactivateFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockFsIface, runMkfs(testLuksDevName)).WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

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
