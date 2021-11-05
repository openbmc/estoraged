#include "estoraged_conf.hpp"
#include "sanitize.hpp"

#include <stdarg.h>

#include <chrono>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

class mockSanitize : public sanitize
{

  public:
    mockSanitize(std::string inDevPath, unsigned int inDelay) :
        sanitize(inDevPath, inDelay)
    {}
    MOCK_METHOD(int, wrapperIOCTL,
                (int fd, unsigned long request, struct mmc_ioc_cmd idata));
};

TEST(sanitizeGood, max)
{
    unsigned int delay = 1;
    std::string devNull = "/dev/null";
    mockSanitize goodSantize(devNull, delay);
}
