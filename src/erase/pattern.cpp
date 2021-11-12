#include "pattern.hpp"

#include "erase.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

#include <iostream>
#include <random>
#include <span>
#include <string>
#include <vector>

#define SEED 0x6a656272

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;
using stdplus::fd::ManagedFd;

int Pattern::writePattern(const uint64_t driveSize, ManagedFd& fd)
{
    // static seed defines a fixed prng sequnce so it can be verified later,
    // and validated for entropy
    unsigned long currentIndex = 0;
    std::minstd_rand0 generator(SEED);
    while (currentIndex < driveSize)
    {
        uint64_t rand = generator();
        std::byte* randByte = reinterpret_cast<std::byte*>(&rand);
        // if we can write all 8 bytes
        size_t spanSize = currentIndex + sizeof(rand) < driveSize
                              ? sizeof(rand)
                              : driveSize - currentIndex;
        std::span<std::byte> randSpan(randByte, spanSize);
        if (fd.write(randSpan).size() != randSpan.size())
        {
            lg2::error("Estoraged erase pattern unable to write sizeof(long)",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"),
                       "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
            throw EraseError();
        }
        currentIndex = currentIndex + sizeof(uint64_t);
    }
    return 0;
}

int Pattern::verifyPattern(const uint64_t driveSize, ManagedFd& fd)
{

    unsigned long currentIndex = 0;
    unsigned long seed = SEED;
    std::minstd_rand0 generator(seed);

    while (currentIndex < driveSize)
    {
        uint64_t rand;
        size_t readSize = currentIndex + sizeof(rand) < driveSize
                              ? sizeof(rand)
                              : driveSize - currentIndex;
        std::vector<std::byte> readVect(readSize);
        try
        {
            rand = generator();
            fd.read(readVect);
        }
        catch (...)
        {
            lg2::error("Estoraged erase pattern unable to read",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"),
                       "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
            throw EraseError();
        }
        std::span<std::byte> readSpan{readVect};
        std::byte* randByte = reinterpret_cast<std::byte*>(&rand);
        std::span<std::byte> randSpan(randByte, sizeof(rand));
        if (!spanEq(randSpan, readSpan, readSpan.size()))
        {
            lg2::error("Estoraged erase pattern does not match",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"));
            throw EraseError();
        }
        currentIndex = currentIndex + sizeof(rand);
    }
    return 0;
}

// helper for comparing two spans of bytes
bool Pattern::spanEq(std::span<std::byte>& firstSpan,
                     std::span<std::byte>& secondSpan, std::size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        if (firstSpan[i] != secondSpan[i])
        {
            return false;
        }
    }
    return true;
}
