#include "zero.hpp"

#include "erase.hpp"

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <array>
#include <span>

constexpr size_t BlockSize = 4096;

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using stdplus::fd::ManagedFd;

void Zero::writeZero(const uint64_t driveSize, ManagedFd& fd)
{

    uint64_t currentIndex = 0;
    std::array<std::byte, BlockSize> blockOfZeros{};
    while (currentIndex < driveSize)
    {
        uint32_t writeSize = currentIndex + BlockSize < driveSize
                                 ? BlockSize
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
    std::array<std::byte, BlockSize> readArr;
    while (currentIndex < driveSize)
    {
        uint32_t readSize = currentIndex + BlockSize < driveSize
                                ? BlockSize
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
        for (std::byte b : readArr)
        {
            if (b != std::byte{0})
            {
                lg2::error("Estoraged erase zeros block is not zero",
                           "REDFISH_MESSAGE_ID",
                           std::string("eStorageD.1.0.EraseFailure"));
                throw InternalFailure();
            }
        }
        currentIndex += readSize;
    }
}
