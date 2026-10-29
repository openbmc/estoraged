#pragma once

#include "estoraged_conf.hpp"

#include <boost/container/flat_map.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/message.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace estoraged
{

#if HAVE_SDBUSPLUS_OBJECT_PATH
using object_path = sdbusplus::object_path;
#else
using object_path = sdbusplus::message::object_path;
#endif

using BasicVariantType =
    std::variant<std::vector<std::string>, std::string, int64_t, uint64_t,
                 double, int32_t, uint32_t, int16_t, uint16_t, uint8_t, bool>;

/* Map of properties for the config interface. */
using StorageData = boost::container::flat_map<std::string, BasicVariantType>;
using ManagedStorageType = boost::container::flat_map<object_path, StorageData>;

const constexpr char* emmcConfigInterface =
    "xyz.openbmc_project.Configuration.EmmcDevice";

/** @brief D-Bus ObjectMapper endpoint and retry settings. */
struct MapperConfig
{
    std::string busName = "xyz.openbmc_project.ObjectMapper";
    std::string path = "/xyz/openbmc_project/object_mapper";
    std::string interface = "xyz.openbmc_project.ObjectMapper";
    std::chrono::milliseconds retryInterval = std::chrono::seconds(10);
    size_t maxRetries = 5;
};

/** @class GetStorageConfiguration
 *  @brief Object used to find Entity Manager config objects.
 *  @details eStoraged will create a new D-Bus object for each config object.
 */
class GetStorageConfiguration :
    public std::enable_shared_from_this<GetStorageConfiguration>
{
  public:
    /** @brief Constructor for GetStorageConfiguration
     *
     *  @param[in] connection - shared_ptr to D-Bus connection object.
     *  @param[in] callbackFunc - callback to run after finding the config
     *    objects.
     *  @param[in] mapperCfg - optional ObjectMapper endpoint and retry config.
     */
    GetStorageConfiguration(
        std::shared_ptr<sdbusplus::asio::connection> connection,
        std::function<void(ManagedStorageType& resp)>&& callbackFunc,
        MapperConfig mapperCfg = {}) :
        dbusConnection(std::move(connection)),
        callback(std::move(callbackFunc)), mapperConfig(std::move(mapperCfg))
    {}

    GetStorageConfiguration& operator=(const GetStorageConfiguration&) = delete;
    GetStorageConfiguration(const GetStorageConfiguration&) = delete;
    GetStorageConfiguration(GetStorageConfiguration&&) = delete;
    GetStorageConfiguration& operator=(GetStorageConfiguration&&) = delete;

    /** @brief Destructor for GetStorageConfiguration.
     *  @details This will execute the callback provided to the constructor.
     */
    ~GetStorageConfiguration();

    /** @brief Find the Entity Manager config objects for eStoraged.
     *
     *  @param[in] retries - Number of times to retry GetSubTree on failure.
     */
    void getConfiguration(size_t retries);

    /** @brief Find the Entity Manager config objects using default retries. */
    void getConfiguration()
    {
        getConfiguration(mapperConfig.maxRetries);
    }

    /** @brief Get the D-Bus properties from the config object.
     *
     *  @param[in] path - D-Bus object path of the config object.
     *  @param[in] owner - D-Bus service that owns the config object.
     *  @param[in] retries - Number of times to retry, if needed.
     */
    void getStorageInfo(const std::string& path, const std::string& owner,
                        size_t retries);

    /** @brief Get the D-Bus properties using default retries. */
    void getStorageInfo(const std::string& path, const std::string& owner)
    {
        getStorageInfo(path, owner, mapperConfig.maxRetries);
    }

    /** @brief Map containing config objects with corresponding properties. */
    ManagedStorageType respData;

    /** @brief Connection to D-Bus. */
    std::shared_ptr<sdbusplus::asio::connection> dbusConnection;

  private:
    /** @brief callback to process the config object data in respData. */
    std::function<void(ManagedStorageType& resp)> callback;
    MapperConfig mapperConfig;
};

} // namespace estoraged
