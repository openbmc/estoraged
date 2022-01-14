#include "estoraged_conf.hpp"
#include "verifyDriveGeometry.hpp"

#include <xyz/openbmc_project/Common/error.hpp>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace estoraged_test
{
using estoraged::VerifyDriveGeometry;
using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

TEST(VerifyGeometry, TooBigFail)
{
    VerifyDriveGeometry maxVerify("");
    EXPECT_THROW(maxVerify.geometryOkay(ERASE_MAX_GEOMETRY + 1),
                 InternalFailure);
}

TEST(VerifyGeometry, TooSmallFail)
{
    VerifyDriveGeometry minVerify("");
    EXPECT_THROW(minVerify.geometryOkay(ERASE_MIN_GEOMETRY - 1),
                 InternalFailure);
}

TEST(VerifyGeometry, pass)
{
    VerifyDriveGeometry passVerify("");
    EXPECT_NO_THROW(passVerify.geometryOkay(ERASE_MIN_GEOMETRY + 1));
}

} // namespace estoraged_test
