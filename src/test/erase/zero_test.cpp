#include "estoraged_conf.hpp"
#include "zero.hpp"

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

using estoraged::Zero;
using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using stdplus::fd::ManagedFd;

namespace estoraged_test
{

TEST(Zeros, zeroPass)
{
    std::string testFileName = "testfile_pass";
    std::ofstream testFile;

    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();
    uint64_t size = 4096;
    Zero pass(testFileName);
    EXPECT_NO_THROW(pass.writeZero(size));
    EXPECT_NO_THROW(pass.verifyZero(size));
}

/* This test that pattern writes the correct number of bytes even if
 * size of the drive is not divisable by the block size
 */
TEST(Zeros, notDivisible)
{
    std::string testFileName = "testfile_notDivisible";
    std::ofstream testFile;

    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    uint64_t size = 4097;
    // 4097 is not divisible by the block size, and we expect no errors
    Zero pass(testFileName);
    EXPECT_NO_THROW(pass.writeZero(size));
    EXPECT_NO_THROW(pass.verifyZero(size));
}

TEST(Zeros, notZeroStart)
{
    std::string testFileName = "testfile_notZeroStart";
    std::ofstream testFile;

    // open the file and write none zero to it
    int dummyValue = 0x88776655;
    testFile.open(testFileName, std::ios::binary | std::ios::out);
    testFile.write((const char*)&dummyValue, sizeof(dummyValue));
    testFile.close();
    uint64_t size = 4096;
    Zero pass(testFileName);
    EXPECT_NO_THROW(pass.writeZero(size - sizeof(dummyValue)));

    // force flush, needed for unit testing
    std::ofstream file;
    file.open(testFileName);
    file.close();

    EXPECT_THROW(pass.verifyZero(size), InternalFailure);
}

TEST(Zeros, notZeroEnd)
{
    std::string testFileName = "testfile_notZeroEnd";
    std::ofstream testFile;

    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    uint64_t size = 4096;
    Zero pass(testFileName);
    int dummyValue = 88;
    EXPECT_NO_THROW(pass.writeZero(size - sizeof(dummyValue)));

    // open the file and write none zero to it
    testFile.open(testFileName, std::ios::out);
    testFile << dummyValue;
    testFile.close();

    EXPECT_THROW(pass.verifyZero(size), InternalFailure);
}

} // namespace estoraged_test
