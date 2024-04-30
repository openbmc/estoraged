
#include "getConfig.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace estoraged
{

namespace mapper
{
constexpr const char* busName = "xyz.openbmc_project.ObjectMapper";
constexpr const char* path = "/xyz/openbmc_project/object_mapper";
constexpr const char* interface = "xyz.openbmc_project.ObjectMapper";
constexpr const char* subtree = "GetSubTree";
} // namespace mapper

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

void GetStorageConfiguration::getStorageInfo(const std::string& path,
                                             const std::string& owner,
                                             size_t retries)
{
    std::shared_ptr<GetStorageConfiguration> self = shared_from_this();
    self->dbusConnection->async_method_call(
        [self, path, owner](
            const boost::system::error_code ec,
            boost::container::flat_map<std::string, BasicVariantType>& data) {
        if (ec)
        {
            lg2::error(
                "Error getting properties for {PATH}: {RETRIES} retries left",
                "PATH", path, "RETRIES", retries - 1, "REDFISH_MESSAGE_ID",
                std::string("OpenBMC.0.1.GetStorageInfoFail"));

            auto timer = std::make_shared<boost::asio::steady_timer>(
                self->dbusConnection->get_io_context());
            timer->expires_after(std::chrono::seconds(10));
            timer->async_wait([self, timer, path, owner,
                               retries](boost::system::error_code ec) {
                if (ec)
                {
                    std::cerr << "Timer error!\n";
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

void GetStorageConfiguration::getConfiguration()
{
    std::shared_ptr<GetStorageConfiguration> self = shared_from_this();
    dbusConnection->async_method_call(
        [self](const boost::system::error_code ec, const GetSubTreeType& ret) {
        if (ec)
        {
            lg2::error("Error calling mapper");
            return;
        }
        for (const auto& [objPath, objDict] : ret)
        {
            if (objDict.empty())
            {
                return;
            }
            const std::string& objOwner = objDict.begin()->first;
            /* Look for the config interface exposed by this object. */
            for (const std::string& interface : objDict.begin()->second)
            {
                if (interface.compare(emmcConfigInterface) == 0)
                {
                    /* Get the properties exposed by this interface. */
                    self->getStorageInfo(objPath, objOwner);
                }
            }
        }
    },
        mapper::busName, mapper::path, mapper::interface, mapper::subtree, "/",
        0, std::vector<const char*>(1, emmcConfigInterface));
}

GetStorageConfiguration::~GetStorageConfiguration()
{
    callback(respData);
}

} // namespace estoraged
