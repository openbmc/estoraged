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
     */
    GetStorageConfiguration(
        std::shared_ptr<sdbusplus::asio::connection> connection,
        std::function<void(ManagedStorageType& resp)>&& callbackFunc) :
        dbusConnection(std::move(connection)), callback(std::move(callbackFunc))
    {}

    GetStorageConfiguration& operator=(const GetStorageConfiguration&) = delete;
    GetStorageConfiguration(const GetStorageConfiguration&) = delete;
    GetStorageConfiguration(GetStorageConfiguration&&) = default;
    GetStorageConfiguration& operator=(GetStorageConfiguration&&) = default;

    /** @brief Destructor for GetStorageConfiguration.
     *  @details This will execute the callback provided to the constructor.
     */
    ~GetStorageConfiguration();

    /** @brief Find the Entity Manager config objects for eStoraged. */
    void getConfiguration();

    /** @brief Get the D-Bus properties from the config object.
     *
     *  @param[in] path - D-Bus object path of the config object.
     *  @param[in] owner - D-Bus service that owns the config object.
     *  @param[in] retries - (optional) Number of times to retry, if needed.
     */
    void getStorageInfo(const std::string& path, const std::string& owner,
                        size_t retries = 5);

    /** @brief Map containing config objects with corresponding properties. */
    ManagedStorageType respData;

    /** @brief Connection to D-Bus. */
    std::shared_ptr<sdbusplus::asio::connection> dbusConnection;

  private:
    /** @brief callback to process the config object data in respData. */
    std::function<void(ManagedStorageType& resp)> callback;
};

} // namespace estoraged
