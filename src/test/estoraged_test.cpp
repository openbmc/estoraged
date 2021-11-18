
#include "estoraged.hpp"

#include <unistd.h>

#include <sdbusplus/bus.hpp>
#include <sdbusplus/test/sdbus_mock.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

#include <exception>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EncryptionError;
using ::testing::_;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::StrEq;

/*
 * This sdbus mock object gets used in the destructor of one of the parent
 * classes for the estoraged object, so this can't be part of the eStoragedTest
 * class.
 */
sdbusplus::SdBusMock sdbusMock;

class eStoragedTest : public testing::Test
{
  public:
    static constexpr char testFileName[] = "testfile";
    static constexpr char testLuksDevName[] = "testfile_luksDev";
    std::ofstream testFile;
    std::unique_ptr<estoraged::eStoraged> esObject;
    static constexpr auto TEST_PATH = "/test/openbmc_project/storage/test_dev";
    static constexpr auto ESTORAGED_INTERFACE = "xyz.openbmc_project.eStoraged";
    sdbusplus::bus::bus bus;
    std::string passwordString;
    std::vector<uint8_t> password;

    eStoragedTest() :
        bus(sdbusplus::get_mocked_new(&sdbusMock)), passwordString("password"),
        password(passwordString.begin(), passwordString.end())
    {}

    void SetUp() override
    {

        /* Create a file for testing */
        testFile.open(testFileName,
                      std::ios::out | std::ios::binary | std::ios::trunc);
        if (testFile.fail())
        {
            testFile.close();
            throw std::runtime_error("Failed to open test file");
        }

        /* Write 50 MB of zeros to the file. */
        for (int i = 0; i < 50 * 1024L * 1024L; ++i)
        {
            char zero = 0;
            testFile.write(&zero, sizeof(char));
        }

        EXPECT_CALL(sdbusMock,
                    sd_bus_add_object_vtable(IsNull(), _, StrEq(TEST_PATH),
                                             StrEq(ESTORAGED_INTERFACE), _, _))
            .WillRepeatedly(Return(0));

        EXPECT_CALL(sdbusMock,
                    sd_bus_emit_object_added(IsNull(), StrEq(TEST_PATH)))
            .WillRepeatedly(Return(0));

        EXPECT_CALL(sdbusMock,
                    sd_bus_emit_object_removed(IsNull(), StrEq(TEST_PATH)))
            .WillRepeatedly(Return(0));

        esObject = std::make_unique<estoraged::eStoraged>(
            bus, TEST_PATH, std::string(testFileName),
            std::string(testLuksDevName));

        EXPECT_CALL(sdbusMock, sd_bus_emit_properties_changed_strv(
                                   IsNull(), StrEq(TEST_PATH),
                                   StrEq(ESTORAGED_INTERFACE), _))
            .WillRepeatedly(Return(0));

        /* Format the encrypted device. */
        esObject->format(password);
        EXPECT_FALSE(esObject->is_locked());
    }

    void TearDown() override
    {
        esObject->lock(password);
        EXPECT_TRUE(esObject->is_locked());

        testFile.close();
        EXPECT_EQ(0, unlink(testFileName));
        std::cerr << "Test done" << std::endl;
    }
};

/* Simple test case to format the LUKS encrypted device and then lock it. */
TEST_F(eStoragedTest, FormatPass)
{}

/* Test case to lock and unlock a LUKS encrypted device. */
TEST_F(eStoragedTest, LockUnlockPass)
{
    /* Lock the device */
    esObject->lock(password);
    EXPECT_TRUE(esObject->is_locked());

    /* Unlock the device */
    esObject->unlock(password);
    EXPECT_FALSE(esObject->is_locked());
}

/* Test case to attempt unlocking with the wrong password. */
TEST_F(eStoragedTest, LockUnlockFail)
{
    /* Lock the device */
    esObject->lock(password);
    EXPECT_TRUE(esObject->is_locked());

    /* Attempt to unlock with the wrong password. */
    std::string wrongPassword("wrongPassword");
    EXPECT_THROW(esObject->unlock(std::vector<uint8_t>(wrongPassword.begin(),
                                                       wrongPassword.end())),
                 EncryptionError);

    /* It should still be locked. */
    EXPECT_TRUE(esObject->is_locked());

    /* Unlock the device with the correct password. */
    esObject->unlock(password);
    EXPECT_FALSE(esObject->is_locked());
}

/* Test case to write a file to the LUKS device and then read it back. */
TEST_F(eStoragedTest, WriteFilePass)
{
    /* Write a file to the device. */
    std::string filepath(esObject->getMountPoint());
    filepath.append("/hello.txt");
    std::ofstream helloTestFile(filepath.c_str(),
                                std::ios::out | std::ios::trunc);

    std::string fileContents("Hello World!");
    helloTestFile.write(fileContents.c_str(), fileContents.size());
    helloTestFile.close();

    /* Lock the device */
    esObject->lock(password);
    EXPECT_TRUE(esObject->is_locked());

    /*
     * Now unlock the device and make sure we can read back the file we just
     * wrote.
     */
    esObject->unlock(password);
    EXPECT_FALSE(esObject->is_locked());

    std::ifstream helloFileIn(filepath.c_str());
    std::string readContents((std::istreambuf_iterator<char>(helloFileIn)),
                             std::istreambuf_iterator<char>());
    helloFileIn.close();
    EXPECT_EQ(fileContents, readContents);
}

/*
 * Test case to ensure that reformatting the device will remove any existing
 * files on it.
 */
TEST_F(eStoragedTest, WriteFileFail)
{
    /* Write a file to the device. */
    std::string filepath(esObject->getMountPoint());
    filepath.append("/hello.txt");
    std::ofstream helloTestFile(filepath.c_str(),
                                std::ios::out | std::ios::trunc);

    std::string fileContents("Hello World!");
    helloTestFile.write(fileContents.c_str(), fileContents.size());
    helloTestFile.close();

    /* Lock the device */
    esObject->lock(password);
    EXPECT_TRUE(esObject->is_locked());

    /* Now reformat the device. */
    esObject->format(password);
    EXPECT_FALSE(esObject->is_locked());

    /*
     * Attempt to read back the hello.txt file. The file shouldn't exist
     * anymore.
     */
    std::ifstream helloFileIn(filepath.c_str());
    EXPECT_FALSE(helloFileIn.is_open());
    EXPECT_TRUE(helloFileIn.fail());
}

/*
 * Test case to ensure that reformatting the device with a new password will
 * make the old password unusable.
 */
TEST_F(eStoragedTest, ReformatPass)
{
    /* Lock the device */
    esObject->lock(password);
    EXPECT_TRUE(esObject->is_locked());

    std::string newPasswordString("newPassword");
    std::vector<uint8_t> newPassword(newPasswordString.begin(),
                                     newPasswordString.end());

    /* Reformat with a new password */
    esObject->format(newPassword);
    EXPECT_FALSE(esObject->is_locked());

    /* Lock the device */
    esObject->lock(newPassword);
    EXPECT_TRUE(esObject->is_locked());

    /* Attempt to unlock with the old password. */
    EXPECT_THROW(esObject->unlock(password), EncryptionError);
    EXPECT_TRUE(esObject->is_locked());

    /* Unlock with the new password */
    esObject->unlock(newPassword);
    EXPECT_FALSE(esObject->is_locked());
}
