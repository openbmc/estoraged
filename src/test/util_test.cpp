#include "estoraged_conf.hpp"
#include "estoraged_test.hpp"
#include "getConfig.hpp"

#include <boost/container/flat_map.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <util.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <filesystem>
#include <fstream>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace estoraged_test
{
using estoraged::util::findPredictedMediaLifeLeftPercent;
using estoraged::util::getPartNumber;
using estoraged::util::getSerialNumber;

TEST(utilTest, passFindPredictedMediaLife)
{
    TestTmpDir tmpDir;
    std::string prefixName = tmpDir.get().string();
    std::string testFileName = prefixName + "/life_time";
    std::ofstream testFile;
    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile << "0x07 0x04";
    testFile.close();
    EXPECT_EQ(findPredictedMediaLifeLeftPercent(prefixName), 40);
    EXPECT_TRUE(std::filesystem::remove(testFileName));
}

TEST(utilTest, estimatesSame)
{
    TestTmpDir tmpDir;
    std::string prefixName = tmpDir.get().string();
    std::string testFileName = prefixName + "/life_time";
    std::ofstream testFile;
    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile << "0x04 0x04";
    testFile.close();

    EXPECT_EQ(findPredictedMediaLifeLeftPercent(prefixName), 70);
    EXPECT_TRUE(std::filesystem::remove(testFileName));
}

TEST(utilTest, estimatesNotAvailable)
{
    TestTmpDir tmpDir;
    std::string prefixName = tmpDir.get().string();
    std::string testFileName = prefixName + "/life_time";
    std::ofstream testFile;
    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    EXPECT_EQ(findPredictedMediaLifeLeftPercent(prefixName), 255);
    EXPECT_TRUE(std::filesystem::remove(testFileName));
}

TEST(utilTest, getPartNumberFail)
{
    TestTmpDir tmpDir;
    /* The part name file won't exist for this test. */
    EXPECT_EQ(getPartNumber(tmpDir.get()), "unknown");
}

TEST(utilTest, getPartNumberPass)
{
    TestTmpDir tmpDir;
    std::filesystem::path testFileName = tmpDir.get() / "name";
    std::ofstream testFile;
    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile << "ABCD1234";
    testFile.close();
    EXPECT_EQ(getPartNumber(tmpDir.get()), "ABCD1234");
    EXPECT_TRUE(std::filesystem::remove(testFileName));
}

TEST(utilTest, getSerialNumberFail)
{
    TestTmpDir tmpDir;
    /* The serial number file won't exist for this test. */
    EXPECT_EQ(getSerialNumber(tmpDir.get()), "unknown");
}

TEST(utilTest, getSerialNumberPass)
{
    TestTmpDir tmpDir;
    std::filesystem::path testFileName = tmpDir.get() / "serial";
    std::ofstream testFile;
    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile << "0x12345678";
    testFile.close();
    EXPECT_EQ(getSerialNumber(tmpDir.get()), "0x12345678");
    EXPECT_TRUE(std::filesystem::remove(testFileName));
}

/* Test case where we successfully find the device file. */
TEST(utilTest, findDevicePass)
{
    TestTmpDir tmpDir;
    const std::filesystem::path& searchDir = tmpDir.get();
    estoraged::StorageData data;

    /* Set up the map of properties. */
    data.emplace(std::string("Type"),
                 estoraged::BasicVariantType("EmmcDevice"));
    data.emplace(std::string("Name"), estoraged::BasicVariantType("emmc"));
    data.emplace(std::string("LocationCode"),
                 estoraged::BasicVariantType("U102020"));

    /* Create a dummy device. */
    std::filesystem::create_directories(searchDir / "abc/device");
    const std::filesystem::path dummyTypeFileName =
        searchDir / "abc/device/type";
    std::ofstream dummyTypeFile(dummyTypeFileName,
                                std::ios::out | std::ios::trunc);
    dummyTypeFile << "SSD";
    dummyTypeFile.close();

    /* Another device. */
    std::filesystem::create_directories(searchDir / "def/device");

    /* Create a dummy eMMC device. */
    std::filesystem::create_directories(searchDir / "mmcblk0/device");
    const std::filesystem::path typeFileName =
        searchDir / "mmcblk0/device/type";
    std::ofstream typeFile(typeFileName, std::ios::out | std::ios::trunc);
    typeFile << "MMC";
    typeFile.close();

    /* Look for the device file. */
    auto result = estoraged::util::findDevice(data, searchDir);
    EXPECT_TRUE(result.has_value());

    /* Validate the results. */
    EXPECT_EQ("/dev/mmcblk0", result->deviceFile.string());
    EXPECT_EQ(searchDir / "mmcblk0/device", result->sysfsDir);
    EXPECT_EQ("luks-mmcblk0", result->luksName);
    EXPECT_EQ("U102020", result->locationCode);
    EXPECT_EQ(ERASE_MAX_GEOMETRY, result->eraseMaxGeometry);
    EXPECT_EQ(ERASE_MIN_GEOMETRY, result->eraseMinGeometry);
    EXPECT_EQ("SSD", result->driveType);
    EXPECT_EQ("eMMC", result->driveProtocol);

    /* Delete the dummy files. */
    EXPECT_EQ(3U, std::filesystem::remove_all(searchDir / "mmcblk0"));
    EXPECT_EQ(3U, std::filesystem::remove_all(searchDir / "abc"));
    EXPECT_EQ(2U, std::filesystem::remove_all(searchDir / "def"));
}

/* Helper that creates a dummy sysfs block device with the given type. */
static void createDummyBlockDevice(const std::filesystem::path& searchDir,
                                   const std::string& name,
                                   const std::string& type)
{
    std::filesystem::create_directories(searchDir / name / "device");
    std::ofstream typeFile(searchDir / name / "device/type",
                           std::ios::out | std::ios::trunc);
    typeFile << type;
}

/* Test case where eMMC boot partitions and partitions are skipped. */
TEST(utilTest, findDeviceSkipsBootAndPartitions)
{
    TestTmpDir tmpDir;
    const std::filesystem::path& searchDir = tmpDir.get();
    estoraged::StorageData data;
    data.emplace(std::string("Type"),
                 estoraged::BasicVariantType("EmmcDevice"));

    createDummyBlockDevice(searchDir, "mmcblk0boot0", "MMC");
    createDummyBlockDevice(searchDir, "mmcblk0boot1", "MMC");
    createDummyBlockDevice(searchDir, "mmcblk0p1", "MMC");
    std::ofstream(searchDir / "mmcblk0p1/partition") << "1";
    createDummyBlockDevice(searchDir, "mmcblk0", "MMC");

    auto result = estoraged::util::findDevice(data, searchDir);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ("/dev/mmcblk0", result->deviceFile.string());
    EXPECT_EQ(searchDir / "mmcblk0/device", result->sysfsDir);
}

/* Test case where only eMMC boot partitions and partitions exist. */
TEST(utilTest, findDeviceOnlyBootAndPartitionsFail)
{
    TestTmpDir tmpDir;
    const std::filesystem::path& searchDir = tmpDir.get();
    estoraged::StorageData data;
    data.emplace(std::string("Type"),
                 estoraged::BasicVariantType("EmmcDevice"));

    createDummyBlockDevice(searchDir, "mmcblk0boot0", "MMC");
    createDummyBlockDevice(searchDir, "mmcblk0p1", "MMC");
    std::ofstream(searchDir / "mmcblk0p1/partition") << "1";

    EXPECT_FALSE(estoraged::util::findDevice(data, searchDir).has_value());
}

/* Test case where we successfully find the device file with updating the
 * min/max geometry. */
TEST(utilTest, findDeviceWithMaxAndMinGeometryPass)
{
    TestTmpDir tmpDir;
    const std::filesystem::path& searchDir = tmpDir.get();
    estoraged::StorageData data;

    /* Set up the map of properties. */
    data.emplace(std::string("Type"),
                 estoraged::BasicVariantType("EmmcDevice"));
    data.emplace(std::string("Name"), estoraged::BasicVariantType("emmc"));
    data.emplace(std::string("LocationCode"),
                 estoraged::BasicVariantType("U102020"));
    data.emplace(std::string("EraseMaxGeometry"),
                 estoraged::BasicVariantType((uint64_t)5566));
    data.emplace(std::string("EraseMinGeometry"),
                 estoraged::BasicVariantType((uint64_t)1234));

    /* Create a dummy device. */
    std::filesystem::create_directories(searchDir / "abc/device");
    const std::filesystem::path dummyTypeFileName =
        searchDir / "abc/device/type";
    std::ofstream dummyTypeFile(dummyTypeFileName,
                                std::ios::out | std::ios::trunc);
    dummyTypeFile << "SSD";
    dummyTypeFile.close();

    /* Another device. */
    std::filesystem::create_directories(searchDir / "def/device");

    /* Create a dummy eMMC device. */
    std::filesystem::create_directories(searchDir / "mmcblk0/device");
    const std::filesystem::path typeFileName =
        searchDir / "mmcblk0/device/type";
    std::ofstream typeFile(typeFileName, std::ios::out | std::ios::trunc);
    typeFile << "MMC";
    typeFile.close();

    /* Look for the device file. */
    auto result = estoraged::util::findDevice(data, searchDir);
    EXPECT_TRUE(result.has_value());

    /* Validate the results. */
    EXPECT_EQ("/dev/mmcblk0", result->deviceFile.string());
    EXPECT_EQ(searchDir / "mmcblk0/device", result->sysfsDir);
    EXPECT_EQ("luks-mmcblk0", result->luksName);
    EXPECT_EQ("U102020", result->locationCode);
    EXPECT_EQ(5566, result->eraseMaxGeometry);
    EXPECT_EQ(1234, result->eraseMinGeometry);
    EXPECT_EQ("SSD", result->driveType);
    EXPECT_EQ("eMMC", result->driveProtocol);

    /* Delete the dummy files. */
    EXPECT_EQ(3U, std::filesystem::remove_all(searchDir / "mmcblk0"));
    EXPECT_EQ(3U, std::filesystem::remove_all(searchDir / "abc"));
    EXPECT_EQ(2U, std::filesystem::remove_all(searchDir / "def"));
}

/* Test case where the "Type" property doesn't exist. */
TEST(utilTest, findDeviceNoTypeFail)
{
    TestTmpDir tmpDir;
    const std::filesystem::path& searchDir = tmpDir.get();
    estoraged::StorageData data;

    /* Set up the map of properties (with the "Type" property missing). */
    data.emplace(std::string("Name"), estoraged::BasicVariantType("emmc"));

    /* Create a dummy device. */
    std::filesystem::create_directories(searchDir / "abc/device");
    const std::filesystem::path dummyTypeFileName =
        searchDir / "abc/device/type";
    std::ofstream dummyTypeFile(dummyTypeFileName,
                                std::ios::out | std::ios::trunc);
    dummyTypeFile << "SSD";
    dummyTypeFile.close();

    /* Another device. */
    std::filesystem::create_directories(searchDir / "def/device");

    /* Create a dummy eMMC device. */
    std::filesystem::create_directories(searchDir / "mmcblk0/device");
    const std::filesystem::path typeFileName =
        searchDir / "mmcblk0/device/type";
    std::ofstream typeFile(typeFileName, std::ios::out | std::ios::trunc);
    typeFile << "MMC";
    typeFile.close();

    /* Look for the device file. */
    auto result = estoraged::util::findDevice(data, searchDir);
    EXPECT_FALSE(result.has_value());

    /* Delete the dummy files. */
    EXPECT_EQ(3U, std::filesystem::remove_all(searchDir / "mmcblk0"));
    EXPECT_EQ(3U, std::filesystem::remove_all(searchDir / "abc"));
    EXPECT_EQ(2U, std::filesystem::remove_all(searchDir / "def"));
}

/* Test case where the device type is not supported. */
TEST(utilTest, findDeviceUnsupportedTypeFail)
{
    TestTmpDir tmpDir;
    const std::filesystem::path& searchDir = tmpDir.get();
    estoraged::StorageData data;

    /* Set up the map of properties (with an unsupported "Type"). */
    data.emplace(std::string("Type"), estoraged::BasicVariantType("NvmeDrive"));
    data.emplace(std::string("Name"),
                 estoraged::BasicVariantType("some_drive"));

    /* Create a dummy device. */
    std::filesystem::create_directories(searchDir / "abc/device");
    const std::filesystem::path dummyTypeFileName =
        searchDir / "abc/device/type";
    std::ofstream dummyTypeFile(dummyTypeFileName,
                                std::ios::out | std::ios::trunc);
    dummyTypeFile << "SSD";
    dummyTypeFile.close();

    /* Another device. */
    std::filesystem::create_directories(searchDir / "def/device");

    /* Create a dummy eMMC device. */
    std::filesystem::create_directories(searchDir / "mmcblk0/device");
    const std::filesystem::path typeFileName =
        searchDir / "mmcblk0/device/type";
    std::ofstream typeFile(typeFileName, std::ios::out | std::ios::trunc);
    typeFile << "MMC";
    typeFile.close();

    /* Look for the device file. */
    auto result = estoraged::util::findDevice(data, searchDir);
    EXPECT_FALSE(result.has_value());

    /* Delete the dummy files. */
    EXPECT_EQ(3U, std::filesystem::remove_all(searchDir / "mmcblk0"));
    EXPECT_EQ(3U, std::filesystem::remove_all(searchDir / "abc"));
    EXPECT_EQ(2U, std::filesystem::remove_all(searchDir / "def"));
}

/* Test case where we can't find the device file. */
TEST(utilTest, findDeviceNotFoundFail)
{
    TestTmpDir tmpDir;
    const std::filesystem::path& searchDir = tmpDir.get();
    estoraged::StorageData data;

    /* Set up the map of properties. */
    data.emplace(std::string("Type"),
                 estoraged::BasicVariantType("EmmcDevice"));
    data.emplace(std::string("Name"), estoraged::BasicVariantType("emmc"));

    /* Create a dummy device. */
    std::filesystem::create_directories(searchDir / "abc/device");
    const std::filesystem::path dummyTypeFileName =
        searchDir / "abc/device/type";
    std::ofstream dummyTypeFile(dummyTypeFileName,
                                std::ios::out | std::ios::trunc);
    dummyTypeFile << "SSD";
    dummyTypeFile.close();

    /* Another device. */
    std::filesystem::create_directories(searchDir / "def/device");

    /* Look for the device file. */
    auto result = estoraged::util::findDevice(data, searchDir);
    EXPECT_FALSE(result.has_value());

    /* Delete the dummy files. */
    EXPECT_EQ(3U, std::filesystem::remove_all(searchDir / "abc"));
    EXPECT_EQ(2U, std::filesystem::remove_all(searchDir / "def"));
}

TEST(utilTest, findSizeOfBlockDeviceFail)
{
    using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
    EXPECT_THROW(estoraged::util::findSizeOfBlockDevice("/nonexistent_device"),
                 InternalFailure);
}

TEST(utilTest, syncBlockDeviceRegularFilePass)
{
    TestTmpDir tmpDir;
    const std::filesystem::path testFileName = tmpDir.get() / "regular";
    std::ofstream(testFileName) << "data";
    stdplus::fd::ManagedFd fd = stdplus::fd::open(
        testFileName.c_str(), stdplus::fd::OpenAccess::WriteOnly);
    EXPECT_NO_THROW(
        estoraged::util::syncBlockDevice(fd.get(), testFileName.string()));
}

TEST(utilTest, syncBlockDeviceInvalidFdFail)
{
    using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
    EXPECT_THROW(estoraged::util::syncBlockDevice(-1, "/dev/invalid"),
                 InternalFailure);
}

/* BLKFLSBUF is only supported for block devices. */
TEST(utilTest, dropBlockDeviceCacheRegularFileFail)
{
    using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
    TestTmpDir tmpDir;
    const std::filesystem::path testFileName = tmpDir.get() / "regular";
    std::ofstream(testFileName) << "data";
    stdplus::fd::ManagedFd fd = stdplus::fd::open(
        testFileName.c_str(), stdplus::fd::OpenAccess::ReadOnly);
    EXPECT_THROW(
        estoraged::util::dropBlockDeviceCache(fd.get(), testFileName.string()),
        InternalFailure);
}

TEST(utilTest, dropBlockDeviceCacheInvalidFdFail)
{
    using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
    EXPECT_THROW(estoraged::util::dropBlockDeviceCache(-1, "/dev/invalid"),
                 InternalFailure);
}

TEST(utilTest, findDeviceNonStringTypeFail)
{
    TestTmpDir tmpDir;
    estoraged::StorageData data;
    data.emplace(std::string("Type"), estoraged::BasicVariantType(uint64_t{1}));

    auto result = estoraged::util::findDevice(data, tmpDir.get());
    EXPECT_FALSE(result.has_value());
}

} // namespace estoraged_test
