#include "pattern.hpp"

#include "erase.hpp"

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

#include <random>
#include <span>
#include <vector>

#define SEED 0x6a656272

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;
using stdplus::fd::ManagedFd;

int pattern::writePattern()
{
    const uint64_t driveSize = findSizeOfBlockDevice();
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
    // while we can write a long to the block device, write a long
    while (currentIndex - sizeof(uint64_t) < driveSize)
    {
        uint64_t rand = generator();
        std::byte* randByte = reinterpret_cast<std::byte*>(&rand);
        std::span<const std::byte> randSpan(randByte, sizeof(rand));
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

    // write part of a uint64_t
    if (currentIndex < driveSize)
    {
        uint64_t rand = static_cast<uint64_t>(generator());
        std::byte* randByte = reinterpret_cast<std::byte*>(&rand);
        std::span<const std::byte> randSpan(randByte, driveSize - currentIndex);
        if (fd.write(randSpan)[0] !=
            static_cast<std::byte>(driveSize - currentIndex))
        {
            lg2::error(
                "Estoraged erase pattern unable to write sizeof(uint8_t)",
                "REDFISH_MESSAGE_ID", std::string("eStorageD.1.0.EraseFailure"),
                "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
            throw EraseError();
        }
    }
    return 0;
}

int pattern::verifyPattern()
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
    const uint64_t driveSize = findSizeOfBlockDevice();

    unsigned long currentIndex = 0;
    unsigned long seed = SEED;
    std::minstd_rand0 generator(seed);

    while (currentIndex - sizeof(uint64_t) < driveSize)
    {
        uint64_t rand;
        std::vector<std::byte> buf(sizeof(rand));
        try
        {
            rand = generator();
            fd.read(buf);
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
        uint64_t read = reinterpret_cast<uint64_t>(buf.data());
        if (rand != read)
        {
            lg2::error("Estoraged erase pattern does not match",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"),
                       "REDFISH_MESSAGE_ARGS",
                       std::to_string(rand) + "!=" + std::to_string(read));
            throw EraseError();
        }
        currentIndex = currentIndex + sizeof(rand);
    }

    if (currentIndex < driveSize)
    {
        int remanding = static_cast<int>(driveSize - currentIndex);
        uint64_t rand;
        std::vector<std::byte> buf(remanding);
        try
        {
            rand = generator();
            fd.read(buf);
        }
        catch (...)
        {
            lg2::error("Estoraged erase pattern unable to read 1",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"));
            throw EraseError();
        }
        uint64_t read = reinterpret_cast<uint64_t>(buf.data());
        if (rand != read)
        {
            lg2::error("Estoraged erase pattern does not match",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"),
                       "REDFISH_MESSAGE_ARGS",
                       std::to_string(rand) + "!=" + std::to_string(read));
            throw EraseError();
        }
    }
    return 0;
}
