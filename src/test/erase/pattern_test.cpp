#include "estoraged_conf.hpp"
#include "pattern.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <stdplus/fd/create.hpp>
#include <stdplus/fd/gmock.hpp>
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
using testing::_;
using testing::Action;
using testing::Invoke;
using testing::Return;

TEST(pattern, patternPass)
{
    std::string testFileName = "patternPass";
    uint64_t size = 4096;
    std::ofstream testFile;
    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    stdplus::fd::Fd&& writeFd =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::WriteOnly);

    Pattern pass(testFileName);
    EXPECT_NO_THROW(pass.writePattern(size, writeFd));

    stdplus::fd::Fd&& readFd =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadOnly);

    EXPECT_NO_THROW(pass.verifyPattern(size, readFd));
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

    stdplus::fd::Fd&& writeFd =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::WriteOnly);
    Pattern pass(testFileName);
    EXPECT_NO_THROW(pass.writePattern(size, writeFd));

    stdplus::fd::Fd&& readFd =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadOnly);
    EXPECT_NO_THROW(pass.verifyPattern(size, readFd));
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

    stdplus::fd::Fd&& writeFd =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::WriteOnly);

    EXPECT_NO_THROW(pass.writePattern(size - sizeof(dummyValue), writeFd));

    stdplus::fd::Fd&& readFd =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadOnly);

    EXPECT_THROW(pass.verifyPattern(size, readFd), InternalFailure);
}

uint64_t size = 4096;
size_t shortSize = 128;
static auto shortData_1 = std::vector<std::byte>(shortSize);
static auto shortData_2 = std::vector<std::byte>(shortSize);
static auto restOfData = std::vector<std::byte>(size - shortSize * 2);

TEST(pattern, shortReadWritePass)
{
    std::string testFileName = "testfile_shortRead";

    uint64_t size = 4096;
    // size_t shortSize = 128;
    Pattern pass(testFileName);
    stdplus::fd::FdMock mock;

    testing::Sequence s;

    // test write pattern with short blocks
    // EXPECT_CALL(mock, write(std::span<std::byte> x))
    EXPECT_CALL(mock, write(_))
        .InSequence(s)
        .WillRepeatedly(Invoke([](std::span<const std::byte> x) {
            std::copy_n(x.begin(), shortData_1.size(), shortData_1.begin());
            return shortData_1;
        }));
    EXPECT_CALL(mock, write(_))
        .InSequence(s)
        .WillRepeatedly(Invoke([](std::span<const std::byte> x) {
            std::copy_n(x.begin(), restOfData.size(), restOfData.begin());
            return restOfData;
        }));
    EXPECT_CALL(mock, write(_))
        .InSequence(s)
        .WillRepeatedly(Invoke([](std::span<const std::byte> x) {
            std::copy_n(x.begin(), shortData_2.size(), shortData_2.begin());
            return shortData_2;
        }));

    EXPECT_NO_THROW(pass.writePattern(size, mock));
}

ACTION(ThrowException)
{
    throw InternalFailure();
}

TEST(pattern, driveIsSmaller)
{
    std::string testFileName = "testfile_driveIsSmaller";

    uint64_t size = 4096;
    Pattern tryPattern(testFileName);

    stdplus::fd::FdMock mocks;
    testing::InSequence s;

    // test writes
    EXPECT_CALL(mocks, write(_))
        .Times(100)
        .WillRepeatedly(Return(std::vector<std::byte>{}))
        .RetiresOnSaturation();
    EXPECT_CALL(mocks, write(_)).WillOnce(ThrowException());

    EXPECT_THROW(tryPattern.writePattern(size, mocks), InternalFailure);

    // test reads
    EXPECT_CALL(mocks, read(_))
        .Times(100)
        .WillRepeatedly(Return(std::vector<std::byte>{}))
        .RetiresOnSaturation();
    EXPECT_CALL(mocks, read(_)).WillOnce(ThrowException());

    EXPECT_THROW(tryPattern.verifyPattern(size, mocks), InternalFailure);
}

} // namespace estoraged_test
