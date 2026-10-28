#include "estoraged_conf.hpp"
#include "estoraged_test.hpp"
#include "util.hpp"
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
    TestTmpDir tmpDir;
    std::string testFileName = (tmpDir.get() / "testfile_pass").string();
    std::ofstream testFile;

    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();
    uint64_t size = 4096;
    Zero pass(testFileName);
    stdplus::fd::ManagedFd write =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite);
    EXPECT_NO_THROW(pass.writeZero(size, write));
    stdplus::fd::ManagedFd read =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite);
    EXPECT_NO_THROW(pass.verifyZero(size, read));
}

/* This test that pattern writes the correct number of bytes even if
 * size of the drive is not divisible by the block size
 */
TEST(Zeros, notDivisible)
{
    TestTmpDir tmpDir;
    std::string testFileName =
        (tmpDir.get() / "testfile_notDivisible").string();
    std::ofstream testFile;

    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    uint64_t size = 4097;
    // 4097 is not divisible by the block size, and we expect no errors

    stdplus::fd::ManagedFd write =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite);
    Zero pass(testFileName);
    EXPECT_NO_THROW(pass.writeZero(size, write));
    stdplus::fd::ManagedFd read =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite);
    EXPECT_NO_THROW(pass.verifyZero(size, read));
}

TEST(Zeros, notZeroStart)
{
    TestTmpDir tmpDir;
    std::string testFileName =
        (tmpDir.get() / "testfile_notZeroStart").string();
    std::ofstream testFile;

    // open the file and write none zero to it
    uint32_t dummyValue = 0x88776655;
    testFile.open(testFileName, std::ios::binary | std::ios::out);
    testFile.write((reinterpret_cast<const char*>(&dummyValue)),
                   sizeof(dummyValue));
    testFile.close();
    uint64_t size = 4096;
    stdplus::fd::ManagedFd readWrite =
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
    TestTmpDir tmpDir;
    std::string testFileName = (tmpDir.get() / "testfile_notZeroEnd").string();
    std::ofstream testFile;

    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    uint64_t size = 4096;
    Zero pass(testFileName);
    uint32_t dummyValue = 88;
    stdplus::fd::ManagedFd write =
        stdplus::fd::open(testFileName, stdplus::fd::OpenAccess::ReadWrite);
    EXPECT_NO_THROW(pass.writeZero(size - sizeof(dummyValue), write));

    // open the file and write none zero to it
    testFile.open(testFileName, std::ios::out);
    testFile << dummyValue;
    testFile.close();

    stdplus::fd::ManagedFd read =
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
    std::span shortDataSpan{shortData};
    std::span restOfDataSpan{restOfData};
    stdplus::fd::FdMock mock;

    // test write zeros with short blocks
    EXPECT_CALL(mock, write(_))
        .WillOnce(Return(shortDataSpan))
        .WillOnce(Return(shortDataSpan))
        .WillOnce(Return(restOfDataSpan))
        .WillOnce(Return(shortDataSpan));

    EXPECT_NO_THROW(pass.writeZero(size, mock));

    // test read zeros with short blocks
    EXPECT_CALL(mock, read(_))
        .WillOnce(Return(shortDataSpan))
        .WillOnce(Return(shortDataSpan))
        .WillOnce(Return(restOfDataSpan))
        .WillOnce(Return(shortDataSpan));

    EXPECT_NO_THROW(pass.verifyZero(size, mock));
}

TEST(Zeros, shortReadWriteFail)
{
    std::string testFileName = "testfile_shortRead";

    uint64_t size = 4096;
    size_t shortSize = 128;
    Zero tryZero(testFileName);
    auto shortData = std::vector<std::byte>(shortSize, std::byte{0});
    auto restOfData =
        std::vector<std::byte>(size - shortSize * 3, std::byte{0});
    std::span shortDataSpan{shortData};
    std::span restOfDataSpan{restOfData};
    // open the file and write none zero to it

    stdplus::fd::FdMock mock;

    // test writes
    EXPECT_CALL(mock, write(_))
        .WillOnce(Return(shortDataSpan))
        .WillOnce(Return(shortDataSpan))
        .WillOnce(Return(restOfDataSpan))
        .WillOnce(Return(restOfDataSpan)); // return too much data!

    EXPECT_THROW(tryZero.writeZero(size, mock), InternalFailure);

    // test reads
    EXPECT_CALL(mock, read(_))
        .WillOnce(Return(shortDataSpan))
        .WillOnce(Return(shortDataSpan))
        .WillOnce(Return(restOfDataSpan))
        .WillOnce(Return(restOfDataSpan)); // return too much data!

    EXPECT_THROW(tryZero.verifyZero(size, mock), InternalFailure);
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
        .Times(33)
        .WillRepeatedly(Return(std::span<std::byte>{}))
        .RetiresOnSaturation();
    EXPECT_THROW(tryZero.writeZero(size, mocks), InternalFailure);
    // test reads
    EXPECT_CALL(mocks, read(_))
        .Times(33)
        .WillRepeatedly(Return(std::span<std::byte>{}))
        .RetiresOnSaturation();
    EXPECT_THROW(tryZero.verifyZero(size, mocks), InternalFailure);
}

/* Overwrites the byte at @p offset of @p path with a nonzero value. */
static void corruptByte(const std::string& path, off_t offset)
{
    stdplus::fd::ManagedFd fd =
        stdplus::fd::open(path, stdplus::fd::OpenAccess::ReadWrite);
    const char value = 0x5a;
    ASSERT_EQ(::pwrite(fd.get(), &value, 1, offset), 1);
    ASSERT_EQ(::fdatasync(fd.get()), 0);
}

/* Write and verify zeros that span multiple I/O blocks, opening the file for
 * direct I/O if supported. Also verify that a nonzero byte is detected, both
 * as the first byte of a block and in the middle of a block.
 */
TEST(Zeros, multiBlockDirect)
{
    TestTmpDir tmpDir;
    std::string testFileName = (tmpDir.get() / "multiBlockDirect").string();
    const uint64_t size = (2 * 1024 * 1024) + (3 * 512);
    std::ofstream testFile;
    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();

    Zero pass(testFileName);
    {
        stdplus::fd::ManagedFd writeFd = estoraged::util::openBlockDeviceDirect(
            testFileName, stdplus::fd::OpenAccess::WriteOnly);
        EXPECT_NO_THROW(pass.writeZero(size, writeFd));
    }
    {
        stdplus::fd::ManagedFd readFd = estoraged::util::openBlockDeviceDirect(
            testFileName, stdplus::fd::OpenAccess::ReadOnly);
        EXPECT_NO_THROW(pass.verifyZero(size, readFd));
    }

    for (const off_t offset : {off_t{1024} * 1024, (off_t{1024} * 1024) + 7})
    {
        {
            stdplus::fd::ManagedFd writeFd =
                estoraged::util::openBlockDeviceDirect(
                    testFileName, stdplus::fd::OpenAccess::WriteOnly);
            EXPECT_NO_THROW(pass.writeZero(size, writeFd));
        }
        corruptByte(testFileName, offset);
        stdplus::fd::ManagedFd readFd = estoraged::util::openBlockDeviceDirect(
            testFileName, stdplus::fd::OpenAccess::ReadOnly);
        EXPECT_THROW(pass.verifyZero(size, readFd), InternalFailure);
    }
}

} // namespace estoraged_test
