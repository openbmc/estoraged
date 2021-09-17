#include "estoraged.hpp"

#include <systemd/sd-journal.h>
#include <sdbusplus/bus.hpp>
#include <string>

int main()
{
    /* DBus path location to place the object(s) */
    constexpr auto path = "/xyz/openbmc_project/encrypted_storage";

    // Create a new bus and affix an object manager for the subtree path we
    // intend to place objects at..
    auto b = sdbusplus::bus::new_default();
    sdbusplus::server::manager_t m{b, path};

    // Reserve the dbus service name
    b.request_name("xyz.openbmc_project.eStoraged");

    // Create an eStoraged object
    openbmc::eStoraged es_object{b, path};

    std::string message("BMC eStorageD is up");
    std::string redfishMsgId("BMC.estorageD.is.up");

    sd_journal_send("MESSAGE=%s", message.c_str(),
                                "REDFISH_MESSAGE_ID=%s", redfishMsgId.c_str(),
                                NULL);
    while (true)
    {
        b.wait();
        b.process_discard();
    }

    return 1;
}
