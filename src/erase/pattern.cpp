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

namespace estoraged
{

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using stdplus::fd::Fd;

void Pattern::writePattern(const uint64_t driveSize, Fd& fd)
{
    // static seed defines a fixed prng sequnce so it can be verified later,
    // and validated for entropy
    uint64_t currentIndex = 0;

    // random number generator seeded with a constant value will
    // generate a predictable sequence of values NOLINTNEXTLINE
    std::minstd_rand0 generator(seed);
    std::array<std::byte, blockSize> randArr{};

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
        size_t writeSize = currentIndex + blockSize < driveSize
                               ? blockSize
                               : driveSize - currentIndex;
        size_t written = 0;
        size_t retry = 0;
        while (written < writeSize)
        {
            written += fd.write({randArr.data() + written, writeSize - written})
                           .size();
            if (written > writeSize)
            {
                throw InternalFailure();
            }
            retry++;
            if (retry > maxRetry)
            {
                lg2::error("Unable to do full write", "REDFISH_MESSAGE_ID",
                           std::string("eStorageD.1.0.EraseFailure"));
                throw InternalFailure();
            }
            usleep(delay);
        }
        currentIndex = currentIndex + writeSize;
    }
}

void Pattern::verifyPattern(const uint64_t driveSize, Fd& fd)
{

    uint64_t currentIndex = 0;
    // random number generator seeded with a constant value will
    // generate a predictable sequence of values NOLINTNEXTLINE
    std::minstd_rand0 generator(seed);
    std::array<std::byte, blockSize> randArr{};
    std::array<std::byte, blockSize> readArr{};

    while (currentIndex < driveSize)
    {
        size_t readSize = currentIndex + blockSize < driveSize
                              ? blockSize
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
            size_t read = 0;
            size_t retry = 0;
            while (read < readSize)
            {
                read +=
                    fd.read({readArr.data() + read, readSize - read}).size();
                if (read > readSize)
                {
                    throw InternalFailure();
                }
                retry++;
                if (retry > maxRetry)
                {
                    lg2::error("Unable to do full read", "REDFISH_MESSAGE_ID",
                               std::string("eStorageD.1.0.EraseFailure"));
                    throw InternalFailure();
                }
                usleep(delay);
            }
        }
        catch (...)
        {
            lg2::error("Estoraged erase pattern unable to read",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"));
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

} // namespace estoraged
