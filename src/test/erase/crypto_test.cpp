
#include "cryptErase.hpp"
#include "cryptsetupInterface.hpp"
#include "estoraged.hpp"
#include "estoraged_test.hpp"

#include <unistd.h>

#include <xyz/openbmc_project/Common/error.hpp>

#include <exception>
#include <filesystem>
#include <fstream>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace estoraged_test
{

using estoraged::CryptErase;
using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;
using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

class CryptoEraseTest : public testing::Test
{
  public:
    static constexpr char testFileName[] = "testfile";
    std::ofstream testFile;

    void SetUp() override
    {
        /* Create an empty file that we'll pretend is a 'storage device'. */
        testFile.open(testFileName,
                      std::ios::out | std::ios::binary | std::ios::trunc);
        testFile.close();
        if (testFile.fail())
        {
            throw std::runtime_error("Failed to open test file");
        }
        testFile.close();
    }
};

TEST_F(CryptoEraseTest, EraseCryptPass)
{
    std::unique_ptr<MockCryptsetupInterface> mockCryptIface =
        std::make_unique<MockCryptsetupInterface>();

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, StrEq(CRYPT_LUKS2), nullptr))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotMax(StrEq(CRYPT_LUKS2)))
        .WillOnce(Return(1));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotStatus(_, 0))
        .WillOnce(Return(CRYPT_SLOT_ACTIVE_LAST));

    EXPECT_CALL(*mockCryptIface, cryptKeyslotDestroy(_, 0)).Times(1);

    CryptErase myCryptErase =
        CryptErase(testFileName, std::move(mockCryptIface));
    EXPECT_NO_THROW(myCryptErase.doErase());
}

TEST_F(CryptoEraseTest, EraseCrypMaxSlotFails)
{
    std::unique_ptr<MockCryptsetupInterface> mockCryptIface =
        std::make_unique<MockCryptsetupInterface>();

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, StrEq(CRYPT_LUKS2), nullptr))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotMax(StrEq(CRYPT_LUKS2)))
        .WillOnce(Return(-1));

    CryptErase myCryptErase =
        CryptErase(testFileName, std::move(mockCryptIface));
    EXPECT_THROW(myCryptErase.doErase(), ResourceNotFound);
}

TEST_F(CryptoEraseTest, EraseCrypMaxSlotZero)
{
    std::unique_ptr<MockCryptsetupInterface> mockCryptIface =
        std::make_unique<MockCryptsetupInterface>();

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, StrEq(CRYPT_LUKS2), nullptr))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotMax(StrEq(CRYPT_LUKS2)))
        .WillOnce(Return(0));

    CryptErase myCryptErase =
        CryptErase(testFileName, std::move(mockCryptIface));
    EXPECT_THROW(myCryptErase.doErase(), ResourceNotFound);
}

TEST_F(CryptoEraseTest, EraseCrypOnlyInvalid)
{
    std::unique_ptr<MockCryptsetupInterface> mockCryptIface =
        std::make_unique<MockCryptsetupInterface>();

    EXPECT_CALL(*mockCryptIface, cryptLoad(_, StrEq(CRYPT_LUKS2), nullptr))
        .WillOnce(Return(0));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotMax(StrEq(CRYPT_LUKS2)))
        .WillOnce(Return(32));

    EXPECT_CALL(*mockCryptIface, cryptKeySlotStatus(_, _))
        .WillRepeatedly(Return(CRYPT_SLOT_INVALID));

    CryptErase myCryptErase =
        CryptErase(testFileName, std::move(mockCryptIface));
    EXPECT_NO_THROW(myCryptErase.doErase());
}

TEST_F(CryptoEraseTest, EraseCrypDestoryFails)
{
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

    CryptErase myCryptErase =
        CryptErase(testFileName, std::move(mockCryptIface));
    EXPECT_THROW(myCryptErase.doErase(), InternalFailure);
}

} // namespace estoraged_test
