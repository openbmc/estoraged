#include "getConfig.hpp"

#include <boost/container/flat_map.hpp>
#include <util.hpp>

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
    std::string prefixName = ".";
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

    std::string prefixName = ".";
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

    std::string prefixName = ".";
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
    std::string prefixName = ".";
    std::string testFileName = prefixName + "/name";
    /* The part name file won't exist for this test. */
    EXPECT_EQ(getPartNumber(prefixName), "unknown");
}

TEST(utilTest, getPartNumberPass)
{
    std::string prefixName = ".";
    std::string testFileName = prefixName + "/name";
    std::ofstream testFile;
    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile << "ABCD1234";
    testFile.close();
    EXPECT_EQ(getPartNumber(prefixName), "ABCD1234");
    EXPECT_TRUE(std::filesystem::remove(testFileName));
}

TEST(utilTest, getSerialNumberFail)
{
    std::string prefixName = ".";
    std::string testFileName = prefixName + "/serial";
    /* The serial number file won't exist for this test. */
    EXPECT_EQ(getSerialNumber(prefixName), "unknown");
}

TEST(utilTest, getSerialNumberPass)
{
    std::string prefixName = ".";
    std::string testFileName = prefixName + "/serial";
    std::ofstream testFile;
    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile << "0x12345678";
    testFile.close();
    EXPECT_EQ(getSerialNumber(prefixName), "0x12345678");
    EXPECT_TRUE(std::filesystem::remove(testFileName));
}

/* Test case where we successfully find the device file. */
TEST(utilTest, findDevicePass)
{
    estoraged::StorageData data;

    /* Set up the map of properties. */
    data.emplace(std::string("Type"),
                 estoraged::BasicVariantType("EmmcDevice"));
    data.emplace(std::string("Name"), estoraged::BasicVariantType("emmc"));

    /* Create a dummy device. */
    std::filesystem::create_directories("abc/device");
    const std::string dummyTypeFileName("abc/device/type");
    std::ofstream dummyTypeFile(dummyTypeFileName,
                                std::ios::out | std::ios::trunc);
    dummyTypeFile << "SSD";
    dummyTypeFile.close();

    /* Another device. */
    std::filesystem::create_directories("def/device");

    /* Create a dummy eMMC device. */
    std::filesystem::create_directories("mmcblk0/device");
    const std::string typeFileName("mmcblk0/device/type");
    std::ofstream typeFile(typeFileName, std::ios::out | std::ios::trunc);
    typeFile << "MMC";
    typeFile.close();

    /* Look for the device file. */
    std::filesystem::path deviceFile, sysfsDir;
    std::string luksName;
    EXPECT_TRUE(estoraged::util::findDevice(data, std::filesystem::path("./"),
                                            deviceFile, sysfsDir, luksName));

    /* Validate the results. */
    EXPECT_EQ("/dev/mmcblk0", deviceFile.string());
    EXPECT_EQ("./mmcblk0/device", sysfsDir.string());
    EXPECT_EQ("luks-mmcblk0", luksName);

    /* Delete the dummy files. */
    EXPECT_EQ(3U, std::filesystem::remove_all("mmcblk0"));
    EXPECT_EQ(3U, std::filesystem::remove_all("abc"));
    EXPECT_EQ(2U, std::filesystem::remove_all("def"));
}

/* Test case where the "Type" property doesn't exist. */
TEST(utilTest, findDeviceNoTypeFail)
{
    estoraged::StorageData data;

    /* Set up the map of properties (with the "Type" property missing). */
    data.emplace(std::string("Name"), estoraged::BasicVariantType("emmc"));

    /* Create a dummy device. */
    std::filesystem::create_directories("abc/device");
    const std::string dummyTypeFileName("abc/device/type");
    std::ofstream dummyTypeFile(dummyTypeFileName,
                                std::ios::out | std::ios::trunc);
    dummyTypeFile << "SSD";
    dummyTypeFile.close();

    /* Another device. */
    std::filesystem::create_directories("def/device");

    /* Create a dummy eMMC device. */
    std::filesystem::create_directories("mmcblk0/device");
    const std::string typeFileName("mmcblk0/device/type");
    std::ofstream typeFile(typeFileName, std::ios::out | std::ios::trunc);
    typeFile << "MMC";
    typeFile.close();

    /* Look for the device file. */
    std::filesystem::path deviceFile, sysfsDir;
    std::string luksName;
    EXPECT_FALSE(estoraged::util::findDevice(data, std::filesystem::path("./"),
                                             deviceFile, sysfsDir, luksName));

    /* Delete the dummy files. */
    EXPECT_EQ(3U, std::filesystem::remove_all("mmcblk0"));
    EXPECT_EQ(3U, std::filesystem::remove_all("abc"));
    EXPECT_EQ(2U, std::filesystem::remove_all("def"));
}

/* Test case where the device type is not supported. */
TEST(utilTest, findDeviceUnsupportedTypeFail)
{
    estoraged::StorageData data;

    /* Set up the map of properties (with an unsupported "Type"). */
    data.emplace(std::string("Type"), estoraged::BasicVariantType("NvmeDrive"));
    data.emplace(std::string("Name"),
                 estoraged::BasicVariantType("some_drive"));

    /* Create a dummy device. */
    std::filesystem::create_directories("abc/device");
    const std::string dummyTypeFileName("abc/device/type");
    std::ofstream dummyTypeFile(dummyTypeFileName,
                                std::ios::out | std::ios::trunc);
    dummyTypeFile << "SSD";
    dummyTypeFile.close();

    /* Another device. */
    std::filesystem::create_directories("def/device");

    /* Create a dummy eMMC device. */
    std::filesystem::create_directories("mmcblk0/device");
    const std::string typeFileName("mmcblk0/device/type");
    std::ofstream typeFile(typeFileName, std::ios::out | std::ios::trunc);
    typeFile << "MMC";
    typeFile.close();

    /* Look for the device file. */
    std::filesystem::path deviceFile, sysfsDir;
    std::string luksName;
    EXPECT_FALSE(estoraged::util::findDevice(data, std::filesystem::path("./"),
                                             deviceFile, sysfsDir, luksName));

    /* Delete the dummy files. */
    EXPECT_EQ(3U, std::filesystem::remove_all("mmcblk0"));
    EXPECT_EQ(3U, std::filesystem::remove_all("abc"));
    EXPECT_EQ(2U, std::filesystem::remove_all("def"));
}

/* Test case where we can't find the device file. */
TEST(utilTest, findDeviceNotFoundFail)
{
    estoraged::StorageData data;

    /* Set up the map of properties. */
    data.emplace(std::string("Type"),
                 estoraged::BasicVariantType("EmmcDevice"));
    data.emplace(std::string("Name"), estoraged::BasicVariantType("emmc"));

    /* Create a dummy device. */
    std::filesystem::create_directories("abc/device");
    const std::string dummyTypeFileName("abc/device/type");
    std::ofstream dummyTypeFile(dummyTypeFileName,
                                std::ios::out | std::ios::trunc);
    dummyTypeFile << "SSD";
    dummyTypeFile.close();

    /* Another device. */
    std::filesystem::create_directories("def/device");

    /* Look for the device file. */
    std::filesystem::path deviceFile, sysfsDir;
    std::string luksName;
    EXPECT_FALSE(estoraged::util::findDevice(data, std::filesystem::path("./"),
                                             deviceFile, sysfsDir, luksName));

    /* Delete the dummy files. */
    EXPECT_EQ(3U, std::filesystem::remove_all("abc"));
    EXPECT_EQ(2U, std::filesystem::remove_all("def"));
}

} // namespace estoraged_test
