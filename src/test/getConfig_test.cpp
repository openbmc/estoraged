#include "getConfig.hpp"

#include <systemd/sd-bus.h>
#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/message.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

namespace estoraged_test
{

namespace
{

using Association = std::tuple<std::string, std::string, std::string>;
using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

constexpr const char* kInventoryRoot = "/xyz/openbmc_project/inventory";
constexpr const char* kTestConfigPath =
    "/xyz/openbmc_project/inventory/system/board/AgoraV2ND/agora_emmc";

sdbusplus::message_t sealMessage(sdbusplus::message_t&& msg)
{
    int rc = sd_bus_message_seal(msg.get(), 1, 0);
    if (rc < 0)
    {
        throw sdbusplus::exception::SdBusError(-rc, "sd_bus_message_seal");
    }
    return std::move(msg);
}

class GetConfigTest : public ::testing::Test
{
  protected:
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> clientConn;
    std::shared_ptr<sdbusplus::asio::connection> serverConn;
    std::unique_ptr<sdbusplus::asio::object_server> objectServer;
    std::string mapperBusName;
    std::string emBusName;
    estoraged::MapperConfig mapperConfig;

    void SetUp() override
    {
        static size_t instanceCounter = 0;
        ++instanceCounter;
        const std::string suffix =
            std::to_string(getpid()) + "_" + std::to_string(instanceCounter);
        mapperBusName = "xyz.openbmc_project.ObjectMapper.test" + suffix;
        emBusName = "xyz.openbmc_project.EntityManager.test" + suffix;

        clientConn = std::make_shared<sdbusplus::asio::connection>(
            io, sdbusplus::bus::new_bus());
        serverConn = std::make_shared<sdbusplus::asio::connection>(
            io, sdbusplus::bus::new_bus());
        serverConn->request_name(mapperBusName.c_str());
        serverConn->request_name(emBusName.c_str());
        objectServer =
            std::make_unique<sdbusplus::asio::object_server>(serverConn);

        mapperConfig.busName = mapperBusName;
        mapperConfig.retryInterval = std::chrono::milliseconds(10);
        mapperConfig.maxRetries = 3;
    }

    void runFor(std::chrono::milliseconds duration)
    {
        io.restart();
        boost::asio::steady_timer stopTimer(io);
        stopTimer.expires_after(duration);
        stopTimer.async_wait([this](const boost::system::error_code& ec) {
            if (!ec)
            {
                io.stop();
            }
        });
        io.run();
    }

    template <typename Predicate>
    bool runUntil(Predicate&& pred,
                  std::chrono::milliseconds timeout = std::chrono::seconds(5))
    {
        if (pred())
        {
            return true;
        }
        io.restart();
        boost::asio::steady_timer stopTimer(io);
        stopTimer.expires_after(timeout);
        stopTimer.async_wait([this](const boost::system::error_code& ec) {
            if (!ec)
            {
                io.stop();
            }
        });
        while (!pred() && !io.stopped())
        {
            if (io.run_one() == 0U)
            {
                break;
            }
        }
        stopTimer.cancel();
        return pred();
    }

    std::shared_ptr<sdbusplus::asio::dbus_interface> registerEmmcConfigObject(
        const std::string& path, const std::string& name = "agora_emmc")
    {
        auto iface =
            objectServer->add_interface(path, estoraged::emmcConfigInterface);
        iface->register_property("Type", std::string("EmmcDevice"));
        iface->register_property("Name", name);
        iface->register_property("LocationCode", std::string("U10"));
        iface->register_property("EraseMaxGeometry", uint64_t{8192});
        iface->initialize();
        return iface;
    }
};

TEST_F(GetConfigTest, ExtractEmmcConfigIgnoresUnrelatedInterfaces)
{
    auto msg = serverConn->new_signal(kInventoryRoot,
                                      "org.freedesktop.DBus.ObjectManager",
                                      "InterfacesAdded");
    boost::container::flat_map<std::string, estoraged::StorageData> interfaces;
    estoraged::StorageData boardProps;
    boardProps.emplace("Name", estoraged::BasicVariantType("AgoraV2ND"));
    interfaces.emplace("xyz.openbmc_project.Inventory.Item.Board", boardProps);
    msg.append(estoraged::object_path(
                   "/xyz/openbmc_project/inventory/system/board/AgoraV2ND"),
               interfaces);

    auto sealed = sealMessage(std::move(msg));
    EXPECT_EQ(estoraged::extractEmmcConfig(sealed), std::nullopt);
}

TEST_F(GetConfigTest, ExtractEmmcConfigSkipsComplexSiblingTypes)
{
    using ComplexVariant =
        std::variant<std::vector<Association>, std::string, uint64_t>;
    using ComplexPropMap =
        boost::container::flat_map<std::string, ComplexVariant>;

    auto msg = serverConn->new_signal(kInventoryRoot,
                                      "org.freedesktop.DBus.ObjectManager",
                                      "InterfacesAdded");
    boost::container::flat_map<std::string, ComplexPropMap> interfaces;

    ComplexPropMap assocProps;
    assocProps.emplace(
        "Associations",
        std::vector<Association>{{"chassis", "storage", "/xyz/board"}});
    interfaces.emplace("xyz.openbmc_project.Association.Definitions",
                       assocProps);

    ComplexPropMap emmcProps;
    emmcProps.emplace("Type", std::string("EmmcDevice"));
    emmcProps.emplace("Name", std::string("agora_emmc"));
    emmcProps.emplace("EraseMaxGeometry", uint64_t{16384});
    interfaces.emplace(estoraged::emmcConfigInterface, emmcProps);

    msg.append(estoraged::object_path(kTestConfigPath), interfaces);
    auto sealed = sealMessage(std::move(msg));

    auto extracted = estoraged::extractEmmcConfig(sealed);
    ASSERT_TRUE(extracted.has_value());
    EXPECT_EQ(extracted->first, kTestConfigPath);
    EXPECT_EQ(std::get<std::string>(extracted->second.at("Type")),
              "EmmcDevice");
    EXPECT_EQ(std::get<std::string>(extracted->second.at("Name")),
              "agora_emmc");
    EXPECT_EQ(std::get<uint64_t>(extracted->second.at("EraseMaxGeometry")),
              16384U);
}

TEST_F(GetConfigTest, ExtractEmmcConfigHandlesMalformedMessage)
{
    auto msg = serverConn->new_signal(kInventoryRoot,
                                      "org.freedesktop.DBus.ObjectManager",
                                      "InterfacesAdded");
    auto sealed = sealMessage(std::move(msg));
    EXPECT_EQ(estoraged::extractEmmcConfig(sealed), std::nullopt);
}

TEST_F(GetConfigTest, GetConfigurationRetriesMapperAndGetAll)
{
    std::shared_ptr<sdbusplus::asio::dbus_interface> delayedIface;
    size_t getSubTreeCalls = 0;

    auto mapperIface =
        objectServer->add_interface(mapperConfig.path, mapperConfig.interface);
    mapperIface->register_method(
        "GetSubTree",
        [this,
         &getSubTreeCalls](const std::string&, int32_t,
                           const std::vector<std::string>&) -> GetSubTreeType {
            ++getSubTreeCalls;
            if (getSubTreeCalls < 3U)
            {
                throw InternalFailure();
            }
            return {{kTestConfigPath,
                     {{emBusName, {estoraged::emmcConfigInterface}}}}};
        });
    mapperIface->initialize();

    mapperConfig.retryInterval = std::chrono::milliseconds(80);
    mapperConfig.maxRetries = 4;

    estoraged::ManagedStorageType discovered;
    bool completed = false;
    {
        auto getter = std::make_shared<estoraged::GetStorageConfiguration>(
            clientConn,
            [&discovered, &completed](estoraged::ManagedStorageType& resp) {
                discovered = std::move(resp);
                completed = true;
            },
            mapperConfig);
        getter->getConfiguration();
    }

    /*
     * Wait until GetSubTree succeeds on attempt 3 while kTestConfigPath is not
     * registered yet, causing the first Properties.GetAll call to fail and arm
     * a retry.
     */
    ASSERT_TRUE(runUntil([&]() { return getSubTreeCalls == 3U; }));
    runFor(std::chrono::milliseconds(20));
    EXPECT_FALSE(completed);

    delayedIface = registerEmmcConfigObject(kTestConfigPath, "delayed_emmc");
    ASSERT_TRUE(runUntil([&]() { return completed; }));

    EXPECT_EQ(getSubTreeCalls, 3U);
    ASSERT_EQ(discovered.size(), 1U);
    const estoraged::object_path expectedPath(kTestConfigPath);
    ASSERT_TRUE(discovered.find(expectedPath) != discovered.end());
    EXPECT_EQ(std::get<std::string>(discovered.at(expectedPath).at("Name")),
              "delayed_emmc");
}

} // namespace
} // namespace estoraged_test
