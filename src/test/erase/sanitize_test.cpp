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

class IOCTLWrapperMock : public estoraged::IOCTLWrapperInterface
{
  public:
    MOCK_METHOD(int, doIoctl,
                (std::string_view devPath, unsigned long request,
                 struct mmc_ioc_cmd& idata),
                (override));

    MOCK_METHOD(int, doIoctlMulti,
                (std::string_view devPath, unsigned long request,
                 struct estoraged::MmcIoMultiCmdErase& data),
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
    uint32_t eraseEndArg = 0;
    EXPECT_CALL(*mockPtr, doIoctlMulti(_, _, _))
        .WillRepeatedly(
            [&eraseEndArg](std::string_view, unsigned long,
                           struct estoraged::MmcIoMultiCmdErase& data) {
                eraseEndArg = data.cmds[1].arg;
                return 0;
            });
    EXPECT_NO_THROW(goodSanitize.doSanitize(52428800));
    EXPECT_EQ(eraseEndArg, 52428800U - 1U);

    EXPECT_NO_THROW(goodSanitize.doSanitize(0x80000000));
    EXPECT_EQ(eraseEndArg, 0x80000000U - 1U);

    EXPECT_NO_THROW(goodSanitize.doSanitize(4000000000));
    EXPECT_EQ(eraseEndArg, (4000000000U / 512U) - 1U);
}

// doIoctlMulti returns -1, and Sanitize::emmcErase throws InternalFailure
TEST(Sanitize, Sanitize_emmcErase)
{
    std::unique_ptr<IOCTLWrapperMock> mockIOCTL =
        std::make_unique<IOCTLWrapperMock>();
    IOCTLWrapperMock* mockPtr = mockIOCTL.get();
    estoraged::Sanitize emptySanitize("", std::move(mockIOCTL));
    EXPECT_CALL(*mockPtr, doIoctlMulti(_, _, _)).WillRepeatedly(Return(-1));
    EXPECT_THROW(emptySanitize.doSanitize(4000000000), InternalFailure);
}

// mock ioctl returns 1, and emmcSanitize throws
TEST(Sanitize, Sanitize_emmcSanitize)
{
    std::unique_ptr<IOCTLWrapperMock> mockIOCTL =
        std::make_unique<IOCTLWrapperMock>();
    IOCTLWrapperMock* mockPtr = mockIOCTL.get();
    estoraged::Sanitize ioctlSanitize("/dev/null", std::move(mockIOCTL));
    EXPECT_CALL(*mockPtr, doIoctl(_, _, _)).WillRepeatedly(Return(1));
    EXPECT_CALL(*mockPtr, doIoctlMulti(_, _, _)).WillRepeatedly(Return(0));
    EXPECT_THROW(ioctlSanitize.doSanitize(4000000000), InternalFailure);
}

TEST(Sanitize, Sanitize_InvalidDriveSize)
{
    std::unique_ptr<IOCTLWrapperMock> mockIOCTL =
        std::make_unique<IOCTLWrapperMock>();
    estoraged::Sanitize sanitize("/dev/null", std::move(mockIOCTL));
    EXPECT_THROW(sanitize.doSanitize(0), InternalFailure);
    EXPECT_THROW(sanitize.doSanitize(511), InternalFailure);
    EXPECT_THROW(sanitize.doSanitize((uint64_t{1} << 32) * 512 + 512),
                 InternalFailure);
}

} // namespace estoraged_test
