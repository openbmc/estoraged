#include "estoraged_conf.hpp"
#include "verifyDriveGeometry.hpp"

#include <xyz/openbmc_project/eStoraged/error.hpp>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;

TEST(VerifyGeometry, TooBigFail)
{
    VerifyDriveGeometry maxVerify("");
    EXPECT_THROW(maxVerify.geometryOkay(ERASE_MAX_GEOMETRY + 1), EraseError);
}

TEST(VerifyGeometry, TooSmallFail)
{
    VerifyDriveGeometry minVerify("");
    EXPECT_THROW(minVerify.geometryOkay(ERASE_MIN_GEOMETRY - 1), EraseError);
}

TEST(VerifyGeometry, pass)
{
    VerifyDriveGeometry passVerify("");
    EXPECT_NO_THROW(passVerify.geometryOkay(ERASE_MIN_GEOMETRY + 1));
}
