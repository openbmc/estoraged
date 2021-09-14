#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/eStoraged/server.hpp>

#include <vector>

namespace estoraged
{
using eStoragedInherit = sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::server::eStoraged>;

class eStoraged : eStoragedInherit
{
  public:
    eStoraged(sdbusplus::bus::bus& bus, const char* path) :
      eStoragedInherit(bus, path)
    {}

    void format(std::vector<uint8_t> password) override;

    void erase(std::vector<uint8_t> password,
               EraseMethod eraseType) override;

    void lock(std::vector<uint8_t> password) override;

    void unlock(std::vector<uint8_t> password) override;

    void changePassword(
        std::vector<uint8_t> oldPassword,
        std::vector<uint8_t> newPassword) override;
};

} // namespace openbmc
