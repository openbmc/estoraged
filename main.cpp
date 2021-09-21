#include "estoraged.hpp"

#include <systemd/sd-journal.h>
#include <sdbusplus/bus.hpp>

#include <filesystem>
#include <string>
#include <iostream>


static void usage(std::string_view name)
{
    std::cerr
        << "Usage: " << name
        << "eStorageD service on the BMC\n\n"
           "  -b <blockDevice>          The phyical encrypted device\n"
           "                            If omitted, it will default to /dev/mmcblk0.\n"
           "  -c <Container>            The unencryped device that will be created\n"
           "                            If omitted, it will default to /dev/mapper/emmc0";
}


int main(int argc, char** argv)
{

    std::string physicalBlockDev = "/dev/mmcblk0";
    std::string containerBlockDev = "/dev/mapper/emmc0";
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

    /* DBus path location to place the object(s) */
    std::string path("/xyz/openbmc_project/encrypted_storage/");
    path.append(deviceName);

    // Create a new bus and affix an object manager for the subtree path we
    // intend to place objects at..
    auto b = sdbusplus::bus::new_default();
    sdbusplus::server::manager_t m{b, path.c_str()};

    // Reserve the dbus service name
    std::string busName("xyz.openbmc_project.eStoraged.");
    busName.append(deviceName);
    b.request_name(busName.c_str());

    // Create an eStoraged object
    openbmc::eStoraged es_object{b, path.c_str(), physicalBlockDev,
      containerBlockDev};

    // Redfish MessageIds are in the form
    // RegistryName.MajorVersion.MinorVersion.MessageKey
    sd_journal_send("MESSAGE=%s", "eStorageD has started",
                    "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.Started",
                    "REDFISH_MESSAGE_ARGS=%s,%s",
                    physicalBlockDev, containerBlockDev, NULL);
    while (true)
    {
        b.wait();
        b.process_discard();
    }

    return 1;
}
