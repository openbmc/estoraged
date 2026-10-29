#include "estoraged_test.hpp"

#include "estoraged.hpp"
#include "estoraged_conf.hpp"

#include <linux/mmc/core.h>
#include <linux/mmc/ioctl.h>
#include <linux/mmc/mmc.h>
#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp>
#include <stdplus/fd/gmock.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Inventory/Item/Volume/server.hpp>

#include <array>
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace estoraged_test
{

using sdbusplus::server::xyz::openbmc_project::inventory::item::Volume;
using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;
using sdbusplus::xyz::openbmc_project::Common::Error::UnsupportedRequest;
using std::filesystem::path;
using stdplus::fd::FdMock;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::StrEq;

static auto fsckDevpathIs(const auto& devPath)
{
    return ElementsAre("-t", "ext4", "-y", devPath);
}

static const std::filesystem::path& testTmpDir()
{
    static const TestTmpDir tmpDir;
    return tmpDir.get();
}

/*
 * Returns true if an executable called @name can be found in $PATH. Uses the
 * same search rules as execvpe(), including the fallback search path if PATH
 * has not been set.
 */
static bool inPath(std::string_view name)
{
    const char* pathEnv = getenv("PATH");
    std::string_view dirs = pathEnv != nullptr ? pathEnv : "/bin:/usr/bin";
    while (true)
    {
        const size_t colon = dirs.find(':');
        std::string_view dir = dirs.substr(0, colon);
        /* An empty PATH entry means the current working directory. */
        const std::filesystem::path candidate =
            std::filesystem::path(dir.empty() ? "." : dir) / name;
        if (access(candidate.c_str(), X_OK) == 0)
        {
            return true;
        }
        if (colon == std::string_view::npos)
        {
            return false;
        }
        dirs.remove_prefix(colon + 1);
    }
}

/* Whether the tools invoked by estoraged::Filesystem are available. */
static bool haveFilesystemTools()
{
    return inPath("mkfs.ext4") && inPath("fsck");
}

class EStoragedTest : public testing::Test
{
  public:
    const std::string testFileName = (testTmpDir() / "testfile").string();
    const char* testLuksDevName = "testfile_luksDev";
    const std::string testCryptDir = testTmpDir().string();
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

        std::unique_ptr<FdMock> mockFd = std::make_unique<FdMock>();
        esObject = std::make_unique<estoraged::EStoraged>(
            std::move(mockFd), *objectServer, testConfigPath, testFileName,
            testLuksDevName, testSize, testLifeTime, testPartNumber,
            testSerialNumber, testLocationCode, ERASE_MAX_GEOMETRY,
            ERASE_MIN_GEOMETRY, testDriveType, testDriveProtocol,
            std::move(cryptIface), std::move(fsIface));
    }

    void TearDown() override
    {
        EXPECT_EQ(0, unlink(testFileName.c_str()));
    }
};

/* Must match EStoragedTest::testCryptDir and EStoragedTest::testLuksDevName. */
static std::filesystem::path mappedDevicePath()
{
    return testTmpDir() / "testfile_luksDev";
}

std::ofstream mappedDevice;

int createMappedDev()
{
    mappedDevice.open(mappedDevicePath(),
                      std::ios::out | std::ios::binary | std::ios::trunc);
    mappedDevice.seekp(10 * 1024 * 1024 - 1);
    mappedDevice.write("", 1);
    mappedDevice.close();
    if (mappedDevice.fail())
    {
        throw std::runtime_error("Failed to open test mapped device");
    }

    return 0;
}

int removeMappedDev()
{
    EXPECT_EQ(0, unlink(mappedDevicePath().c_str()));

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

    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface,
                runFsck(fsckDevpathIs(esObject->getCryptDevicePath())))
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
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _))
        .WillOnce(&removeMappedDev);

    /* Format the encrypted device. */
    esObject->formatLuks(password, Volume::FilesystemType::ext4);
    EXPECT_FALSE(esObject->isLocked());

    esObject->lock();
    EXPECT_TRUE(esObject->isLocked());
}

/* Test case where fsck repairs filesystem errors (exit code 1). */
TEST_F(EStoragedTest, FormatFsckRepairsErrorsPass)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(&createMappedDev);

    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface,
                runFsck(fsckDevpathIs(esObject->getCryptDevicePath())))
        .WillOnce(Return(1));

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
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _))
        .WillOnce(&removeMappedDev);

    /* Format the encrypted device. */
    esObject->formatLuks(password, Volume::FilesystemType::ext4);
    EXPECT_FALSE(esObject->isLocked());

    esObject->lock();
    EXPECT_TRUE(esObject->isLocked());
}

/* Test case to format and then lock the LUKS device using real Filesystem for
 * runMkfs and runFsck. */
TEST_F(EStoragedTest, FormatWithRealFilesystem)
{
    if (!haveFilesystemTools())
    {
        GTEST_SKIP() << "mkfs.ext4 and/or fsck not found in PATH";
    }

    estoraged::Filesystem realFs;

    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(&createMappedDev);

    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce([&realFs](std::initializer_list<std::string_view> args) {
            return realFs.runMkfs(args);
        });

    EXPECT_CALL(*mockFsIface,
                runFsck(fsckDevpathIs(esObject->getCryptDevicePath())))
        .WillOnce([&realFs](std::initializer_list<std::string_view> args) {
            return realFs.runFsck(args);
        });

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
        .WillOnce(Return(0));

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

    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface,
                runFsck(fsckDevpathIs(esObject->getCryptDevicePath())))
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
        .WillOnce(Return(0));

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
    EXPECT_EQ(0, unlink(testFileName.c_str()));

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

    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce(Return(-1));

    /* formatLuks() must lock the device again after the failure. */
    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _))
        .WillOnce(&removeMappedDev);

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_TRUE(esObject->isLocked());
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

    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface,
                runFsck(fsckDevpathIs(esObject->getCryptDevicePath())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    /* formatLuks() must lock the device again after the failure. */
    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _))
        .WillOnce(&removeMappedDev);

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_TRUE(esObject->isLocked());
}

/* Test case where we fail to mount the filesystem when formatting. */
TEST_F(EStoragedTest, FormatMountFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(&createMappedDev);

    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface,
                runFsck(fsckDevpathIs(esObject->getCryptDevicePath())))
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
        .WillOnce(Return(0));

    /*
     * If this filesystem fails to mount when initially formatting the device,
     * there is a major issue and decrypting the device most likely won't be
     * enough to recover.
     */
    /* formatLuks() must lock the device again after the failure. */
    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _))
        .WillOnce(&removeMappedDev);

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_TRUE(esObject->isLocked());
}

/* Test case where fsck fails with uncorrected errors (exit code 4). */
TEST_F(EStoragedTest, FormatFsckErrorsFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(&createMappedDev);

    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface,
                runFsck(fsckDevpathIs(esObject->getCryptDevicePath())))
        .WillOnce(Return(4));

    /* formatLuks() must lock the device again after the failure. */
    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _))
        .WillOnce(&removeMappedDev);

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_TRUE(esObject->isLocked());
}

/* Test case where we fail to mount the filesystem when unlocking a device. */
TEST_F(EStoragedTest, UnlockMountFail)
{
    /* formatLuks: formatLuksDev */
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    /* formatLuks: formatLuksDev */
    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    /*
     * unlock: activateLuksDev
     * formatLuks: activateLuksDev
     */
    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(2);

    /*
     * unlock: activateLuksDev
     * formatLuks: activateLuksDev
     */
    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .Times(2)
        .WillRepeatedly(&createMappedDev);

    /* formatLuks: createFilesystem */
    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce(Return(0));

    /*
     * unlock: mountFilesystem
     * formatLuks: mountFilesystem
     */
    EXPECT_CALL(*mockFsIface,
                runFsck(fsckDevpathIs(esObject->getCryptDevicePath())))
        .WillOnce(Return(-1))
        .WillOnce(Return(0));

    /* formatLuks: mountFilesystem */
    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    /* formatLuks: mountFilesystem */
    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    /* formatLuks: mountFilesystem */
    EXPECT_CALL(*mockFsIface,
                doMount(StrEq(esObject->getCryptDevicePath()),
                        StrEq(esObject->getMountPoint()), _, _, _))
        .WillOnce(Return(0));

    /* unlock: deactivateLuksDev */
    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _))
        .WillOnce(&removeMappedDev);

    /*
     * The device can be decrypted, but the filesystem can't be mounted.
     * This is either due to:
     * 1. the filesystem wasn't created when formatLuks was originally ran due
     *    to the service crashing or unit being abruptly powered off.
     * 2. the filesystem was corrupted to the point where it can't be recovered
     *    via fsck or some other utility.
     * Re-encrypt the device so that it is possible to recover by reformatting.
     */
    EXPECT_THROW(esObject->unlock(password), InternalFailure);
    EXPECT_TRUE(esObject->isLocked());

    /* Expect to be able to reformat the device to recover */
    EXPECT_NO_THROW(
        esObject->formatLuks(password, Volume::FilesystemType::ext4));
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

    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface,
                runFsck(fsckDevpathIs(esObject->getCryptDevicePath())))
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
        .WillOnce(testing::DoAll(testing::Assign(&errno, EBUSY), Return(-1)));

    esObject->formatLuks(password, Volume::FilesystemType::ext4);
    EXPECT_FALSE(esObject->isLocked());

    EXPECT_THROW(esObject->lock(), InternalFailure);
    EXPECT_FALSE(esObject->isLocked());

    EXPECT_EQ(0, removeMappedDev());
}

/*
 * Test case where the volume is unlocked but the filesystem is not mounted.
 * lock() must still deactivate the LUKS device.
 */
TEST_F(EStoragedTest, LockNotMounted)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(&createMappedDev);

    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface,
                runFsck(fsckDevpathIs(esObject->getCryptDevicePath())))
        .WillOnce(Return(0));

    /* formatLuks: mountFilesystem */
    EXPECT_CALL(*mockFsIface, directoryExists(path(esObject->getMountPoint())))
        .WillOnce(Return(false));

    EXPECT_CALL(*mockFsIface, createDirectory(path(esObject->getMountPoint())))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockFsIface,
                doMount(StrEq(esObject->getCryptDevicePath()),
                        StrEq(esObject->getMountPoint()), _, _, _))
        .WillOnce(Return(0));

    /* The filesystem is no longer mounted, e.g. after a manual umount. */
    EXPECT_CALL(*mockFsIface, doUnmount(StrEq(esObject->getMountPoint())))
        .WillOnce(testing::DoAll(testing::Assign(&errno, EINVAL), Return(-1)));

    EXPECT_CALL(*mockFsIface, removeDirectory(path(esObject->getMountPoint())))
        .WillOnce(testing::DoAll(testing::Assign(&errno, ENOENT), Return(-1)));

    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _))
        .WillOnce(&removeMappedDev);

    esObject->formatLuks(password, Volume::FilesystemType::ext4);
    EXPECT_FALSE(esObject->isLocked());

    EXPECT_NO_THROW(esObject->lock());
    EXPECT_TRUE(esObject->isLocked());
}

/*
 * Test case where formatLuks() fails after activation and locking the device
 * again fails too. The original error must be reported.
 */
TEST_F(EStoragedTest, FormatFailDeactivateFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(&createMappedDev);

    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce(Return(-1));

    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _)).WillOnce(Return(-1));

    EXPECT_THROW(esObject->formatLuks(password, Volume::FilesystemType::ext4),
                 InternalFailure);
    EXPECT_FALSE(esObject->isLocked());

    EXPECT_EQ(0, removeMappedDev());
}

/*
 * Test case where we fail to remove the mount point.
 * lock() must still deactivate the LUKS device.
 */
TEST_F(EStoragedTest, RemoveMountPointFail)
{
    EXPECT_CALL(*mockCryptIface, cryptFormat(_, _, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptKeyslotAddByVolumeKey(_, _, _, _, _, _))
        .Times(1);

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, _, _)).Times(1);

    EXPECT_CALL(*mockCryptIface, cryptActivateByPassphrase(_, _, _, _, _, _))
        .WillOnce(&createMappedDev);

    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface,
                runFsck(fsckDevpathIs(esObject->getCryptDevicePath())))
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
        .WillOnce(testing::DoAll(testing::Assign(&errno, EBUSY), Return(-1)));

    EXPECT_CALL(*mockCryptIface, cryptDeactivate(_, _))
        .WillOnce(&removeMappedDev);

    esObject->formatLuks(password, Volume::FilesystemType::ext4);
    EXPECT_FALSE(esObject->isLocked());

    /* Lock must succeed even if removing the mount point fails. */
    EXPECT_NO_THROW(esObject->lock());
    EXPECT_TRUE(esObject->isLocked());
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

    EXPECT_CALL(
        *mockFsIface,
        runMkfs(ElementsAre("-E", "discard", esObject->getCryptDevicePath())))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockFsIface,
                runFsck(fsckDevpathIs(esObject->getCryptDevicePath())))
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
        .WillOnce(Return(0));

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

TEST_F(EStoragedTest, EmptyPasswordFail)
{
    const std::vector<uint8_t> emptyPassword;
    EXPECT_THROW(
        esObject->formatLuks(emptyPassword, Volume::FilesystemType::ext4),
        InternalFailure);
    EXPECT_THROW(esObject->unlock(emptyPassword), InternalFailure);
    EXPECT_THROW(esObject->changePassword(emptyPassword, password),
                 InternalFailure);
    EXPECT_THROW(esObject->changePassword(password, emptyPassword),
                 InternalFailure);
}

/* LogicalVerify must be rejected if no LogicalOverWrite preceded it. */
TEST_F(EStoragedTest, LogicalVerifyWithoutOverWriteFails)
{
    EXPECT_THROW(esObject->erase(Volume::EraseMethod::LogicalVerify),
                 UnsupportedRequest);
}

/*
 * A failed LogicalOverWrite must not enable LogicalVerify. The test device is
 * a regular file, so querying its size with BLKGETSIZE64 fails.
 */
TEST_F(EStoragedTest, LogicalVerifyAfterFailedOverWriteFails)
{
    EXPECT_THROW(esObject->erase(Volume::EraseMethod::LogicalOverWrite),
                 InternalFailure);
    EXPECT_THROW(esObject->erase(Volume::EraseMethod::LogicalVerify),
                 UnsupportedRequest);
}

TEST(EMMCBackgroundOperation, IoCtlFailure)
{
    std::unique_ptr<FdMock> mockFd = std::make_unique<FdMock>();

    EXPECT_CALL(*mockFd, ioctl(MMC_IOC_CMD, testing::_)).WillOnce(Return(1));
    EXPECT_THROW(estoraged::EStoraged::enableBackgroundOperation(
                     std::move(mockFd), "/dev/test"),
                 estoraged::BkopsIoctlFailure);
}

TEST(EMMCBackgroundOperation, BkOpsNotSupported)
{
    std::unique_ptr<FdMock> mockFd = std::make_unique<FdMock>();

    EXPECT_CALL(*mockFd, ioctl(MMC_IOC_CMD, testing::_)).WillOnce(Return(0));
    EXPECT_FALSE(estoraged::EStoraged::enableBackgroundOperation(
        std::move(mockFd), "/dev/test"));
}

TEST(EMMCBackgroundOperation, EnableFailure)
{
    std::unique_ptr<FdMock> mockFd = std::make_unique<FdMock>();

    EXPECT_CALL(*mockFd, ioctl(MMC_IOC_CMD, testing::_))
        .WillOnce(testing::Invoke([](unsigned long, void* data) {
            struct mmc_ioc_cmd* idata = static_cast<struct mmc_ioc_cmd*>(data);
            EXPECT_EQ(idata->opcode, MMC_SEND_EXT_CSD);
            EXPECT_EQ(idata->blksz, 512);
            EXPECT_EQ(idata->flags, MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC);

            // Set specific bytes to simulate a hardware state
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            *reinterpret_cast<uint8_t*>(idata->data_ptr + 502) =
                0x01; // BKOPS_SUPPORT = Supported
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            *reinterpret_cast<uint8_t*>(idata->data_ptr + 163) =
                0x00; // BKOPS_EN = Disabled
            return 0; // Success
        }))
        .WillOnce(Return(1));
    EXPECT_THROW(estoraged::EStoraged::enableBackgroundOperation(
                     std::move(mockFd), "/dev/test"),
                 estoraged::BkopsEnableFailure);
}

TEST(EMMCBackgroundOperation, AlreadyEnabled)
{
    std::unique_ptr<FdMock> mockFd = std::make_unique<FdMock>();

    EXPECT_CALL(*mockFd, ioctl(MMC_IOC_CMD, testing::_))
        .WillOnce(testing::Invoke([](unsigned long, void* data) {
            struct mmc_ioc_cmd* idata = static_cast<struct mmc_ioc_cmd*>(data);
            EXPECT_EQ(idata->opcode, MMC_SEND_EXT_CSD);
            EXPECT_EQ(idata->blksz, 512);
            EXPECT_EQ(idata->flags, MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC);

            // Set specific bytes to simulate a hardware state
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            *reinterpret_cast<uint8_t*>(idata->data_ptr + 502) =
                0x01; // BKOPS_SUPPORT = Supported
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            *reinterpret_cast<uint8_t*>(idata->data_ptr + 163) =
                EXT_CSD_MANUAL_BKOPS_MASK; // BKOPS_EN = Manual Mode
            return 0;                      // Success
        }));
    EXPECT_FALSE(estoraged::EStoraged::enableBackgroundOperation(
        std::move(mockFd), "/dev/test"));
}

TEST(EMMCBackgroundOperation, EnableSuccess)
{
    std::unique_ptr<FdMock> mockFd = std::make_unique<FdMock>();

    EXPECT_CALL(*mockFd, ioctl(MMC_IOC_CMD, testing::_))
        .WillOnce(testing::Invoke([](unsigned long, void* data) {
            struct mmc_ioc_cmd* idata = static_cast<struct mmc_ioc_cmd*>(data);
            EXPECT_EQ(idata->opcode, MMC_SEND_EXT_CSD);
            EXPECT_EQ(idata->blksz, 512);
            EXPECT_EQ(idata->flags, MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC);

            // Set specific bytes to simulate a hardware state
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            *reinterpret_cast<uint8_t*>(idata->data_ptr + 502) =
                0x01; // BKOPS_SUPPORT = Supported
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            *reinterpret_cast<uint8_t*>(idata->data_ptr + 163) =
                0x00; // BKOPS_EN = Disabled
            return 0; // Success
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

TEST(EMMCBackgroundOperation, Null)
{
    EXPECT_FALSE(
        estoraged::EStoraged::enableBackgroundOperation(nullptr, "/dev/test"));
}

class FilesystemTest : public testing::Test
{
  protected:
    const std::filesystem::path imgPath =
        testTmpDir() / "estoraged_test_fs.img";
    const std::filesystem::path testDir = testTmpDir() / "estoraged_test_dir";

    void SetUp() override
    {
        std::error_code ec;
        std::filesystem::remove(imgPath, ec);
        std::filesystem::remove_all(testDir, ec);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(imgPath, ec);
        std::filesystem::remove_all(testDir, ec);
    }
};

TEST_F(FilesystemTest, RunMkfsAndRunFsckSuccess)
{
    if (!haveFilesystemTools())
    {
        GTEST_SKIP() << "mkfs.ext4 and/or fsck not found in PATH";
    }

    /* Create a 10MB sparse file for filesystem operations. */
    std::ofstream imgFile(imgPath,
                          std::ios::out | std::ios::binary | std::ios::trunc);
    imgFile.seekp(10 * 1024 * 1024 - 1);
    imgFile.write("", 1);
    imgFile.close();
    ASSERT_TRUE(imgFile.good());
    ASSERT_TRUE(std::filesystem::exists(imgPath));

    estoraged::Filesystem fs;
    std::string imgStr = imgPath.string();
    EXPECT_EQ(fs.runMkfs({"-F", "-E", "discard", imgStr}), 0);
    EXPECT_EQ(fs.runFsck({"-t", "ext4", "-y", imgStr}), 0);
}

TEST_F(FilesystemTest, RunMkfsFailure)
{
    if (!haveFilesystemTools())
    {
        GTEST_SKIP() << "mkfs.ext4 and/or fsck not found in PATH";
    }

    estoraged::Filesystem fs;
    EXPECT_NE(fs.runMkfs({"--invalid-option-xyz"}), 0);
}

TEST_F(FilesystemTest, RunFsckFailure)
{
    if (!haveFilesystemTools())
    {
        GTEST_SKIP() << "mkfs.ext4 and/or fsck not found in PATH";
    }

    estoraged::Filesystem fs;
    EXPECT_NE(fs.runFsck({"/dev/null", "--invalid-option-xyz"}), 0);
}

TEST_F(FilesystemTest, DirectoryOperations)
{
    estoraged::Filesystem fs;
    EXPECT_FALSE(fs.directoryExists(testDir));
    EXPECT_TRUE(fs.createDirectory(testDir));
    EXPECT_TRUE(fs.directoryExists(testDir));
    EXPECT_EQ(fs.removeDirectory(testDir), 0);
    EXPECT_FALSE(fs.directoryExists(testDir));
}

TEST_F(FilesystemTest, DirectoryOperationsFailureDoesNotThrow)
{
    estoraged::Filesystem fs;
    /* The parent directory does not exist. */
    const std::filesystem::path missingParent =
        testTmpDir() / "nonexistent_parent" / "dir";
    EXPECT_NO_THROW(EXPECT_FALSE(fs.createDirectory(missingParent)));
    EXPECT_NO_THROW(EXPECT_FALSE(fs.directoryExists(missingParent)));
    EXPECT_NO_THROW(EXPECT_EQ(fs.removeDirectory(missingParent), -1));
    EXPECT_EQ(errno, ENOENT);

    /* Removing a non-empty directory fails. */
    const std::filesystem::path nonEmpty = testTmpDir() / "nonempty_dir";
    ASSERT_TRUE(std::filesystem::create_directory(nonEmpty));
    std::ofstream(nonEmpty / "file") << "x";
    EXPECT_NO_THROW(EXPECT_EQ(fs.removeDirectory(nonEmpty), -1));
    EXPECT_TRUE(errno == ENOTEMPTY || errno == EEXIST);
    std::filesystem::remove_all(nonEmpty);
}

TEST_F(FilesystemTest, MountFailure)
{
    estoraged::Filesystem fs;
    EXPECT_EQ(fs.doMount("/dev/null", "/nonexistent_path_xyz", "ext4", 0,
                         nullptr),
              -1);
}

TEST_F(FilesystemTest, UnmountFailure)
{
    estoraged::Filesystem fs;
    EXPECT_EQ(fs.doUnmount("/nonexistent_path_xyz"), -1);
}

TEST(EStoragedSignalTest, ConstructorSkipsInitialPropertiesChanged)
{
    const std::string devFile = (testTmpDir() / "sigtest_emmc").string();
    {
        std::ofstream f(devFile,
                        std::ios::out | std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(f.good());
    }

    boost::asio::io_context io;
    auto serverConn = std::make_shared<sdbusplus::asio::connection>(
        io, sdbusplus::bus::new_bus());
    auto clientConn = std::make_shared<sdbusplus::asio::connection>(
        io, sdbusplus::bus::new_bus());

    constexpr std::string_view objPath =
        "/xyz/openbmc_project/inventory/storage/sigtest_emmc";
    size_t propChangedCount = 0;
    size_t interfacesAddedCount = 0;

    sdbusplus::bus::match_t propMatch(
        static_cast<sdbusplus::bus_t&>(*clientConn),
        sdbusplus::bus::match::rules::type::signal() +
            sdbusplus::bus::match::rules::member("PropertiesChanged") +
            sdbusplus::bus::match::rules::path(objPath),
        [&](sdbusplus::message_t&) { ++propChangedCount; });

    sdbusplus::bus::match_t iaMatch(
        static_cast<sdbusplus::bus_t&>(*clientConn),
        sdbusplus::bus::match::rules::interfacesAdded("/"),
        [&](sdbusplus::message_t& msg) {
            estoraged::object_path iaPath;
            msg.read(iaPath);
            if (iaPath.str == objPath)
            {
                ++interfacesAddedCount;
            }
        });

    auto pump = [&](std::chrono::milliseconds ms) {
        io.restart();
        boost::asio::steady_timer timer(io, ms);
        timer.async_wait([&io](const boost::system::error_code&) {
            io.stop();
        });
        io.run();
    };
    pump(std::chrono::milliseconds(20));

    auto cryptIface = std::make_unique<MockCryptsetupInterface>();
    EXPECT_CALL(*cryptIface, cryptGetDir)
        .WillOnce(Return(testTmpDir().string()));
    auto fsIface = std::make_unique<MockFilesystemInterface>();
    auto mockFd = std::make_unique<FdMock>();

    sdbusplus::asio::object_server objServer(serverConn);
    estoraged::EStoraged esObj(
        std::move(mockFd), objServer,
        "/xyz/openbmc_project/inventory/system/board/b0/emmc", devFile,
        "sigtest_luks", 24, 25, "PN1", "SN1", "U10", ERASE_MAX_GEOMETRY,
        ERASE_MIN_GEOMETRY, "SSD", "eMMC", std::move(cryptIface),
        std::move(fsIface));

    pump(std::chrono::milliseconds(50));

    EXPECT_EQ(interfacesAddedCount, 6U);
    EXPECT_EQ(propChangedCount, 0U);
    EXPECT_EQ(unlink(devFile.c_str()), 0);
}

} // namespace estoraged_test
