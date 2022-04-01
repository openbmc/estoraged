#include "estoraged_conf.hpp"
#include "pattern.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <fstream>
#include <system_error>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace estoraged_test
{

using estoraged::Pattern;
using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

TEST(pattern, patternPass)
{
    std::string testFileName = "patternPass";
    uint64_t size = 4096;
    std::ofstream testFile;
    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    Pattern pass(testFileName);
    EXPECT_NO_THROW(pass.writePattern(
        size,
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite)));
    EXPECT_NO_THROW(pass.verifyPattern(
        size,
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite)));
}

/* This test that pattern writes the correct number of bytes even if
 * size of the drive is not divisable by the block size
 */
TEST(pattern, patternNotDivisible)
{
    std::string testFileName = "notDivisible";
    uint64_t size = 4097;
    // 4097 is not divisible by the block size, and we expect no errors
    std::ofstream testFile;
    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    Pattern pass(testFileName);
    EXPECT_NO_THROW(pass.writePattern(
        size,
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite)));
    EXPECT_NO_THROW(pass.verifyPattern(
        size,
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite)));
}

TEST(pattern, patternsDontMatch)
{
    std::string testFileName = "patternsDontMatch";
    uint64_t size = 4096;
    std::ofstream testFile;

    Pattern pass(testFileName);

    int dummyValue = 88;
    testFile.open(testFileName, std::ios::binary | std::ios::out);
    testFile.write((reinterpret_cast<const char*>(&dummyValue)),
                   sizeof(dummyValue));
    testFile.close();

    EXPECT_NO_THROW(pass.writePattern(
        size - sizeof(dummyValue),
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite)));
    EXPECT_THROW(
        pass.verifyPattern(
            size, stdplus::fd::open(testFileName,
                                    stdplus::fd::OpenAccess::ReadWrite)),
        InternalFailure);
}

} // namespace estoraged_test
