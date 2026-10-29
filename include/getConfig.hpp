#pragma once

#include "estoraged_conf.hpp"

#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/message.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
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

/** @brief Extract EmmcDevice configuration from an InterfacesAdded signal.
 *
 *  @param[in] message - D-Bus InterfacesAdded signal message.
 *  @return Pair of (config object path, StorageData) if emmcConfigInterface
 *          is present in the signal payload, or std::nullopt otherwise.
 */
std::optional<std::pair<std::string, StorageData>> extractEmmcConfig(
    sdbusplus::message_t& message);

/** @class GetStorageConfiguration
 *  @brief Object used to find Entity Manager config objects.
 *  @details eStoraged will create a new D-Bus object for each config object.
 */
class GetStorageConfiguration :
    public std::enable_shared_from_this<GetStorageConfiguration>
{
  public:
    using PathKnownCallback = std::function<bool(const std::string&)>;

    /** @brief Constructor for GetStorageConfiguration
     *
     *  @param[in] connection - shared_ptr to D-Bus connection object.
     *  @param[in] callbackFunc - callback to run after finding the config
     *    objects.
     *  @param[in] pathKnownFunc - optional predicate returning true if a config
     *    object path has already been processed.
     *  @param[in] mapperCfg - optional ObjectMapper endpoint and retry config.
     */
    GetStorageConfiguration(
        std::shared_ptr<sdbusplus::asio::connection> connection,
        std::function<void(ManagedStorageType& resp)>&& callbackFunc,
        PathKnownCallback pathKnownFunc = nullptr,
        MapperConfig mapperCfg = {}) :
        dbusConnection(std::move(connection)),
        callback(std::move(callbackFunc)),
        isPathKnown(std::move(pathKnownFunc)),
        mapperConfig(std::move(mapperCfg))
    {}

    GetStorageConfiguration& operator=(const GetStorageConfiguration&) = delete;
    GetStorageConfiguration(const GetStorageConfiguration&) = delete;
    GetStorageConfiguration(GetStorageConfiguration&&) = delete;
    GetStorageConfiguration& operator=(GetStorageConfiguration&&) = delete;

    /** @brief Destructor for GetStorageConfiguration.
     *  @details This will execute the callback provided to the constructor
     *    unless cancel() has been called.
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

    /** @brief Cancel pending retry timers and suppress the completion callback.
     */
    void cancel() noexcept;

    /** @brief Map containing config objects with corresponding properties. */
    ManagedStorageType respData;

    /** @brief Connection to D-Bus. */
    std::shared_ptr<sdbusplus::asio::connection> dbusConnection;

  private:
    bool isPathAlreadyHandled(const std::string& path) const;
    std::shared_ptr<boost::asio::steady_timer> makeRetryTimer();

    /** @brief callback to process the config object data in respData. */
    std::function<void(ManagedStorageType& resp)> callback;
    PathKnownCallback isPathKnown;
    MapperConfig mapperConfig;
    std::vector<std::weak_ptr<boost::asio::steady_timer>> retryTimers;
    bool cancelled = false;
};

/** @class StorageConfigScanner
 *  @brief Coordinates initial scan, PropertiesChanged, and InterfacesAdded
 *         discovery without overlapping D-Bus retry chains.
 */
class StorageConfigScanner
{
  public:
    /** @brief Callback invoked to create a storage object for @p path.
     *  @return true if a valid storage object is active for @p path, false
     *          otherwise.
     */
    using CreateCallback =
        std::function<bool(const std::string& path, const StorageData& data)>;
    using PathKnownCallback = std::function<bool(const std::string& path)>;

    StorageConfigScanner(
        std::shared_ptr<sdbusplus::asio::connection> connection,
        CreateCallback createFunc, PathKnownCallback pathKnownFunc,
        MapperConfig mapperCfg = {},
        std::chrono::milliseconds debounce = std::chrono::seconds(1));

    ~StorageConfigScanner();

    StorageConfigScanner(const StorageConfigScanner&) = delete;
    StorageConfigScanner& operator=(const StorageConfigScanner&) = delete;
    StorageConfigScanner(StorageConfigScanner&&) = delete;
    StorageConfigScanner& operator=(StorageConfigScanner&&) = delete;

    /** @brief Schedule an immediate ObjectMapper scan if none is active. */
    void startScan();

    /** @brief Handle an EmmcDevice PropertiesChanged signal. */
    void handlePropertiesChanged(sdbusplus::message_t& message) noexcept;

    /** @brief Handle an inventory InterfacesAdded signal. */
    void handleInterfacesAdded(sdbusplus::message_t& message) noexcept;

  private:
    void cancelFilterTimer() noexcept;
    bool invokeCreateCallback(const std::string& path,
                              const StorageData& data) noexcept;
    void startPendingScan();
    void scheduleFilteredScan();
    void processConfigurations(const ManagedStorageType& storageConfigurations);
    void pruneKnownPendingPaths();

    std::shared_ptr<sdbusplus::asio::connection> dbusConnection;
    CreateCallback createCallback;
    PathKnownCallback isPathKnown;
    MapperConfig mapperConfig;
    std::chrono::milliseconds debounceDelay;
    boost::asio::steady_timer filterTimer;
    std::weak_ptr<GetStorageConfiguration> activeGetter;
    boost::container::flat_set<std::string> pendingPaths;
    std::shared_ptr<bool> lifetimeToken = std::make_shared<bool>(true);
    bool unscopedScanPending = false;
};

} // namespace estoraged
