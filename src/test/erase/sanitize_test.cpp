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

class MockSanitize : public Sanitize
{

  public:
    MockSanitize(std::string_view inDevPath) : Sanitize(inDevPath)
    {}
    MOCK_METHOD(int, wrapperIOCTL,
                (ManagedFd & fd, unsigned long request,
                 struct mmc_ioc_cmd idata),
                (override));
};

// mock ioctl returns 0, and everything passes
TEST(Sanitize, Successful)
{
    constexpr std::string_view devNull = "/dev/null";
    MockSanitize goodSanitize(devNull);
    EXPECT_CALL(goodSanitize, wrapperIOCTL(_, _, _)).WillRepeatedly(Return(0));
    EXPECT_NO_THROW(goodSanitize.doSanitize());
}

// empty string can never be opened, stdplus throws
TEST(Sanitize, FileNotFound)
{
    constexpr std::string_view empty = "";
    MockSanitize emptySanitize(empty);
    EXPECT_THROW(emptySanitize.doSanitize(), std::system_error);
}

// mock ioctl returns 1, and santize throws
TEST(Sanitize, ioctlFailure)
{
    constexpr std::string_view devNull = "/dev/null";
    MockSanitize ioctlSanitize(devNull);
    EXPECT_CALL(ioctlSanitize, wrapperIOCTL(_, _, _)).WillRepeatedly(Return(1));
    EXPECT_THROW(ioctlSanitize.doSanitize(), EraseError);
}
