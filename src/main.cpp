
#include "estoraged.hpp"

#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>
#include <util.hpp>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

static void usage(std::string_view name)
{
    std::cerr
        << "Usage: " << name
        << "eStorageD service on the BMC\n\n"
           "  -b <blockDevice>          The phyical encrypted device\n"
           "                            If omitted, default is /dev/mmcblk0.\n"
           "  -c <containerName>        The LUKS container name to be created\n"
           "                            If omitted, default is luks-<devName>"
           "  -s <sysfsDevice>          The interface to kernel data "
           "structures\n"
           "                            Dealing with this disk. If omitted, "
           "default\n"
           "                            /sys/block/mmcblk0/device/ \n";
}

int main(int argc, char** argv)
{
    std::string physicalBlockDev = "/dev/mmcblk0";
    std::string sysfsDev = "/sys/block/mmcblk0/device";
    std::string containerBlockDev;
    int opt = 0;
    while ((opt = getopt(argc, argv, "b:c:s:")) != -1)
    {
        switch (opt)
        {
            case 'b':
                physicalBlockDev = optarg;
                break;
            case 'c':
                containerBlockDev = optarg;
                break;
            case 's':
                sysfsDev = optarg;
                break;
            default:
                usage(*argv);
                exit(EXIT_FAILURE);
        }
    }
    try
    {
        /* Get the filename of the device (without "/dev/"). */
        std::string deviceName =
            std::filesystem::path(physicalBlockDev).filename().string();
        /* If containerName arg wasn't provided, create one based on deviceName.
         */
        if (containerBlockDev.empty())
        {
            containerBlockDev = "luks-" + deviceName;
        }

        // setup connection to dbus
        boost::asio::io_context io;
        auto conn = std::make_shared<sdbusplus::asio::connection>(io);
        // request D-Bus server name.
        std::string busName = "xyz.openbmc_project.eStoraged." + deviceName;
        conn->request_name(busName.c_str());
        auto server = sdbusplus::asio::object_server(conn);

        estoraged::EStoraged esObject{
            server, physicalBlockDev, containerBlockDev,
            estoraged::util::findSizeOfBlockDevice(physicalBlockDev),
            estoraged::util::findPredictedMediaLifeLeftPercent(sysfsDev)};
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
