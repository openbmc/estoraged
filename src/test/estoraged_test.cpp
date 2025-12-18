#include "estoraged_test.hpp"

#include "estoraged.hpp"
#include "estoraged_conf.hpp"

#include <linux/mmc/core.h>
#include <linux/mmc/ioctl.h>
#include <linux/mmc/mmc.h>
#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Inventory/Item/Volume/server.hpp>

#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace estoraged_test
{

using sdbusplus::server::xyz::openbmc_project::inventory::item::Volume;
using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;
using std::filesystem::path;
using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Return;
using ::testing::StrEq;

class MockFd : public stdplus::Fd
{
  public:
    MOCK_METHOD(int, ioctl, (unsigned long id, void* data), (override));
    MOCK_METHOD(int, constIoctl, (unsigned long id, void* data),
                (const, override));
    MOCK_METHOD(std::span<std::byte>, read, (std::span<std::byte>), (override));
    MOCK_METHOD(std::span<std::byte>, recv,
                (std::span<std::byte>, stdplus::fd::RecvFlags), (override));
    MOCK_METHOD((std::tuple<std::span<std::byte>, std::span<std::byte>>),
                recvfrom,
                (std::span<std::byte>, stdplus::fd::RecvFlags,
                 std::span<std::byte>),
                (override));
    MOCK_METHOD(std::span<const std::byte>, write, (std::span<const std::byte>),
                (override));
    MOCK_METHOD(std::span<const std::byte>, send,
                (std::span<const std::byte>, stdplus::fd::SendFlags),
                (override));
    MOCK_METHOD(std::span<const std::byte>, sendto,
                (std::span<const std::byte>, stdplus::fd::SendFlags,
                 std::span<const std::byte>),
                (override));
    MOCK_METHOD(size_t, lseek, (off_t, stdplus::fd::Whence), (override));
    MOCK_METHOD(void, truncate, (off_t), (override));
    MOCK_METHOD(void, bind, (std::span<const std::byte>), (override));
    MOCK_METHOD(void, connect, (std::span<const std::byte>), (override));
    MOCK_METHOD(void, listen, (int), (override));
    MOCK_METHOD((std::optional<std::tuple<int, std::span<std::byte>>>), accept,
                (std::span<std::byte> sockaddr), (override));
    MOCK_METHOD(void, setsockopt,
                (stdplus::fd::SockLevel, stdplus::fd::SockOpt,
                 std::span<const std::byte>),
                (override));
    MOCK_METHOD(void, fcntlSetfd, (stdplus::fd::FdFlags), (override));
    MOCK_METHOD(stdplus::fd::FdFlags, fcntlGetfd, (), (const, override));
    MOCK_METHOD(void, fcntlSetfl, (stdplus::fd::FileFlags), (override));
    MOCK_METHOD(stdplus::fd::FileFlags, fcntlGetfl, (), (const override));
    MOCK_METHOD(std::span<std::byte>, mmap,
                (std::byte*, std::size_t, stdplus::fd::ProtFlags,
                 stdplus::fd::MMapFlags, off_t),
                (override));
    MOCK_METHOD(void, munmap, (std::span<std::byte>), (override));
};

class EStoragedTest : public testing::Test
{
  public:
    const char* testFileName = "testfile";
    const char* testLuksDevName = "testfile_luksDev";
    const char* testCryptDir = "/tmp";
    const std::string testConfigPath =
        "/xyz/openbmc_project/inventory/system/board/test_board/test_emmc";
    const uint64_t testSize = 24;
    const uint8_t testLifeTime = 25;
    const std::string testPartNumber = "12345678";
    const std::string testSerialNumber = "ABCDEF1234";
    const std::string testLocationCode = "U102020";
    const std::string testDriveType = "SSD";
    const std::string testDriveProtocol = "eMMC";
    std::ofstream testFile;
    std::string passwordString;
    std::vector<uint8_t> password;
    MockCryptsetupInterface* mockCryptIface{};
    static const constexpr std::array<const char*, 2> options = {
        "-E", "discard"};
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

        /* Set up location of dummy mapped crypt file. */
        EXPECT_CALL(*cryptIface, cryptGetDir).WillOnce(Return(testCryptDir));

        conn = std::make_shared<sdbusplus::asio::connection>(io);
        // request D-Bus server name.
        conn->request_name("xyz.openbmc_project.eStoraged.test");
        objectServer = std::make_unique<sdbusplus::asio::object_server>(conn);

        std::unique_ptr<MockFd> mockFd = std::make_unique<MockFd>();
        esObject = std::make_unique<estoraged::EStoraged>(
            std::move(mockFd), *objectServer, testConfigPath, testFileName,
            testLuksDevName, testSize, testLifeTime, testPartNumber,
            testSerialNumber, testLocationCode, ERASE_MAX_GEOMETRY,
            ERASE_MIN_GEOMETRY, testDriveType, testDriveProtocol,
            std::move(cryptIface), std::move(fsIface));
    }

    void TearDown() override
    {
        EXPECT_EQ(0, unlink(testFileName));
    }
};

const char* mappedDevicePath = "/tmp/testfile_luksDev";
std::ofstream mappedDevice;

int createMappedDev()
{
    mappedDevice.open(mappedDevicePath,
                      std::ios::out | std::ios::binary | std::ios::trunc);
    mappedDevice.close();
    if (mappedDevice.fail())
    {
        throw std::runtime_error("Failed to open test mapped device");
    }

    return 0;
}

int removeMappedDev()
{
    EXPECT_EQ(0, unlink(mappedDevicePath));

    return 0;
}

/* Test case to format and then lock the LUKS device. */
TEST_F(EStoragedTest, FormatPass)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(&createMappedDev);

    EXPECT_CALL(*mockFsIface, runMkfs(StrEq(esObject->getCryptDevicePath()),
                                      ElementsAreArray(EStoragedTest::options)))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, runFsck(StrEq(esObject->getCryptDevicePath()),
                                      StrEq("-t ext4 -p")))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockFsIface,
                doMount(StrEq(esObject->getCryptDevicePath()),
                        StrEq(esObject->getMountPoint()), _, _, _))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, doUnmount(StrEq(esObject->getMountPoint())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, removeDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _))
        .WillOnce(&removeMappedDev);

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
        .WillOnce(&createMappedDev);

    EXPECT_CALL(*mockFsIface, runMkfs(StrEq(esObject->getCryptDevicePath()),
                                      ElementsAreArray(EStoragedTest::options)))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, runFsck(StrEq(esObject->getCryptDevicePath()),
                                      StrEq("-t ext4 -p")))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .Times(0);

    EXPECT_CALL(*mockFsIface,
                doMount(StrEq(esObject->getCryptDevicePath()),
                        StrEq(esObject->getMountPoint()), _, _, _))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, doUnmount(StrEq(esObject->getMountPoint())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, removeDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _))
        .WillOnce(&removeMappedDev);

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
    EXPECT_TRUE(esObject->isLocked());

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
    EXPECT_TRUE(esObject->isLocked());
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
        .WillOnce(&createMappedDev);

    EXPECT_CALL(*mockFsIface, runMkfs(StrEq(esObject->getCryptDevicePath()),
                                      ElementsAreArray(EStoragedTest::options)))
        .WillOnce(Return(-1));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_FALSE(esObject->isLocked());

    EXPECT_EQ(0, removeMappedDev());
}

/* Test case where we fail to create the mount point. */
TEST_F(EStoragedTest, CreateMountPointFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(&createMappedDev);

    EXPECT_CALL(*mockFsIface, runMkfs(StrEq(esObject->getCryptDevicePath()),
                                      ElementsAreArray(EStoragedTest::options)))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, runFsck(StrEq(esObject->getCryptDevicePath()),
                                      StrEq("-t ext4 -p")))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_FALSE(esObject->isLocked());

    EXPECT_EQ(0, removeMappedDev());
}

/* Test case where we fail to mount the filesystem. */
TEST_F(EStoragedTest, MountFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(&createMappedDev);

    EXPECT_CALL(*mockFsIface, runMkfs(StrEq(esObject->getCryptDevicePath()),
                                      ElementsAreArray(EStoragedTest::options)))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, runFsck(StrEq(esObject->getCryptDevicePath()),
                                      StrEq("-t ext4 -p")))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockFsIface,
                doMount(StrEq(esObject->getCryptDevicePath()),
                        StrEq(esObject->getMountPoint()), _, _, _))
        .WillOnce(Return(-1));

    EXPECT_CALL(*mockFsIface, removeDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_FALSE(esObject->isLocked());

    EXPECT_EQ(0, removeMappedDev());
}

/* Test case where we fail to unmount the filesystem. */
TEST_F(EStoragedTest, UnmountFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(&createMappedDev);

    EXPECT_CALL(*mockFsIface, runMkfs(StrEq(esObject->getCryptDevicePath()),
                                      ElementsAreArray(EStoragedTest::options)))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, runFsck(StrEq(esObject->getCryptDevicePath()),
                                      StrEq("-t ext4 -p")))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockFsIface,
                doMount(StrEq(esObject->getCryptDevicePath()),
                        StrEq(esObject->getMountPoint()), _, _, _))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, doUnmount(StrEq(esObject->getMountPoint())))
        .WillOnce(Return(-1));

    esObject->formatLuks(password, Volume::FilesystemType::ext4);
    EXPECT_FALSE(esObject->isLocked());

    EXPECT_THROW(esObject->lock(), InternalFailure);
    EXPECT_FALSE(esObject->isLocked());

    EXPECT_EQ(0, removeMappedDev());
}

/* Test case where we fail to remove the mount point. */
TEST_F(EStoragedTest, RemoveMountPointFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(&createMappedDev);

    EXPECT_CALL(*mockFsIface, runMkfs(StrEq(esObject->getCryptDevicePath()),
                                      ElementsAreArray(EStoragedTest::options)))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, runFsck(StrEq(esObject->getCryptDevicePath()),
                                      StrEq("-t ext4 -p")))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockFsIface,
                doMount(StrEq(esObject->getCryptDevicePath()),
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

    EXPECT_EQ(0, removeMappedDev());
}

/* Test case where we fail to deactivate the LUKS device. */
TEST_F(EStoragedTest, DeactivateFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(&createMappedDev);

    EXPECT_CALL(*mockFsIface, runMkfs(StrEq(esObject->getCryptDevicePath()),
                                      ElementsAreArray(EStoragedTest::options)))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, runFsck(StrEq(esObject->getCryptDevicePath()),
                                      StrEq("-t ext4 -p")))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockFsIface,
                doMount(StrEq(esObject->getCryptDevicePath()),
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

    EXPECT_EQ(0, removeMappedDev());
}

/* Test case where we successfully change the password. */
TEST_F(EStoragedTest, ChangePasswordSuccess)
{
    std::string newPasswordString("newPassword");
    std::vector<uint8_t> newPassword(newPasswordString.begin(),
                                     newPasswordString.end());

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface,
                cryptKeyslotChangeByPassphrase(
                    _, _, _, reinterpret_cast<const char*>(password.data()),
                    password.size(),
                    reinterpret_cast<const char*>(newPassword.data()),
                    newPassword.size()))
        .WillOnce(Return(0));

    /* Change the password for the LUKS-encrypted device. */
    esObject->changePassword(password, newPassword);
}

/* Test case where we fail to change the password. */
TEST_F(EStoragedTest, ChangePasswordFail)
{
    std::string newPasswordString("newPassword");
    std::vector<uint8_t> newPassword(newPasswordString.begin(),
                                     newPasswordString.end());

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface,
                cryptKeyslotChangeByPassphrase(
                    _, _, _, reinterpret_cast<const char*>(password.data()),
                    password.size(),
                    reinterpret_cast<const char*>(newPassword.data()),
                    newPassword.size()))
        .WillOnce(Return(-1));

    EXPECT_THROW(esObject->changePassword(password, newPassword),
                 InternalFailure);
}

TEST(EMMCBackgroundOperation, EnableSuccess)
{
    std::unique_ptr<MockFd> mockFd = std::make_unique<MockFd>();

    EXPECT_CALL(*mockFd, ioctl(MMC_IOC_CMD, testing::_))
        .WillOnce(testing::Invoke([](unsigned long, void* data) {
            struct mmc_ioc_cmd* idata = static_cast<struct mmc_ioc_cmd*>(data);
            EXPECT_EQ(idata->opcode, MMC_SEND_EXT_CSD);
            EXPECT_EQ(idata->blksz, 512);
            EXPECT_EQ(idata->flags, MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC);

            uint8_t* extCsd = reinterpret_cast<uint8_t*>(idata->data_ptr);

            // Set specific bytes to simulate a hardware state
            extCsd[502] = 0x01; // BKOPS_SUPPORT = Supported
            extCsd[163] = 0x00; // BKOPS_EN = Disabled
            return 0;           // Success
        }))
        .WillOnce(testing::Invoke([](unsigned long, void* data) {
            struct mmc_ioc_cmd* idata = static_cast<struct mmc_ioc_cmd*>(data);
            EXPECT_EQ(idata->opcode, MMC_SWITCH);
            EXPECT_EQ(idata->arg, (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                                      (EXT_CSD_BKOPS_EN << 16) |
                                      (EXT_CSD_MANUAL_BKOPS_MASK << 8) |
                                      EXT_CSD_CMD_SET_NORMAL);
            EXPECT_EQ(idata->flags, MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC);
            return 0; // Success
        }));
    EXPECT_TRUE(estoraged::EStoraged::enableBackgroundOperation(
        std::move(mockFd), "/dev/test"));
}

TEST(EMMCBackgroundOperation, IoCtlFailure)
{
    std::unique_ptr<MockFd> mockFd = std::make_unique<MockFd>();

    EXPECT_CALL(*mockFd, ioctl(MMC_IOC_CMD, testing::_)).WillOnce(Return(1));
    EXPECT_THROW(estoraged::EStoraged::enableBackgroundOperation(
                     std::move(mockFd), "/dev/test"),
                 estoraged::BkopsIoctlFailure);
}

TEST(EMMCBackgroundOperation, BkOpsNotSupported)
{
    std::unique_ptr<MockFd> mockFd = std::make_unique<MockFd>();

    EXPECT_CALL(*mockFd, ioctl(MMC_IOC_CMD, testing::_)).WillOnce(Return(0));
    EXPECT_THROW(estoraged::EStoraged::enableBackgroundOperation(
                     std::move(mockFd), "/dev/test"),
                 estoraged::BkopsUnsupported);
}

TEST(EMMCBackgroundOperation, EnableFailure)
{
    std::unique_ptr<MockFd> mockFd = std::make_unique<MockFd>();

    EXPECT_CALL(*mockFd, ioctl(MMC_IOC_CMD, testing::_))
        .WillOnce(testing::Invoke([](unsigned long, void* data) {
            struct mmc_ioc_cmd* idata = static_cast<struct mmc_ioc_cmd*>(data);
            EXPECT_EQ(idata->opcode, MMC_SEND_EXT_CSD);
            EXPECT_EQ(idata->blksz, 512);
            EXPECT_EQ(idata->flags, MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC);

            uint8_t* extCsd = reinterpret_cast<uint8_t*>(idata->data_ptr);

            // Set specific bytes to simulate a hardware state
            extCsd[502] = 0x01; // BKOPS_SUPPORT = Supported
            extCsd[163] = 0x00; // BKOPS_EN = Disabled
            return 0;           // Success
        }))
        .WillOnce(Return(1));
    EXPECT_THROW(estoraged::EStoraged::enableBackgroundOperation(
                     std::move(mockFd), "/dev/test"),
                 estoraged::BkopsEnableFailure);
}

} // namespace estoraged_test
