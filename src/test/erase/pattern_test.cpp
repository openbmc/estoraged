#include "estoraged_conf.hpp"
#include "pattern.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;
using stdplus::fd::ManagedFd;

class MockManagedFd : public ManagedFd
{
  public:
    MockManagedFd(int fd) : ManagedFd(std::move(fd))
    {}
    ~MockManagedFd()
    {}
};

TEST(VerifyGeometry, patternPass)
{
    uint64_t size = 4096;
    int fds[2];
    Pattern pass("fileName");
    if (pipe(fds))
    {
        FAIL();
    }
    MockManagedFd writeFd(fds[1]);
    pass.writePattern(size, writeFd);
    MockManagedFd verifyFd(fds[0]);
    pass.verifyPattern(size, verifyFd);
}

TEST(VerifyGeometry, patternPassNotDivisible)
{
    uint64_t size = 4097;
    int fds[2];
    Pattern pass("fileName");
    if (pipe(fds))
    {
        FAIL();
    }
    MockManagedFd writeFd(fds[1]);
    pass.writePattern(size, writeFd);
    MockManagedFd verifyFd(fds[0]);
    pass.verifyPattern(size, verifyFd);
}

TEST(VerifyGeometry, patternsDontMatch)
{
    uint64_t size = 4096;
    int fds[2];
    Pattern pass("fileName");
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
    pass.writePattern(size - sizeof(dummyValue), writeFd);
    MockManagedFd verifyFd(fds[0]);
    EXPECT_THROW(pass.verifyPattern(size, verifyFd), EraseError);
}
