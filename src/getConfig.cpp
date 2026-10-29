
#include "getConfig.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/message.hpp>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace estoraged
{

namespace mapper
{
constexpr const char* subtree = "GetSubTree";
} // namespace mapper

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

std::optional<std::pair<std::string, StorageData>> extractEmmcConfig(
    sdbusplus::message_t& message)
{
    if (message.is_method_error())
    {
        lg2::error("InterfacesAdded callback method error");
        return std::nullopt;
    }

    object_path objectPath;
    boost::container::flat_map<std::string, StorageData> interfaces;
    try
    {
        message.read(objectPath, interfaces);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to parse InterfacesAdded: {ERROR}", "ERROR",
                   e.what());
        return std::nullopt;
    }

    auto it = interfaces.find(emmcConfigInterface);
    if (it == interfaces.end())
    {
        return std::nullopt;
    }

    return std::make_pair(std::move(objectPath.str), std::move(it->second));
}

bool GetStorageConfiguration::isPathAlreadyHandled(
    const std::string& path) const
{
    return isPathKnown && isPathKnown(path);
}

std::shared_ptr<boost::asio::steady_timer>
    GetStorageConfiguration::makeRetryTimer()
{
    std::erase_if(retryTimers,
                  [](const auto& weakTimer) { return weakTimer.expired(); });
    auto timer = std::make_shared<boost::asio::steady_timer>(
        dbusConnection->get_io_context());
    retryTimers.push_back(timer);
    return timer;
}

void GetStorageConfiguration::cancel() noexcept
{
    cancelled = true;
    for (const auto& weakTimer : retryTimers)
    {
        if (auto timer = weakTimer.lock())
        {
            try
            {
                timer->cancel();
            }
            catch (const std::exception& e)
            {
                lg2::error("Failed to cancel retry timer: {ERROR}", "ERROR",
                           e.what());
            }
            catch (...)
            {
                lg2::error("Failed to cancel retry timer");
            }
        }
    }
    retryTimers.clear();
}

void GetStorageConfiguration::getStorageInfo(
    const std::string& path, const std::string& owner, size_t retries)
{
    if (cancelled || isPathAlreadyHandled(path))
    {
        return;
    }

    std::shared_ptr<GetStorageConfiguration> self = shared_from_this();
    self->dbusConnection->async_method_call(
        [self, path, owner, retries](
            const boost::system::error_code ec,
            boost::container::flat_map<std::string, BasicVariantType>& data) {
            if (self->cancelled || self->isPathAlreadyHandled(path))
            {
                return;
            }
            if (ec)
            {
                if (retries == 0U)
                {
                    lg2::error(
                        "Error getting properties for {PATH}: no retries left",
                        "PATH", path, "REDFISH_MESSAGE_ID",
                        std::string("OpenBMC.0.1.GetStorageInfoFail"));
                    return;
                }

                lg2::error(
                    "Error getting properties for {PATH}: {RETRIES} retries left",
                    "PATH", path, "RETRIES", retries - 1, "REDFISH_MESSAGE_ID",
                    std::string("OpenBMC.0.1.GetStorageInfoFail"));

                auto timer = self->makeRetryTimer();
                timer->expires_after(self->mapperConfig.retryInterval);
                timer->async_wait([self, timer, path, owner,
                                   retries](boost::system::error_code waitEc) {
                    if (waitEc == boost::asio::error::operation_aborted ||
                        self->cancelled)
                    {
                        return;
                    }
                    if (waitEc)
                    {
                        lg2::error("Timer error!");
                        return;
                    }
                    self->getStorageInfo(path, owner, retries - 1);
                });

                return;
            }

            self->respData[path] = std::move(data);
        },
        owner, path, "org.freedesktop.DBus.Properties", "GetAll",
        emmcConfigInterface);
}

void GetStorageConfiguration::getConfiguration(size_t retries)
{
    if (cancelled)
    {
        return;
    }

    std::shared_ptr<GetStorageConfiguration> self = shared_from_this();
    dbusConnection->async_method_call(
        [self, retries](const boost::system::error_code ec,
                        const GetSubTreeType& ret) {
            if (self->cancelled)
            {
                return;
            }
            if (ec)
            {
                if (retries == 0U)
                {
                    lg2::error("Error calling mapper: no retries left");
                    return;
                }

                lg2::error("Error calling mapper: {RETRIES} retries left",
                           "RETRIES", retries - 1);

                auto timer = self->makeRetryTimer();
                timer->expires_after(self->mapperConfig.retryInterval);
                timer->async_wait(
                    [self, timer, retries](boost::system::error_code waitEc) {
                        if (waitEc == boost::asio::error::operation_aborted ||
                            self->cancelled)
                        {
                            return;
                        }
                        if (waitEc)
                        {
                            lg2::error("Timer error!");
                            return;
                        }
                        self->getConfiguration(retries - 1);
                    });
                return;
            }
            for (const auto& [objPath, objDict] : ret)
            {
                if (objDict.empty() || self->isPathAlreadyHandled(objPath))
                {
                    continue;
                }
                const std::string& objOwner = objDict.begin()->first;
                /* Look for the config interface exposed by this object. */
                for (const std::string& interface : objDict.begin()->second)
                {
                    if (interface == emmcConfigInterface)
                    {
                        /* Get the properties exposed by this interface. */
                        self->getStorageInfo(objPath, objOwner);
                        break;
                    }
                }
            }
        },
        mapperConfig.busName, mapperConfig.path, mapperConfig.interface,
        mapper::subtree, "/", 0,
        std::vector<const char*>(1, emmcConfigInterface));
}

GetStorageConfiguration::~GetStorageConfiguration()
{
    if (cancelled)
    {
        return;
    }

    /*
     * Destructors are implicitly noexcept. Catch all exceptions such that an
     * exception thrown by the callback does not trigger std::terminate().
     */
    try
    {
        callback(respData);
    }
    catch (const std::exception& e)
    {
        lg2::error("Unhandled exception in configuration callback: {ERROR}",
                   "ERROR", e.what());
    }
    catch (...)
    {
        lg2::error("Unhandled unknown exception in configuration callback");
    }
}

StorageConfigScanner::StorageConfigScanner(
    std::shared_ptr<sdbusplus::asio::connection> connection,
    CreateCallback createFunc, PathKnownCallback pathKnownFunc,
    MapperConfig mapperCfg, std::chrono::milliseconds debounce) :
    dbusConnection(std::move(connection)),
    createCallback(std::move(createFunc)),
    isPathKnown(std::move(pathKnownFunc)), mapperConfig(std::move(mapperCfg)),
    debounceDelay(debounce), filterTimer(dbusConnection->get_io_context())
{}

StorageConfigScanner::~StorageConfigScanner()
{
    cancelFilterTimer();
    if (auto getter = activeGetter.lock())
    {
        getter->cancel();
    }
}

void StorageConfigScanner::cancelFilterTimer() noexcept
{
    try
    {
        filterTimer.cancel();
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to cancel filter timer: {ERROR}", "ERROR", e.what());
    }
    catch (...)
    {
        lg2::error("Failed to cancel filter timer");
    }
}

bool StorageConfigScanner::invokeCreateCallback(
    const std::string& path, const StorageData& data) noexcept
{
    if (!createCallback)
    {
        return false;
    }
    try
    {
        return createCallback(path, data);
    }
    catch (const std::exception& e)
    {
        lg2::error("Unhandled exception in configuration callback: {ERROR}",
                   "ERROR", e.what());
    }
    catch (...)
    {
        lg2::error("Unhandled unknown exception in configuration callback");
    }
    return false;
}

void StorageConfigScanner::startScan()
{
    unscopedScanPending = true;
    startPendingScan();
}

void StorageConfigScanner::handlePropertiesChanged(
    sdbusplus::message_t& message) noexcept
{
    try
    {
        if (message.is_method_error())
        {
            lg2::error("eventHandler callback method error");
            return;
        }

        const char* path = message.get_path();
        if (path != nullptr)
        {
            if (isPathKnown && isPathKnown(path))
            {
                return;
            }
            pendingPaths.insert(path);
        }
        else
        {
            unscopedScanPending = true;
        }

        scheduleFilteredScan();
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to handle PropertiesChanged: {ERROR}", "ERROR",
                   e.what());
    }
    catch (...)
    {
        lg2::error("Failed to handle PropertiesChanged");
    }
}

void StorageConfigScanner::handleInterfacesAdded(
    sdbusplus::message_t& message) noexcept
{
    try
    {
        auto extracted = extractEmmcConfig(message);
        if (!extracted)
        {
            return;
        }

        const auto& [path, data] = *extracted;
        const bool created = invokeCreateCallback(path, data);

        if (created || (isPathKnown && isPathKnown(path)))
        {
            pendingPaths.erase(path);
            if (pendingPaths.empty() && !unscopedScanPending)
            {
                cancelFilterTimer();
                if (created)
                {
                    if (auto getter = activeGetter.lock())
                    {
                        getter->cancel();
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to handle InterfacesAdded: {ERROR}", "ERROR",
                   e.what());
    }
    catch (...)
    {
        lg2::error("Failed to handle InterfacesAdded");
    }
}

void StorageConfigScanner::startPendingScan()
{
    if (!activeGetter.expired())
    {
        return;
    }

    pruneKnownPendingPaths();
    if (pendingPaths.empty() && !unscopedScanPending)
    {
        return;
    }

    unscopedScanPending = false;
    pendingPaths.clear();

    std::weak_ptr<bool> weakToken = lifetimeToken;
    auto getter = std::make_shared<GetStorageConfiguration>(
        dbusConnection,
        [this, weakToken](ManagedStorageType& storageConfigurations) {
            if (weakToken.expired())
            {
                return;
            }
            processConfigurations(storageConfigurations);
            pruneKnownPendingPaths();
            if (!pendingPaths.empty() || unscopedScanPending)
            {
                boost::asio::post(dbusConnection->get_io_context(),
                                  [this, weakToken]() {
                                      if (!weakToken.expired())
                                      {
                                          startPendingScan();
                                      }
                                  });
            }
        },
        isPathKnown, mapperConfig);
    activeGetter = getter;
    getter->getConfiguration();
}

void StorageConfigScanner::scheduleFilteredScan()
{
    std::weak_ptr<bool> weakToken = lifetimeToken;
    filterTimer.expires_after(debounceDelay);
    filterTimer.async_wait([this,
                            weakToken](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted || weakToken.expired())
        {
            return;
        }
        if (ec)
        {
            lg2::error("timer error");
            return;
        }
        startPendingScan();
    });
}

void StorageConfigScanner::processConfigurations(
    const ManagedStorageType& storageConfigurations)
{
    size_t numConfigObj = storageConfigurations.size();
    if (numConfigObj > 1)
    {
        lg2::error("eStoraged can only manage 1 eMMC device; found {NUM}",
                   "NUM", numConfigObj, "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.CreateStorageObjectsFail"));
        return;
    }

    for (const auto& [path, data] : storageConfigurations)
    {
        invokeCreateCallback(path.str, data);
    }
}

void StorageConfigScanner::pruneKnownPendingPaths()
{
    if (!isPathKnown)
    {
        return;
    }
    for (auto it = pendingPaths.begin(); it != pendingPaths.end();)
    {
        if (isPathKnown(*it))
        {
            it = pendingPaths.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace estoraged
