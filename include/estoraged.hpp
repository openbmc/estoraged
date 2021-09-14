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
    eStoraged(sdbusplus::bus::bus& bus, const char* path) :
      eStoraged_inherit(bus, path)
    {}

    void format(std::vector<uint8_t> password) override;

    void erase(std::vector<uint8_t> password,
               enum EraseMethod eraseType) override;

    void lock(std::vector<uint8_t> password) override;

    void unlock(std::vector<uint8_t> password) override;

    void changePassword(
        std::vector<uint8_t> oldPassword,
        std::vector<uint8_t> newPassword) override;
};

} // namespace openbmc
