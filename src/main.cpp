
#include "estoraged.hpp"
#include "getConfig.hpp"
#include "util.hpp"

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/throw_exception.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <util.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

/*
 * Get the configuration objects from Entity Manager and create new D-Bus
 * objects for each one. This function can be called multiple times, in case
 * new configuration objects show up later.
 *
 * Note: Currently, eStoraged can only support 1 eMMC device.
 * Additional changes will be needed to support more than 1 eMMC, or to support
 * more types of storage devices.
 */
void createStorageObjects(
    sdbusplus::asio::object_server& objectServer,
    boost::container::flat_map<
        std::string, std::unique_ptr<estoraged::EStoraged>>& storageObjects,
    std::shared_ptr<sdbusplus::asio::connection>& dbusConnection)
{
    auto getter = std::make_shared<estoraged::GetStorageConfiguration>(
        dbusConnection,
        [&objectServer, &storageObjects](
            const estoraged::ManagedStorageType& storageConfigurations) {
            size_t numConfigObj = storageConfigurations.size();
            if (numConfigObj > 1)
            {
                lg2::error(
                    "eStoraged can only manage 1 eMMC device; found {NUM}",
                    "NUM", numConfigObj, "REDFISH_MESSAGE_ID",
                    std::string("OpenBMC.0.1.CreateStorageObjectsFail"));
                return;
            }

            for (const std::pair<sdbusplus::message::object_path,
                                 estoraged::StorageData>& storage :
                 storageConfigurations)
            {
                const std::string& path = storage.first.str;

                if (storageObjects.find(path) != storageObjects.end())
                {
                    /*
                     * We've already created this object, or at least
                     * attempted to.
                     */
                    continue;
                }

                /* Get the properties from the config object. */
                const estoraged::StorageData& data = storage.second;

                /* Look for the device file. */
                const std::filesystem::path blockDevDir{"/sys/block"};
                std::filesystem::path deviceFile, sysfsDir;
                std::string luksName;
                bool found = estoraged::util::findDevice(
                    data, blockDevDir, deviceFile, sysfsDir, luksName);
                if (!found)
                {
                    lg2::error(
                        "Device not found for path {PATH}", "PATH", path,
                        "REDFISH_MESSAGE_ID",
                        std::string("OpenBMC.0.1.CreateStorageObjectsFail"));
                    /*
                     * Set a NULL pointer as a placeholder, so that we don't
                     * try and fail again later.
                     */
                    storageObjects[path] = nullptr;
                    continue;
                }

                uint64_t size =
                    estoraged::util::findSizeOfBlockDevice(deviceFile);

                uint8_t lifeleft =
                    estoraged::util::findPredictedMediaLifeLeftPercent(
                        sysfsDir);
                std::string partNumber =
                    estoraged::util::getPartNumber(sysfsDir);
                std::string serialNumber =
                    estoraged::util::getSerialNumber(sysfsDir);
                /* Create the storage object. */
                storageObjects[path] = std::make_unique<estoraged::EStoraged>(
                    objectServer, path, deviceFile, luksName, size, lifeleft,
                    partNumber, serialNumber);
                lg2::info("Created eStoraged object for path {PATH}", "PATH",
                          path, "REDFISH_MESSAGE_ID",
                          std::string("OpenBMC.0.1.CreateStorageObjects"));
            }
        });
    getter->getConfiguration();
}

int main(void)
{
    try
    {
        // setup connection to dbus
        boost::asio::io_context io;
        auto conn = std::make_shared<sdbusplus::asio::connection>(io);
        // request D-Bus server name.
        conn->request_name("xyz.openbmc_project.eStoraged");
        sdbusplus::asio::object_server server(conn);
        boost::container::flat_map<std::string,
                                   std::unique_ptr<estoraged::EStoraged>>
            storageObjects;

        boost::asio::post(io, [&]() {
            createStorageObjects(server, storageObjects, conn);
        });

        /*
         * Set up an event handler to process any new configuration objects
         * that show up later.
         */
        boost::asio::deadline_timer filterTimer(io);
        std::function<void(sdbusplus::message_t&)> eventHandler =
            [&](sdbusplus::message_t& message) {
                if (message.is_method_error())
                {
                    lg2::error("eventHandler callback method error");
                    return;
                }
                /*
                 * This implicitly cancels the timer, if it's already pending.
                 * If there's a burst of events within a short period, we want
                 * to handle them all at once. So, we will wait this long for no
                 * more events to occur, before processing them.
                 */
                filterTimer.expires_from_now(boost::posix_time::seconds(1));

                filterTimer.async_wait(
                    [&](const boost::system::error_code& ec) {
                        if (ec == boost::asio::error::operation_aborted)
                        {
                            /* we were canceled */
                            return;
                        }
                        if (ec)
                        {
                            lg2::error("timer error");
                            return;
                        }
                        createStorageObjects(server, storageObjects, conn);
                    });
            };

        auto match = std::make_unique<sdbusplus::bus::match_t>(
            static_cast<sdbusplus::bus_t&>(*conn),
            "type='signal',member='PropertiesChanged',path_namespace='" +
                std::string("/xyz/openbmc_project/inventory") +
                "',arg0namespace='" + estoraged::emmcConfigInterface + "'",
            eventHandler);

        lg2::info("Storage management service is running", "REDFISH_MESSAGE_ID",
                  std::string("OpenBMC.1.0.ServiceStarted"));

        io.run();
        return 0;
    }
    catch (const std::exception& e)
    {
        lg2::error(e.what(), "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.1.0.ServiceException"));

        return 2;
    }
    return 1;
}
