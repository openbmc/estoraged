#include "pattern.hpp"

#include "erase.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <array>
#include <iostream>
#include <random>
#include <span>
#include <string>

constexpr uint32_t Seed = 0x6a656272
constexpr size_t BlockSize = 4096;
constexpr size_t blockSizeUsing32 = BlockSize / sizeof(uint32_t);

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using stdplus::fd::ManagedFd;

void Pattern::writePattern(const uint64_t driveSize, ManagedFd& fd)
{
    // static seed defines a fixed prng sequnce so it can be verified later,
    // and validated for entropy
    uint64_t currentIndex = 0;
    std::minstd_rand0 generator(Seed);
    std::array<std::byte, BlockSize> randArr;
    while (currentIndex < driveSize)
    {
        // generate a 4k block of prng
        std::array<uint32_t, blockSizeUsing32>* randArrFill =
            reinterpret_cast<std::array<uint32_t, blockSizeUsing32>*>(&randArr);
        for (uint32_t i = 0; i < blockSizeUsing32; i++)
        {
            (*randArrFill)[i] = generator();
        }
        // if we can write all 4k bytes do that, else write the remainder
        size_t writeSize = currentIndex + BlockSize < driveSize
                               ? BlockSize
                               : driveSize - currentIndex;
        if (fd.write({randArr.data(), writeSize}).size() != writeSize)
        {
            lg2::error("Estoraged erase pattern unable to write sizeof(long)",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"),
                       "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
            throw InternalFailure();
        }
        currentIndex = currentIndex + writeSize;
    }
}

void Pattern::verifyPattern(const uint64_t driveSize, ManagedFd& fd)
{

    uint64_t currentIndex = 0;
    std::minstd_rand0 generator(Seed);
    std::array<std::byte, BlockSize> randArr;
    std::array<std::byte, BlockSize> readArr;
    while (currentIndex < driveSize)
    {
        size_t readSize = currentIndex + BlockSize < driveSize
                              ? BlockSize
                              : driveSize - currentIndex;
        try
        {
            std::array<uint32_t, blockSizeUsing32>* randArrFill =
                reinterpret_cast<std::array<uint32_t, blockSizeUsing32>*>(
                    &randArr);
            for (uint32_t i = 0; i < blockSizeUsing32; i++)
            {
                (*randArrFill)[i] = generator();
            }
            fd.read({readArr.data(), readSize});
        }
        catch (...)
        {
            lg2::error("Estoraged erase pattern unable to read",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"),
                       "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
            throw InternalFailure();
        }

        if (!std::equal(randArr.begin(), randArr.begin() + readSize,
                        readArr.begin(), readArr.begin() + readSize))
        {
            lg2::error("Estoraged erase pattern does not match",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"));
            throw InternalFailure();
        }
        currentIndex = currentIndex + readSize;
    }
}
