
#include "estoraged.hpp"

#include <sdbusplus/bus.hpp>
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
    while ((opt = getopt(argc, argv, "p:h:")) != -1)
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

    /* DBus path location to place the object(s) */
    constexpr auto path = "/xyz/openbmc_project/storage/dev_name";

    // Create a new bus and affix an object manager for the subtree path we
    // intend to place objects at..
    auto b = sdbusplus::bus::new_default();
    sdbusplus::server::manager_t m{b, path};

    // Reserve the dbus service name
    b.request_name("xyz.openbmc_project.eStoraged.dev_name");

    // Create an eStoraged object
    estoraged::eStoraged es_object{b, path, physicalBlockDev, containerBlockDev};

    std::cerr << "eStoraged has started" << std::endl;

    while (true)
    {
        b.wait();
        b.process_discard();
    }

    return 1;
}
