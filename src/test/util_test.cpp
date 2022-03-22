#include <util.hpp>

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
    EXPECT_EQ(findPredictedMediaLifeLeftPercent(prefixName), 30);
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

    EXPECT_EQ(findPredictedMediaLifeLeftPercent(prefixName), 60);
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

} // namespace estoraged_test
