#include "pattern.hpp"

#include "erase.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <iostream>
#include <random>
#include <span>
#include <string>
#include <vector>

#define SEED 0x6a656272
#define BLOCK_SIZE 4096u

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using stdplus::fd::ManagedFd;

void Pattern::writePattern(const uint64_t driveSize, ManagedFd& fd)
{
    // static seed defines a fixed prng sequnce so it can be verified later,
    // and validated for entropy
    uint64_t currentIndex = 0;
    std::minstd_rand0 generator(SEED);
    std::byte* randArr = new std::byte[BLOCK_SIZE];
    while (currentIndex < driveSize)
    {
        // generate a 4k block of prng
        uint32_t* randArrFill = (uint32_t*)randArr;
        for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t); i++)
        {
            randArrFill[i] = generator();
        }
        /* if we can write all 4k bytes do that, else write the remainder*/
        size_t spanSize = currentIndex + BLOCK_SIZE < driveSize
                              ? BLOCK_SIZE
                              : driveSize - currentIndex;
        if (fd.write(std::span{randArr, spanSize}).size() != spanSize)
        {
            lg2::error("Estoraged erase pattern unable to write sizeof(long)",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"),
                       "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));

            delete[] randArr;
            throw InternalFailure();
        }
        currentIndex = currentIndex + spanSize;
    }
    delete[] randArr;
}

void Pattern::verifyPattern(const uint64_t driveSize, ManagedFd& fd)
{

    uint64_t currentIndex = 0;
    std::minstd_rand0 generator(SEED);
    std::byte* randArr = new std::byte[BLOCK_SIZE];
    std::byte* readArr = new std::byte[BLOCK_SIZE];
    while (currentIndex < driveSize)
    {
        size_t readSize = currentIndex + BLOCK_SIZE < driveSize
                              ? BLOCK_SIZE
                              : driveSize - currentIndex;
        try
        {
            uint32_t* randArrFill = (uint32_t*)randArr;
            for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t); i++)
            {
                randArrFill[i] = generator();
            }
            fd.read(stdplus::span<std::byte>{readArr, readSize});
        }
        catch (...)
        {
            delete[] readArr;
            delete[] randArr;
            lg2::error("Estoraged erase pattern unable to read",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"),
                       "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
            throw InternalFailure();
        }
        std::span<std::byte> readSpan{readArr, readSize};
        std::span<std::byte> randSpan{randArr, readSize};

        if (!std::equal(randSpan.begin(), randSpan.begin() + readSize,
                        readSpan.begin(), readSpan.end()))
        {
            delete[] readArr;
            delete[] randArr;
            lg2::error("Estoraged erase pattern does not match",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"));
            throw InternalFailure();
        }
        currentIndex = currentIndex + readSize;
    }
    delete[] readArr;
    delete[] randArr;
}
