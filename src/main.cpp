
#include "estoraged.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>

#include <filesystem>
#include <iostream>
#include <string>

static void usage(std::string_view name)
{
    std::cerr
        << "Usage: " << name
        << "eStorageD service on the BMC\n\n"
           "  -b <blockDevice>          The phyical encrypted device\n"
           "                            If omitted, default is /dev/mmcblk0.\n"
           "  -c <containerName>        The LUKS container name to be created\n"
           "                            If omitted, default is luks-<devName>";
}

int main(int argc, char** argv)
{

    std::string physicalBlockDev = "/dev/mmcblk0";
    std::string containerBlockDev;
    int opt;
    while ((opt = getopt(argc, argv, "b:c:")) != -1)
    {
        switch (opt)
        {
            case 'b':
                physicalBlockDev = optarg;
                break;
            case 'c':
                containerBlockDev = optarg;
                break;
            default:
                usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    /* Get the filename of the device (without "/dev/"). */
    auto deviceName =
        std::filesystem::path(physicalBlockDev).filename().string();

    /* If containerName arg wasn't provided, create one based on deviceName. */
    if (containerBlockDev.empty())
    {
        containerBlockDev = "luks-" + deviceName;
    }

    /* DBus path location to place the object. */
    std::string path = "/xyz/openbmc_project/storage/" + deviceName;

    /*
     * Create a new bus and affix an object manager for the subtree path we
     * intend to place objects at.
     */
    auto b = sdbusplus::bus::new_default();
    sdbusplus::server::manager_t m{b, path.c_str()};

    /* Reserve the dbus service name. */
    std::string busName = "xyz.openbmc_project.eStoraged." + deviceName;
    b.request_name(busName.c_str());

    /* Create an eStoraged object. */
    estoraged::eStoraged esObject{b, path.c_str(), physicalBlockDev,
                                  containerBlockDev};
    std::string msg = "OpenBMC.1.0.ServiceStarted";
    lg2::info("Storage management service is running", "REDFISH_MESSAGE_ID",
              msg);

    while (true)
    {
        b.wait();
        b.process_discard();
    }

    return 1;
}
