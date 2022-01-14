
#include "cryptErase.hpp"
#include "cryptsetupInterface.hpp"
#include "estoraged.hpp"
#include "estoraged_test.hpp"
#include "filesystemInterface.hpp"

#include <unistd.h>

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdbusplus/test/sdbus_mock.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Inventory/Item/Volume/client.hpp>
#include <xyz/openbmc_project/Inventory/Item/Volume/server.hpp>

#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace estoraged_test
{

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;
using sdbusplus::xyz::openbmc_project::Inventory::Item::server::Volume;
using std::filesystem::path;
using ::testing::_;
using ::testing::ContainsRegex;
using ::testing::Ge;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::StrEq;

/* Crypto erase uses the cryptIface so it requires use of the mockCryptIface */
/* if mockCryptIface can be factored out of this file, cryptoErase can be teseed
 * in a seperate file */

TEST_F(eStoragedTest, EraseCrypPass)
{
    EXPECT_CALL(*mockCryptIface, cryptLoad(_, Ge(CRYPT_LUKS2), nullptr))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotMax(Ge(CRYPT_LUKS2)))
        .WillOnce(Return(1));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotStatus(_, 0))
        .WillOnce(Return(CRYPT_SLOT_ACTIVE_LAST));

    EXPECT_CALL(*mockCryptIface, cryptKeyslotDestroy(_, 0)).Times(1);

    EXPECT_NO_THROW(esObject->erase(Volume::EraseMethod::CryptoErase));
}

TEST_F(eStoragedTest, EraseCrypMaxSlotFails)
{
    EXPECT_CALL(*mockCryptIface, cryptLoad(_, Ge(CRYPT_LUKS2), nullptr))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotMax(Ge(CRYPT_LUKS2)))
        .WillOnce(Return(0));

    EXPECT_THROW(esObject->erase(Volume::EraseMethod::CryptoErase),
                 ResourceNotFound);
}

TEST_F(eStoragedTest, EraseCrypOnlyInvalid)
{
    EXPECT_CALL(*mockCryptIface, cryptLoad(_, Ge(CRYPT_LUKS2), nullptr))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotMax(Ge(CRYPT_LUKS2)))
        .WillOnce(Return(1));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotStatus(_, 0))
        .WillOnce(Return(CRYPT_SLOT_INVALID));

    EXPECT_NO_THROW(esObject->erase(Volume::EraseMethod::CryptoErase));
}

TEST_F(eStoragedTest, EraseCrypDestoryFails)
{
    EXPECT_CALL(*mockCryptIface, cryptLoad(_, Ge(CRYPT_LUKS2), nullptr))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotMax(Ge(CRYPT_LUKS2)))
        .WillOnce(Return(1));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotStatus(_, 0))
        .WillOnce(Return(CRYPT_SLOT_ACTIVE));

    EXPECT_CALL(*mockCryptIface, cryptKeyslotDestroy(_, 0))
        .WillOnce(Return(-1));

    EXPECT_THROW(esObject->erase(Volume::EraseMethod::CryptoErase),
                 InternalFailure);
}

} // namespace estoraged_test
