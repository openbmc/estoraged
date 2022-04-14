#include "getConfig.hpp"

#include <unistd.h>

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
}

/* Test case where we successfully find the device file. */
TEST(utilTest, findDevicePass)
{
    estoraged::StorageData data;

    /* Set up the map of properties. */
    data.emplace(std::string("Type"),
                 estoraged::BasicVariantType("EmmcDevice"));
    data.emplace(std::string("Name"), estoraged::BasicVariantType("emmc"));

    /* Create a dummy device file. */
    const std::string testDevName("mmcblk0");
    std::ofstream testFile;
    testFile.open(testDevName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    /* Create another dummy file. */
    const std::string testDummyFilename("abc");
    testFile.open(testDummyFilename,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

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
    EXPECT_EQ(0, unlink(testDevName.c_str()));
    EXPECT_EQ(0, unlink(testDummyFilename.c_str()));
}

/* Test case where the "Type" property doesn't exist. */
TEST(utilTest, findDeviceNoTypeFail)
{
    estoraged::StorageData data;

    /* Set up the map of properties (with the "Type" property missing). */
    data.emplace(std::string("Name"), estoraged::BasicVariantType("emmc"));

    /* Create a dummy device file. */
    const std::string testDevName("mmcblk0");
    std::ofstream testFile;
    testFile.open(testDevName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    /* Create another dummy file. */
    const std::string testDummyFilename("abc");
    testFile.open(testDummyFilename,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    /* Look for the device file. */
    std::filesystem::path deviceFile, sysfsDir;
    std::string luksName;
    EXPECT_FALSE(estoraged::util::findDevice(data, std::filesystem::path("./"),
                                             deviceFile, sysfsDir, luksName));

    /* Delete the dummy files. */
    EXPECT_EQ(0, unlink(testDevName.c_str()));
    EXPECT_EQ(0, unlink(testDummyFilename.c_str()));
}

/* Test case where the device type is not supported. */
TEST(utilTest, findDeviceUnsupportedTypeFail)
{
    estoraged::StorageData data;

    /* Set up the map of properties (with an unsupported "Type"). */
    data.emplace(std::string("Type"), estoraged::BasicVariantType("NvmeDrive"));
    data.emplace(std::string("Name"),
                 estoraged::BasicVariantType("some_drive"));

    /* Create a dummy device file. */
    const std::string testDevName("mmcblk0");
    std::ofstream testFile;
    testFile.open(testDevName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    /* Create another dummy file. */
    const std::string testDummyFilename("abc");
    testFile.open(testDummyFilename,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    /* Look for the device file. */
    std::filesystem::path deviceFile, sysfsDir;
    std::string luksName;
    EXPECT_FALSE(estoraged::util::findDevice(data, std::filesystem::path("./"),
                                             deviceFile, sysfsDir, luksName));

    /* Delete the dummy files. */
    EXPECT_EQ(0, unlink(testDevName.c_str()));
    EXPECT_EQ(0, unlink(testDummyFilename.c_str()));
}

/* Test case where we can't find the device file. */
TEST(utilTest, findDeviceNotFoundFail)
{
    estoraged::StorageData data;

    /* Set up the map of properties. */
    data.emplace(std::string("Type"),
                 estoraged::BasicVariantType("EmmcDevice"));
    data.emplace(std::string("Name"), estoraged::BasicVariantType("emmc"));

    /* Create a dummy file. */
    const std::string testDummyFilename("abc");
    std::ofstream testFile;
    testFile.open(testDummyFilename,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    /* Look for the device file. */
    std::filesystem::path deviceFile, sysfsDir;
    std::string luksName;
    EXPECT_FALSE(estoraged::util::findDevice(data, std::filesystem::path("./"),
                                             deviceFile, sysfsDir, luksName));

    /* Delete the dummy file. */
    EXPECT_EQ(0, unlink(testDummyFilename.c_str()));
}

} // namespace estoraged_test
