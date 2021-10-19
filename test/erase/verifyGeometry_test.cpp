#include "estoraged_conf.hpp"
#include "verifyGeometry.hpp"

#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>


TEST(VerifyGeometry, max)
{
  verifyGeometry maxVerify("");
  ASSERT_EQ(-1, maxVerify.GeometryOkay( [](std::string, unsigned long &bytes)
  {
     bytes = ERASE_MAX_GEOMETRY+1;
     return 0;
  }));
}

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
