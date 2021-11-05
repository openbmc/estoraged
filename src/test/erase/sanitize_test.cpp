#include "estoraged_conf.hpp"
#include "sanitize.hpp"

#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

using stdplus::fd::ManagedFd;

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;

class IOCTLWrapperMock : public IOCTLWrapperInterface
{
  public:
    MOCK_METHOD(int, doIoctl,
                (ManagedFd & fd, unsigned long request,
                 struct mmc_ioc_cmd idata),
                (override));
    ~IOCTLWrapperMock()
    {}
};

// mock ioctl returns 0, and everything passes
TEST(Sanitize, Successful)
{
    std::unique_ptr<IOCTLWrapperMock> mockIOCTL =
        std::make_unique<IOCTLWrapperMock>();
    IOCTLWrapperMock* mockPtr = mockIOCTL.get();
    Sanitize goodSanitize("/dev/null", std::move(mockIOCTL));
    EXPECT_CALL(*mockPtr, doIoctl(_, _, _)).WillRepeatedly(Return(0));
    EXPECT_NO_THROW(goodSanitize.doSanitize());
}

// empty string can never be opened, stdplus throws
TEST(Sanitize, FileNotFound)
{
    std::unique_ptr<IOCTLWrapperMock> mockIOCTL =
        std::make_unique<IOCTLWrapperMock>();
    Sanitize emptySanitize("", std::move(mockIOCTL));
    EXPECT_THROW(emptySanitize.doSanitize(), std::system_error);
}

// mock ioctl returns 1, and santize throws
TEST(Sanitize, ioctlFailure)
{
    std::unique_ptr<IOCTLWrapperMock> mockIOCTL =
        std::make_unique<IOCTLWrapperMock>();
    IOCTLWrapperMock* mockPtr = mockIOCTL.get();
    Sanitize ioctlSanitize("/dev/null", std::move(mockIOCTL));
    EXPECT_CALL(*mockPtr, doIoctl(_, _, _)).WillRepeatedly(Return(1));
    EXPECT_THROW(ioctlSanitize.doSanitize(), EraseError);
}
