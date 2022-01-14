
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

using estoraged::CryptErase;
using estoraged::Cryptsetup;
using estoraged::CryptsetupInterface;
using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;
using sdbusplus::xyz::openbmc_project::Inventory::Item::server::Volume;
using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

TEST(cryptoErase, EraseCrypPass)
{
    /* Create an empty file that we'll pretend is a 'storage device'. */
    std::string testFileName = "testfile";
    std::ofstream testFile;

    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();
    if (testFile.fail())
    {
        throw std::runtime_error("Failed to open test file");
    }

    std::unique_ptr<MockCryptsetupInterface> mockCryptIface =
        std::make_unique<MockCryptsetupInterface>();

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, StrEq(CRYPT_LUKS2), nullptr))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotMax(StrEq(CRYPT_LUKS2)))
        .WillOnce(Return(1));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotStatus(_, 0))
        .WillOnce(Return(CRYPT_SLOT_ACTIVE_LAST));

    EXPECT_CALL(*mockCryptIface, cryptKeyslotDestroy(_, 0)).Times(1);

    CryptErase myCryptErase = CryptErase(testFileName);
    EXPECT_NO_THROW(myCryptErase.doErase(std::move(mockCryptIface)));
}
TEST(cryptoErase, EraseCrypMaxSlotFails)
{
    /* Create an empty file that we'll pretend is a 'storage device'. */
    std::string testFileName = "testfile";
    std::ofstream testFile;

    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();
    if (testFile.fail())
    {
        throw std::runtime_error("Failed to open test file");
    }

    std::unique_ptr<MockCryptsetupInterface> mockCryptIface =
        std::make_unique<MockCryptsetupInterface>();

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, StrEq(CRYPT_LUKS2), nullptr))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotMax(StrEq(CRYPT_LUKS2)))
        .WillOnce(Return(0));

    CryptErase myCryptErase = CryptErase(testFileName);
    EXPECT_THROW(myCryptErase.doErase(std::move(mockCryptIface)),
                 ResourceNotFound);
}

TEST(cryptoErase, EraseCrypOnlyInvalid)
{
    /* Create an empty file that we'll pretend is a 'storage device'. */
    std::string testFileName = "testfile";
    std::ofstream testFile;

    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();
    if (testFile.fail())
    {
        throw std::runtime_error("Failed to open test file");
    }

    std::unique_ptr<MockCryptsetupInterface> mockCryptIface =
        std::make_unique<MockCryptsetupInterface>();

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, StrEq(CRYPT_LUKS2), nullptr))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotMax(StrEq(CRYPT_LUKS2)))
        .WillOnce(Return(32));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotStatus(_, _))
        .WillRepeatedly(Return(CRYPT_SLOT_INVALID));

    CryptErase myCryptErase = CryptErase(testFileName);
    EXPECT_NO_THROW(myCryptErase.doErase(std::move(mockCryptIface)));
}

TEST(cryptoErase, EraseCrypDestoryFails)
{
    /* Create an empty file that we'll pretend is a 'storage device'. */
    std::string testFileName = "testfile";
    std::ofstream testFile;

    testFile.open(testFileName,
                  std::ios::out | std::ios::binary | std::ios::trunc);
    testFile.close();
    if (testFile.fail())
    {
        throw std::runtime_error("Failed to open test file");
    }

    std::unique_ptr<MockCryptsetupInterface> mockCryptIface =
        std::make_unique<MockCryptsetupInterface>();

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, StrEq(CRYPT_LUKS2), nullptr))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotMax(StrEq(CRYPT_LUKS2)))
        .WillOnce(Return(1));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotStatus(_, 0))
        .WillOnce(Return(CRYPT_SLOT_ACTIVE));

    EXPECT_CALL(*mockCryptIface, cryptKeyslotDestroy(_, 0))
        .WillOnce(Return(-1));

    CryptErase myCryptErase = CryptErase(testFileName);
    EXPECT_THROW(myCryptErase.doErase(std::move(mockCryptIface)),
                 InternalFailure);
}
} // namespace estoraged_test
