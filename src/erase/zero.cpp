#include "zero.hpp"

#include "erase.hpp"
#include "util.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <chrono>
#include <cstring>
#include <span>
#include <thread>

namespace estoraged
{

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using stdplus::fd::Fd;

void Zero::writeZero(const uint64_t driveSize, Fd& fd)
{
    uint64_t currentIndex = 0;
    const util::AlignedBuffer<std::byte> zeroBuf(blockSize);
    const std::span<const std::byte> blockOfZeros = zeroBuf.span();

    while (currentIndex < driveSize)
    {
        size_t writeSize = currentIndex + blockSize < driveSize
                               ? blockSize
                               : driveSize - currentIndex;
        try
        {
            size_t written = 0;
            size_t retry = 0;
            while (written < writeSize)
            {
                written +=
                    fd.write(blockOfZeros.subspan(written, writeSize - written))
                        .size();
                if (written == writeSize)
                {
                    break;
                }
                if (written > writeSize)
                {
                    throw InternalFailure();
                }
                retry++;
                if (retry > maxRetry)
                {
                    lg2::error("Unable to make full write",
                               "REDFISH_MESSAGE_ID",
                               std::string("eStorageD.1.0.EraseFailure"));
                    throw InternalFailure();
                }
                std::this_thread::sleep_for(delay);
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
    util::AlignedBuffer<std::byte> readBuf(blockSize);
    const std::span<std::byte> readArr = readBuf.span();

    while (currentIndex < driveSize)
    {
        size_t readSize = currentIndex + blockSize < driveSize
                              ? blockSize
                              : driveSize - currentIndex;
        try
        {
            size_t read = 0;
            size_t retry = 0;
            while (read < readSize)
            {
                read += fd.read(readArr.subspan(read, readSize - read)).size();
                if (read == readSize)
                {
                    break;
                }
                if (read > readSize)
                {
                    throw InternalFailure();
                }
                retry++;
                if (retry > maxRetry)
                {
                    lg2::error("Unable to make full read", "REDFISH_MESSAGE_ID",
                               std::string("eStorageD.1.0.EraseFailure"));
                    throw InternalFailure();
                }
                std::this_thread::sleep_for(delay);
            }
        }
        catch (...)
        {
            lg2::error("Estoraged erase zeros block unable to read size",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"));
            throw InternalFailure();
        }
        // The block only contains zeros if the first byte is zero and if each
        // byte equals the byte after it. This avoids allocating a block of
        // zeros to compare with.
        if (readSize != 0 && (readArr[0] != std::byte{0} ||
                              memcmp(readArr.data(), readArr.subspan(1).data(),
                                     readSize - 1) != 0))
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
