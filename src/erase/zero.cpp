#include "zero.hpp"

#include "erase.hpp"

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <array>
#include <span>

namespace estoraged
{

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using stdplus::fd::ManagedFd;

void Zero::writeZero(const uint64_t driveSize, ManagedFd& fd)
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
            fd.write({blockOfZeros.data(), writeSize});
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

void Zero::verifyZero(uint64_t driveSize, ManagedFd& fd)
{
    uint64_t currentIndex = 0;
    std::array<std::byte, blockSize> readArr;
    const std::array<const std::byte, blockSize> blockOfZeros{};

    while (currentIndex < driveSize)
    {
        uint32_t readSize = currentIndex + blockSize < driveSize
                                ? blockSize
                                : driveSize - currentIndex;
        try
        {
            fd.read({readArr.data(), readSize});
        }
        catch (...)
        {
            lg2::error("Estoraged erase zeros block unable to read size",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"));
            throw InternalFailure();
        }
        if (memcmp(readArr.data(), blockOfZeros.data(), readSize))
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
