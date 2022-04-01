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

constexpr uint32_t seed = 0x6a656272;
constexpr size_t blockSize = 4096;
constexpr size_t blockSizeUsing32 = blockSize / sizeof(uint32_t);

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
        uint32_t written = 0;
        while (written < writeSize)
        {
            written += fd.write({randArr.data() + written, writeSize - written})
                           .size();
            std::cerr << "pattern writting " << written << std::endl;
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
            uint32_t read = 0;
            while (read < readSize)
            {
                read +=
                    fd.read({readArr.data() + read, readSize - read}).size();
                std::cerr << "reading" << std::endl;
            }
        }
        catch (...)
        {
            lg2::error("Estoraged erase pattern unable to read",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"));
            std::cerr << "unable to read" << std::endl;
            throw InternalFailure();
        }

        if (!std::equal(randArr.begin(), randArr.begin() + readSize,
                        readArr.begin(), readArr.begin() + readSize))
        {
            lg2::error("Estoraged erase pattern does not match",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"));
            std::cerr << "don't match" << std::endl;
            for (auto const b : readArr)
            {
                std::cerr << std::to_integer<int>(b);
            }
            std::cerr << std::endl;
            for (auto const b : randArr)
            {
                std::cerr << std::to_integer<int>(b);
            }
            throw InternalFailure();
        }
        currentIndex = currentIndex + readSize;
    }
}

} // namespace estoraged
