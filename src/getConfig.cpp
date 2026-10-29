
#include "getConfig.hpp"

#include <boost/asio/error.hpp>
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

void GetStorageConfiguration::getStorageInfo(
    const std::string& path, const std::string& owner, size_t retries)
{
    std::shared_ptr<GetStorageConfiguration> self = shared_from_this();
    self->dbusConnection->async_method_call(
        [self, path, owner, retries](
            const boost::system::error_code ec,
            boost::container::flat_map<std::string, BasicVariantType>& data) {
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

                auto timer = std::make_shared<boost::asio::steady_timer>(
                    self->dbusConnection->get_io_context());
                timer->expires_after(self->mapperConfig.retryInterval);
                timer->async_wait([self, timer, path, owner,
                                   retries](boost::system::error_code waitEc) {
                    if (waitEc == boost::asio::error::operation_aborted)
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
    std::shared_ptr<GetStorageConfiguration> self = shared_from_this();
    dbusConnection->async_method_call(
        [self, retries](const boost::system::error_code ec,
                        const GetSubTreeType& ret) {
            if (ec)
            {
                if (retries == 0U)
                {
                    lg2::error("Error calling mapper: no retries left");
                    return;
                }

                lg2::error("Error calling mapper: {RETRIES} retries left",
                           "RETRIES", retries - 1);

                auto timer = std::make_shared<boost::asio::steady_timer>(
                    self->dbusConnection->get_io_context());
                timer->expires_after(self->mapperConfig.retryInterval);
                timer->async_wait(
                    [self, timer, retries](boost::system::error_code waitEc) {
                        if (waitEc == boost::asio::error::operation_aborted)
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
                if (objDict.empty())
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

} // namespace estoraged
