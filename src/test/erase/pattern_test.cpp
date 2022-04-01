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
    std::cerr << "about to write" << std::endl;
    EXPECT_NO_THROW(pass.writePattern(size, writeFd));

    stdplus::fd::Fd&& readFd =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadOnly);

    std::cerr << "about to verify" << std::endl;
    EXPECT_NO_THROW(pass.verifyPattern(size, readFd));
    std::cerr << "done verify" << std::endl;
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

TEST(pattern, shortReadWritePass)
{
    std::string testFileName = "testfile_shortRead";

    uint64_t size = 4096;
    size_t shortSize = 128;
    Pattern pass(testFileName);
    auto shortData_1 = std::vector<std::byte>(shortSize);
    auto shortData_2 = std::vector<std::byte>(shortSize);
    auto restOfData = std::vector<std::byte>(size - shortSize * 2);
    stdplus::fd::FdMock mock;

    // test write pattern with short blocks
    EXPECT_CALL(mock, write(std::span<std::byte> x))
        .WillOnce([this](std::span<std::byte> x){
        std::copy_n(x.begin(), shortSize, shortData_1.begin());
        Return(shortData_1);
        };
        .WillOnce([this](std::span<std::byte> x){
        std::copy_1(x.begin(), restOfData.size(), restOfData.begin());
        Return(restOfData);
        });
        .WillOnce([this](std::span<std::byte> x){
        std::copy_n(x.begin(), shortSize, shortData_2.begin());
        Return(shortData_2);
        });

    EXPECT_NO_THROW(pass.writePattern(size, mock));

    // test read pattern with short blocks
    EXPECT_CALL(mock, read(_))
        .WillOnce(Return(shortData_1))
        .WillOnce(Return(restOfData))
        .WillOnce(Return(shortData_2));

    EXPECT_NO_THROW(pass.verifyPattern(size, mock));
}
/*
TEST(pattern, shortReadFail)
{
    std::string testFileName = "testfile_shortRead";

    uint64_t size = 4096;
    size_t shortSize = 128;
    Pattern tryPattern(testFileName);
    auto shortData = std::vector<std::byte>(shortSize);
    auto restOfData_1 = std::vector<std::byte>(size - shortSize);
    auto restOfData_2 = std::vector<std::byte>(size - shortSize);
    // open the file and write none zero to it

    stdplus::fd::FdMock mock;

    // test writes
    EXPECT_CALL(mock, write(_))
        .WillOnce(Return(shortData))
        .WillOnce(Return(restOfData_1))
        .WillOnce(Return(restOfData_2)); // return too much data!

    EXPECT_THROW(tryPattern.writePattern(size, mock), InternalFailure);

    // test reads
    EXPECT_CALL(mock, read(_))
        .WillOnce(Return(shortData))
        .WillOnce(Return(restOfData_1))
        .WillOnce(Return(restOfData_2)); // return too much data!

    EXPECT_THROW(tryPattern.verifyPattern(size, mock), InternalFailure);
}
*/
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
