#include "estoraged_conf.hpp"
#include "sanitize.hpp"

#include <stdarg.h>

#include <stdplus/fd/managed.hpp>

#include <chrono>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

using stdplus::fd::ManagedFd;

class MockSanitize : public Sanitize
{

  public:
    MockSanitize(std::string_view inDevPath) : Sanitize(inDevPath)
    {}
    MOCK_METHOD(int, wrapperIOCTL,
                (ManagedFd & fd, unsigned long request,
                 struct mmc_ioc_cmd idata));
};

TEST(sanitizeGood, max)
{
    std::string devNull = "/dev/null";
    MockSanitize goodSanitize(devNull);
    EXPECT_CALL(goodSanitize, wrapperIOCTL(_, _, _)).WillRepeatedly(Return(0));
    goodSanitize.doSanitize();
}
