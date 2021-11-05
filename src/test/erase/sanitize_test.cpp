#include "estoraged_conf.hpp"
#include "sanitize.hpp"

#include <sys/ioctl.h>

#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <string>
#include <string_view>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace estoraged_test
{

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using ::testing::_;
using ::testing::Return;

constexpr uint32_t extCsdEraseGroupDef = 175;
constexpr uint32_t extCsdEraseTimeoutMult = 223;
constexpr uint32_t extCsdHcEraseGrpSize = 224;

class IOCTLWrapperMock : public estoraged::IOCTLWrapperInterface
{
  public:
    MOCK_METHOD(int, doIoctl,
                (std::string_view devPath, unsigned long request,
                 struct mmc_ioc_cmd idata),
                (override));

    MOCK_METHOD(int, doIoctlMulti,
                (std::string_view devPath, unsigned long request,
                 struct estoraged::mmc_io_multi_cmd_erase),
                (override));
};

// mock ioctl returns 0, and everything passes
TEST(Sanitize, Successful)
{
    std::unique_ptr<IOCTLWrapperMock> mockIOCTL =
        std::make_unique<IOCTLWrapperMock>();
    IOCTLWrapperMock* mockPtr = mockIOCTL.get();
    estoraged::Sanitize goodSanitize("/dev/null", std::move(mockIOCTL));
    EXPECT_CALL(*mockPtr, doIoctl(_, _, _)).WillRepeatedly(Return(0));
    EXPECT_CALL(*mockPtr, doIoctlMulti(_, _, _)).WillRepeatedly(Return(0));
    EXPECT_NO_THROW(goodSanitize.doSanitize(52428800));
}

// doIoctlMulti returns -1, and sanitize throws InternalFailure
TEST(Sanitize, FileNotFound)
{
    std::unique_ptr<IOCTLWrapperMock> mockIOCTL =
        std::make_unique<IOCTLWrapperMock>();
    IOCTLWrapperMock* mockPtr = mockIOCTL.get();
    estoraged::Sanitize emptySanitize("", std::move(mockIOCTL));
    EXPECT_CALL(*mockPtr, doIoctlMulti(_, _, _)).WillRepeatedly(Return(-1));
    EXPECT_THROW(emptySanitize.doSanitize(4000000000), InternalFailure);
}

// mock ioctl returns 1, and santize throws
TEST(Sanitize, ioctlFailure)
{
    std::unique_ptr<IOCTLWrapperMock> mockIOCTL =
        std::make_unique<IOCTLWrapperMock>();
    IOCTLWrapperMock* mockPtr = mockIOCTL.get();
    estoraged::Sanitize ioctlSanitize("/dev/null", std::move(mockIOCTL));
    EXPECT_CALL(*mockPtr, doIoctl(_, _, _)).WillRepeatedly(Return(1));
    EXPECT_CALL(*mockPtr, doIoctlMulti(_, _, _)).WillRepeatedly(Return(0));
    EXPECT_THROW(ioctlSanitize.doSanitize(4000000000), InternalFailure);
}

} // namespace estoraged_test
