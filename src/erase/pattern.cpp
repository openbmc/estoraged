#include "pattern.hpp"

#include "erase.hpp"

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

#include <random>
#include <span>
#include <string>
#include <vector>

#define SEED 0x6a656272

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;
using stdplus::fd::ManagedFd;

int pattern::writePattern(const uint64_t driveSize)
{
    // static seed defines a fixed prng sequnce so it can be verified later,
    // and validated for entropy
    unsigned long currentIndex = 0;
    ManagedFd fd;
    try
    {
        fd = stdplus::fd::open(devPath.c_str(),
                               stdplus::fd::OpenAccess::WriteOnly);
    }
    catch (...)
    {
        lg2::error("erase unable to open blockdev for writing",
                   "REDFISH_MESSAGE_ID",
                   std::string("openbmc.0.1.Erasefailurejjjebr"),
                   "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
        throw EraseError();
    }

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

        if (fd.write(randSpan)[0] != static_cast<std::byte>(sizeof(rand)))
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

int pattern::verifyPattern(const uint64_t driveSize)
{

    ManagedFd fd;
    try
    {
        fd = stdplus::fd::open(devPath, stdplus::fd::OpenAccess::ReadOnly);
    }
    catch (...)
    {
        lg2::error("erase unable to open blockdev", "REDFISH_MESSAGE_ID",
                   std::string("openbmc.0.1.driveerasefailure"),
                   "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
        throw EraseError();
    }
    unsigned long currentIndex = 0;
    unsigned long seed = SEED;
    std::minstd_rand0 generator(seed);

    while (currentIndex < driveSize)
    {
        uint64_t rand;
        std::vector<std::byte> read(sizeof(rand));
        try
        {
            rand = generator();
            fd.read(read);
        }
        catch (...)
        {
            lg2::error("Estoraged erase pattern unable to read sizeof(long)",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"),
                       "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
            throw EraseError();
        }

        // compare
        // uint64_t read = reinterpret_cast<uint64_t>(buf.data());

        std::byte* randByte = reinterpret_cast<std::byte*>(&rand);
        std::span<std::byte> randSpan(randByte, sizeof(rand));
        std::span<std::byte> readSpan{read};
        if (spanEq(randSpan, readSpan, randSpan.size()))
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
bool pattern::spanEq(std::span<std::byte>& firstSpan,
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
