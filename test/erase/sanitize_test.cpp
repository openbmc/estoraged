#include "estoraged_conf.hpp"
#include "sanitize.hpp"

#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <chrono>
#include <stdarg.h>



int mock_ioctl(int fd, unsigned long request, struct mmc_ioc_cmd data)
{
//    va_list ap;
//    va_start(ap, 0);
//    struct mmc_ioc_cmd data = va_arg(ap, struct mmc_ioc_cmd);
//    va_end(ap);

    EXPECT_EQ(request, 18); //179); //linux MMC_BLOCK MAJOR
//    ASSERT_EQ(data,0);
    return 0;
}

TEST(sanitizeGood, max)
{
  unsigned int delay = 1;
  std::string devNull = "/dev/null";
  // sanitize goodSantize(devNull, delay, int mock_ioctl(int, unsigned long, ...));
  std::function<int(int, unsigned long, struct mmc_ioc_cmd)> mock = mock_ioctl;
  sanitize goodSantize(devNull, delay, mock);
}
/*
TEST(VerifyGeometry, min)
{
  verifyGeometry maxVerify("");
  ASSERT_EQ(-1, maxVerify.GeometryOkay( [](std::string, unsigned long &bytes)
  {
     bytes = ERASE_MIN_GEOMETRY-1;
     return 0;
  }));
}

TEST(VerifyGeometry, pass)
{
  verifyGeometry maxVerify("");
  ASSERT_EQ(0, maxVerify.GeometryOkay( [](std::string, unsigned long &bytes)
  {
     bytes = ERASE_MIN_GEOMETRY+1;
     return 0;
  }));
}
*/
