
#include "estoraged.hpp"

#include <sdbusplus/bus.hpp>

#include <iostream>

int main()
{
    /* DBus path location to place the object(s) */
    constexpr auto path = "/xyz/openbmc_project/storage/dev_name";

    // Create a new bus and affix an object manager for the subtree path we
    // intend to place objects at..
    auto b = sdbusplus::bus::new_default();
    sdbusplus::server::manager_t m{b, path};

    // Reserve the dbus service name
    b.request_name("xyz.openbmc_project.eStoraged.dev_name");

    // Create an eStoraged object
    openbmc::eStoraged es_object{b, path};

    std::cerr << "eStoraged has started" << std::endl;

    while (true)
    {
        b.wait();
        b.process_discard();
    }

    return 1;
}
