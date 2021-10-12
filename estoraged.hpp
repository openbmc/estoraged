#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/eStoraged/server.hpp>

#include <vector>

namespace openbmc
{
using eStoraged_inherit = sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::server::eStoraged>;

class eStoraged : eStoraged_inherit
{
  public:
    eStoraged(sdbusplus::bus::bus& bus, const char* path, std::string physPath, std::string containerPath) :
      eStoraged_inherit(bus, path), m_phyPath(physPath), m_containerPath(containerPath)
    {}

    void format(
        std::vector<uint8_t> encryptionPassword,
        std::vector<uint8_t> devicePassword) override;

    void erase(
        std::vector<uint8_t> encryptionPassword,
        std::vector<uint8_t> devicePassword,
        EraseMethod inEraseMethod) override;

    void lock(std::vector<uint8_t> devicePassword) override;

    void unlock(
        std::vector<uint8_t> encryptionPassword,
        std::vector<uint8_t> devicePassword) override;

    void changePassword(
        std::vector<uint8_t> oldEncryptionPassword,
        std::vector<uint8_t> oldDevicePassword,
        std::vector<uint8_t> newEncryptionPassword,
        std::vector<uint8_t> newDevicePassword) override;
  private:
    std::string m_phyPath;
    std::string m_containerPath;
};


} // namespace openbmc
