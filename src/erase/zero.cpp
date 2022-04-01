#include "zero.hpp"

#include "erase.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <array>
#include <chrono>
#include <span>
#include <thread>

namespace estoraged
{

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using stdplus::fd::Fd;

void Zero::writeZero(const uint64_t driveSize, Fd& fd)
{
    uint64_t currentIndex = 0;
    const std::array<const std::byte, blockSize> blockOfZeros{};

    while (currentIndex < driveSize)
    {
        uint32_t writeSize = currentIndex + blockSize < driveSize
                                 ? blockSize
                                 : driveSize - currentIndex;
        try
        {
            size_t written = 0;
            size_t retry = 0;
            while (written < writeSize)
            {
                written += fd.write({blockOfZeros.data() + written,
                                     writeSize - written})
                               .size();
                if (written > writeSize)
                {
                    throw InternalFailure();
                }
                if (retry > 0)
                {
                    std::this_thread::sleep_for(delay);
                }
                retry++;
                if (retry > maxRetry)
                {
                    lg2::error("Unable to make full write",
                               "REDFISH_MESSAGE_ID",
                               std::string("eStorageD.1.0.EraseFailure"));
                    throw InternalFailure();
                }
            }
        }
        catch (...)
        {
            lg2::error("Estoraged erase zeros unable to write size",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"));
            throw InternalFailure();
        }
        currentIndex += writeSize;
    }
}

void Zero::verifyZero(uint64_t driveSize, Fd& fd)
{
    uint64_t currentIndex = 0;
    std::array<std::byte, blockSize> readArr{};
    const std::array<const std::byte, blockSize> blockOfZeros{};

    while (currentIndex < driveSize)
    {
        uint32_t readSize = currentIndex + blockSize < driveSize
                                ? blockSize
                                : driveSize - currentIndex;
        try
        {
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
                if (retry > 0)
                {
                    std::this_thread::sleep_for(delay);
                }
                retry++;
                if (retry > maxRetry)
                {
                    lg2::error("Unable to make full read", "REDFISH_MESSAGE_ID",
                               std::string("eStorageD.1.0.EraseFailure"));
                    throw InternalFailure();
                }
            }
        }
        catch (...)
        {
            lg2::error("Estoraged erase zeros block unable to read size",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"));
            throw InternalFailure();
        }
        if (memcmp(readArr.data(), blockOfZeros.data(), readSize) != 0)
        {
            lg2::error("Estoraged erase zeros block is not zero",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"));
            throw InternalFailure();
        }
        currentIndex += readSize;
    }
}

} // namespace estoraged
