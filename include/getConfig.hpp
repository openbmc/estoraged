#pragma once

#include <boost/container/flat_map.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <cstdlib>
#include <functional>
#include <memory>

namespace estoraged
{

using BasicVariantType =
    std::variant<std::vector<std::string>, std::string, int64_t, uint64_t,
                 double, int32_t, uint32_t, int16_t, uint16_t, uint8_t, bool>;

/* Map of properties for the config interface. */
using StorageData = boost::container::flat_map<std::string, BasicVariantType>;
using ManagedStorageType =
    boost::container::flat_map<sdbusplus::message::object_path, StorageData>;

const constexpr char* emmcConfigInterface =
    "xyz.openbmc_project.Configuration.EmmcDevice";

class GetStorageConfiguration :
    public std::enable_shared_from_this<GetStorageConfiguration>
{
  public:
    GetStorageConfiguration(
        std::shared_ptr<sdbusplus::asio::connection> connection,
        std::function<void(ManagedStorageType& resp)>&& callbackFunc) :
        dbusConnection(std::move(connection)),
        callback(std::move(callbackFunc))
    {}

    GetStorageConfiguration& operator=(const GetStorageConfiguration&) = delete;
    GetStorageConfiguration(const GetStorageConfiguration&) = delete;
    GetStorageConfiguration(GetStorageConfiguration&&) = default;
    GetStorageConfiguration& operator=(GetStorageConfiguration&&) = default;

    ~GetStorageConfiguration();

    void getConfiguration(size_t retries);

    void getStorageInfo(const std::string& path, const std::string& owner);

    ManagedStorageType respData;

    std::shared_ptr<sdbusplus::asio::connection> dbusConnection;

    std::function<void(ManagedStorageType& resp)> callback;
};

} // namespace estoraged
