#include "estoraged_conf.hpp"
#include "sanitize.hpp"

#include <stdarg.h>

#include <chrono>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

class mockSanitize : public sanitize
{

  public:
    mockSanitize(std::string inDevPath) : sanitize(inDevPath)
    {}
    MOCK_METHOD(int, wrapperIOCTL,
                (int fd, unsigned long request, struct mmc_ioc_cmd idata));
};

TEST(sanitizeGood, max)
{
    std::string devNull = "/dev/null";
    mockSanitize goodSanitize(devNull);
    EXPECT_CALL(goodSanitize, wrapperIOCTL(int fd, unsigned long req,
                                           struct mmc_io_cmd idata))
        .WillRepeatedly(Return(0));
    goodSanitize.doSanitize();
}
