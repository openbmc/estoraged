#include "estoraged_conf.hpp"
#include "zero.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <system_error>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using stdplus::fd::ManagedFd;

class MockManagedFd : public ManagedFd
{
  public:
    MockManagedFd(int fd) : ManagedFd(std::move(fd))
    {}
    ~MockManagedFd()
    {}
};

TEST(Zeros, zeroPass)
{
    uint64_t size = 4096;
    int fds[2];
    Zero pass("fileName");
    if (pipe(fds))
    {
        FAIL();
    }
    MockManagedFd writeFd(fds[1]);
    EXPECT_NO_THROW(pass.writeZero(size, writeFd));
    MockManagedFd verifyFd(fds[0]);
    EXPECT_NO_THROW(pass.verifyZero(size, verifyFd));
}

/* This test that pattern writes the correct number of bytes even if
 * size of the drive is not divisable by the block size
 */
TEST(Zeros, notDivisible)
{
    uint64_t size = 4097;
    // 4097 is not divisible by the block size, and we expect no errors
    int fds[2];
    Zero pass("fileName");
    if (pipe(fds))
    {
        FAIL();
    }
    MockManagedFd writeFd(fds[1]);
    EXPECT_NO_THROW(pass.writeZero(size, writeFd));
    MockManagedFd verifyFd(fds[0]);
    EXPECT_NO_THROW(pass.verifyZero(size, verifyFd));
}

TEST(Zeros, notZeroStart)
{
    uint64_t size = 4096;
    int fds[2];
    Zero pass("fileName");
    if (pipe(fds))
    {
        FAIL();
    }
    int dummyValue = 88;
    if (::write(fds[1], &dummyValue, sizeof(dummyValue)) != sizeof(dummyValue))
    {
        FAIL();
    }
    MockManagedFd writeFd(fds[1]);
    EXPECT_NO_THROW(pass.writeZero(size - sizeof(dummyValue), writeFd));
    MockManagedFd verifyFd(fds[0]);
    EXPECT_THROW(pass.verifyZero(size, verifyFd), InternalFailure);
}

TEST(Zeros, notZeroEnd)
{
    uint64_t size = 4096;
    int fds[2];
    Zero pass("fileName");
    if (pipe(fds))
    {
        FAIL();
    }
    int dummyValue = 88;
    MockManagedFd writeFd(fds[1]);
    EXPECT_NO_THROW(pass.writeZero(size - sizeof(dummyValue), writeFd));
    if (::write(fds[1], &dummyValue, sizeof(dummyValue)) != sizeof(dummyValue))
    {
        FAIL();
    }
    MockManagedFd verifyFd(fds[0]);
    EXPECT_THROW(pass.verifyZero(size, verifyFd), InternalFailure);
}
