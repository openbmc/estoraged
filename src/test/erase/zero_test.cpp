#include "estoraged_conf.hpp"
#include "zero.hpp"

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

using estoraged::Zero;
using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using testing::_;
using testing::Return;

TEST(Zeros, zeroPass)
{
    std::string testFileName = "testfile_pass";
    std::ofstream testFile;

    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();
    uint64_t size = 4096;
    Zero pass(testFileName);
    stdplus::fd::Fd&& write =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite);
    EXPECT_NO_THROW(pass.writeZero(size, write));
    stdplus::fd::Fd&& read =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite);
    EXPECT_NO_THROW(pass.verifyZero(size, read));
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

    stdplus::fd::Fd&& write =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite);
    Zero pass(testFileName);
    EXPECT_NO_THROW(pass.writeZero(size, write));
    stdplus::fd::Fd&& read =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite);
    EXPECT_NO_THROW(pass.verifyZero(size, read));
}

TEST(Zeros, notZeroStart)
{
    std::string testFileName = "testfile_notZeroStart";
    std::ofstream testFile;

    // open the file and write none zero to it
    uint32_t dummyValue = 0x88776655;
    testFile.open(testFileName, std::ios::binary | std::ios::out);
    testFile.write((reinterpret_cast<const char*>(&dummyValue)),
                   sizeof(dummyValue));
    testFile.close();
    uint64_t size = 4096;
    stdplus::fd::Fd&& readWrite =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite);

    Zero pass(testFileName);
    EXPECT_NO_THROW(pass.writeZero(size - sizeof(dummyValue), readWrite));

    // force flush, needed for unit testing
    std::ofstream file;
    file.open(testFileName);
    file.close();

    EXPECT_THROW(pass.verifyZero(size, readWrite), InternalFailure);
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
    uint32_t dummyValue = 88;
    stdplus::fd::Fd&& write =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite);
    EXPECT_NO_THROW(pass.writeZero(size - sizeof(dummyValue), write));

    // open the file and write none zero to it
    testFile.open(testFileName, std::ios::out);
    testFile << dummyValue;
    testFile.close();

    stdplus::fd::Fd&& read =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite);
    EXPECT_THROW(pass.verifyZero(size, read), InternalFailure);
}

TEST(Zeros, shortReadWritePass)
{
    std::string testFileName = "testfile_shortRead";

    uint64_t size = 4096;
    size_t shortSize = 128;
    Zero pass(testFileName);
    auto shortData = std::vector<std::byte>(shortSize, std::byte{0});
    auto restOfData =
        std::vector<std::byte>(size - shortSize * 3, std::byte{0});
    stdplus::fd::FdMock mock;

    // test write zeros with short blocks
    EXPECT_CALL(mock, write(_))
        .WillOnce(Return(shortData))
        .WillOnce(Return(shortData))
        .WillOnce(Return(restOfData))
        .WillOnce(Return(shortData));

    EXPECT_NO_THROW(pass.writeZero(size, mock));

    // test read zeros with short blocks
    EXPECT_CALL(mock, read(_))
        .WillOnce(Return(shortData))
        .WillOnce(Return(shortData))
        .WillOnce(Return(restOfData))
        .WillOnce(Return(shortData));

    EXPECT_NO_THROW(pass.verifyZero(size, mock));
}

TEST(Zeros, shortReadFail)
{
    std::string testFileName = "testfile_shortRead";

    uint64_t size = 4096;
    size_t shortSize = 128;
    Zero tryZero(testFileName);
    auto shortData = std::vector<std::byte>(shortSize, std::byte{0});
    auto restOfData =
        std::vector<std::byte>(size - shortSize * 3, std::byte{0});
    // open the file and write none zero to it

    stdplus::fd::FdMock mock;

    // test writes
    EXPECT_CALL(mock, write(_))
        .WillOnce(Return(shortData))
        .WillOnce(Return(shortData))
        .WillOnce(Return(restOfData))
        .WillOnce(Return(restOfData)); // return too much data!

    EXPECT_THROW(tryZero.writeZero(size, mock), InternalFailure);

    // test reads
    EXPECT_CALL(mock, read(_))
        .WillOnce(Return(shortData))
        .WillOnce(Return(shortData))
        .WillOnce(Return(restOfData))
        .WillOnce(Return(restOfData)); // return too much data!

    EXPECT_THROW(tryZero.verifyZero(size, mock), InternalFailure);
}

// NOLINTNEXTLINE
ACTION(ThrowException)
{
    throw InternalFailure();
}

TEST(Zeros, driveIsSmaller)
{
    std::string testFileName = "testfile_driveIsSmaller";

    uint64_t size = 4096;
    Zero tryZero(testFileName);

    stdplus::fd::FdMock mocks;
    testing::InSequence s;

    // test writes
    EXPECT_CALL(mocks, write(_))
        .Times(100)
        .WillRepeatedly(Return(std::vector<std::byte>{}))
        .RetiresOnSaturation();
    EXPECT_CALL(mocks, write(_)).WillOnce(ThrowException());

    EXPECT_THROW(tryZero.writeZero(size, mocks), InternalFailure);

    // test reads
    EXPECT_CALL(mocks, read(_))
        .Times(100)
        .WillRepeatedly(Return(std::vector<std::byte>{}))
        .RetiresOnSaturation();
    EXPECT_CALL(mocks, read(_)).WillOnce(ThrowException());

    EXPECT_THROW(tryZero.verifyZero(size, mocks), InternalFailure);
}

} // namespace estoraged_test
