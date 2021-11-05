#include "estoraged_conf.hpp"
#include "sanitize.hpp"

#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <chrono>




TEST(sanitizeGood, max)
{
  std::chrono::seconds delay(1);
  sanitize goodSantize("/dev/null", delay, [](int fd, unsigned long request, struct mmc_ioc_cmd data){
    ASSERT_EQ(request, 179); //linux MMC_BLOCK MAJOR
    ASSERT_EQ(data,0);
  });
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
