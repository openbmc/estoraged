#include "getConfig.hpp"

#include <systemd/sd-bus.h>
#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
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
constexpr const char* kOtherConfigPath =
    "/xyz/openbmc_project/inventory/system/board/AgoraV2ND/other_emmc";

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

    sdbusplus::message_t makePropertiesChangedSignal(const char* path)
    {
        auto msg = serverConn->new_signal(
            path, "org.freedesktop.DBus.Properties", "PropertiesChanged");
        estoraged::StorageData changed;
        changed.emplace("Type", estoraged::BasicVariantType("EmmcDevice"));
        std::vector<std::string> invalidated;
        msg.append(std::string(estoraged::emmcConfigInterface), changed,
                   invalidated);
        return sealMessage(std::move(msg));
    }

    sdbusplus::message_t makeEmmcInterfacesAddedSignal(const char* path)
    {
        auto added = serverConn->new_signal(
            kInventoryRoot, "org.freedesktop.DBus.ObjectManager",
            "InterfacesAdded");
        boost::container::flat_map<std::string, estoraged::StorageData> ifaces;
        estoraged::StorageData emmcData;
        emmcData.emplace("Type", estoraged::BasicVariantType("EmmcDevice"));
        ifaces.emplace(estoraged::emmcConfigInterface, emmcData);
        added.append(estoraged::object_path(path), ifaces);
        return sealMessage(std::move(added));
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

TEST_F(GetConfigTest, InterfacesAddedCreatesImmediatelyAndCancelsRetries)
{
    size_t getSubTreeCalls = 0;
    auto mapperIface =
        objectServer->add_interface(mapperConfig.path, mapperConfig.interface);
    mapperIface->register_method(
        "GetSubTree",
        [&getSubTreeCalls](const std::string&, int32_t,
                           const std::vector<std::string>&) -> GetSubTreeType {
            ++getSubTreeCalls;
            throw InternalFailure();
        });
    mapperIface->initialize();

    mapperConfig.retryInterval = std::chrono::milliseconds(150);
    mapperConfig.maxRetries = 5;

    boost::container::flat_map<std::string, estoraged::StorageData> created;
    estoraged::StorageConfigScanner scanner(
        clientConn,
        [&created](const std::string& path,
                   const estoraged::StorageData& data) {
            created.emplace(path, data);
            return true;
        },
        [&created](const std::string& path) {
            return created.find(path) != created.end();
        },
        mapperConfig, std::chrono::milliseconds(10));

    /* Start initial scan; GetSubTree fails once and waits on 150 ms retry. */
    scanner.startScan();
    ASSERT_TRUE(runUntil([&]() { return getSubTreeCalls == 1U; }));
    runFor(std::chrono::milliseconds(20));
    EXPECT_EQ(getSubTreeCalls, 1U);
    EXPECT_TRUE(created.empty());

    /* Unrelated InterfacesAdded signals must not trigger scans or creates. */
    for (int i = 0; i < 10; ++i)
    {
        auto unrelated = serverConn->new_signal(
            kInventoryRoot, "org.freedesktop.DBus.ObjectManager",
            "InterfacesAdded");
        boost::container::flat_map<std::string, estoraged::StorageData> ifaces;
        ifaces.emplace("xyz.openbmc_project.Sensor.Value",
                       estoraged::StorageData{});
        unrelated.append(
            estoraged::object_path("/xyz/openbmc_project/inventory/sensor"),
            ifaces);
        auto sealed = sealMessage(std::move(unrelated));
        scanner.handleInterfacesAdded(sealed);
    }
    EXPECT_TRUE(created.empty());

    /* EmmcDevice InterfacesAdded creates immediately and cancels retries. */
    auto sealedAdded = makeEmmcInterfacesAddedSignal(kTestConfigPath);
    scanner.handleInterfacesAdded(sealedAdded);

    ASSERT_EQ(created.size(), 1U);
    EXPECT_TRUE(created.find(kTestConfigPath) != created.end());

    /* Run well past retryInterval to confirm no further GetSubTree retries. */
    runFor(std::chrono::milliseconds(250));
    EXPECT_EQ(getSubTreeCalls, 1U);
}

TEST_F(GetConfigTest, FailedInterfacesAddedDoesNotCancelInFlightScan)
{
    auto emmcIface = registerEmmcConfigObject(kTestConfigPath);
    size_t getSubTreeCalls = 0;
    auto mapperIface =
        objectServer->add_interface(mapperConfig.path, mapperConfig.interface);
    mapperIface->register_method(
        "GetSubTree",
        [this,
         &getSubTreeCalls](const std::string&, int32_t,
                           const std::vector<std::string>&) -> GetSubTreeType {
            ++getSubTreeCalls;
            if (getSubTreeCalls == 1U)
            {
                throw InternalFailure();
            }
            return {{kTestConfigPath,
                     {{emBusName, {estoraged::emmcConfigInterface}}}}};
        });
    mapperIface->initialize();

    mapperConfig.retryInterval = std::chrono::milliseconds(80);
    mapperConfig.maxRetries = 3;

    boost::container::flat_set<std::string> knownPaths;
    boost::container::flat_map<std::string, estoraged::StorageData> created;
    estoraged::StorageConfigScanner scanner(
        clientConn,
        [&created, &knownPaths](const std::string& path,
                                const estoraged::StorageData& data) {
            if (path == kOtherConfigPath)
            {
                /* Simulate placeholder insertion on failed findDevice(). */
                knownPaths.insert(path);
                return false;
            }
            created.emplace(path, data);
            knownPaths.insert(path);
            return true;
        },
        [&knownPaths](const std::string& path) {
            return knownPaths.find(path) != knownPaths.end();
        },
        mapperConfig, std::chrono::milliseconds(10));

    scanner.startScan();
    ASSERT_TRUE(runUntil([&]() { return getSubTreeCalls == 1U; }));

    /*
     * A failing InterfacesAdded that records a known placeholder path must not
     * cancel the in-flight initial scan before it discovers kTestConfigPath.
     */
    auto failedAdded = makeEmmcInterfacesAddedSignal(kOtherConfigPath);
    scanner.handleInterfacesAdded(failedAdded);
    EXPECT_TRUE(created.empty());

    ASSERT_TRUE(runUntil([&]() { return created.size() == 1U; }));
    EXPECT_EQ(getSubTreeCalls, 2U);
    EXPECT_TRUE(created.find(kTestConfigPath) != created.end());
}

TEST_F(GetConfigTest, InterfacesAddedCallbackExceptionIsCaught)
{
    size_t callbackCalls = 0;
    estoraged::StorageConfigScanner scanner(
        clientConn,
        [&callbackCalls](const std::string&,
                         const estoraged::StorageData&) -> bool {
            ++callbackCalls;
            throw InternalFailure();
        },
        [](const std::string&) { return false; }, mapperConfig,
        std::chrono::milliseconds(10));

    auto sealedAdded = makeEmmcInterfacesAddedSignal(kTestConfigPath);
    EXPECT_NO_THROW(scanner.handleInterfacesAdded(sealedAdded));
    EXPECT_EQ(callbackCalls, 1U);
}

TEST_F(GetConfigTest, PropertiesChangedIgnoresKnownPath)
{
    size_t getSubTreeCalls = 0;
    auto mapperIface =
        objectServer->add_interface(mapperConfig.path, mapperConfig.interface);
    mapperIface->register_method(
        "GetSubTree",
        [&getSubTreeCalls](const std::string&, int32_t,
                           const std::vector<std::string>&) -> GetSubTreeType {
            ++getSubTreeCalls;
            return {};
        });
    mapperIface->initialize();

    boost::container::flat_set<std::string> knownPaths{kTestConfigPath};
    estoraged::StorageConfigScanner scanner(
        clientConn,
        [](const std::string&, const estoraged::StorageData&) { return false; },
        [&knownPaths](const std::string& path) {
            return knownPaths.find(path) != knownPaths.end();
        },
        mapperConfig, std::chrono::milliseconds(5));

    auto sig = makePropertiesChangedSignal(kTestConfigPath);
    scanner.handlePropertiesChanged(sig);
    runFor(std::chrono::milliseconds(30));

    EXPECT_EQ(getSubTreeCalls, 0U);
}

TEST_F(GetConfigTest, PropertiesChangedSingleFlightAndMapperRetry)
{
    auto emmcIface = registerEmmcConfigObject(kTestConfigPath);
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

    mapperConfig.retryInterval = std::chrono::milliseconds(40);
    mapperConfig.maxRetries = 5;

    boost::container::flat_map<std::string, estoraged::StorageData> created;
    estoraged::StorageConfigScanner scanner(
        clientConn,
        [&created](const std::string& path,
                   const estoraged::StorageData& data) {
            created.emplace(path, data);
            return true;
        },
        [&created](const std::string& path) {
            return created.find(path) != created.end();
        },
        mapperConfig, std::chrono::milliseconds(5));

    /*
     * Inject repeated PropertiesChanged bursts across multiple debounce windows
     * while the first GetStorageConfiguration retry chain is in flight.
     * Single-flight deduplication must keep exactly one chain active (3 calls
     * total: 2 failures + 1 success) and perform no redundant follow-up scan
     * once kTestConfigPath is created.
     */
    auto firstSig = makePropertiesChangedSignal(kTestConfigPath);
    scanner.handlePropertiesChanged(firstSig);
    ASSERT_TRUE(runUntil([&]() { return getSubTreeCalls >= 1U; }));

    for (int wave = 0; wave < 3; ++wave)
    {
        auto sig = makePropertiesChangedSignal(kTestConfigPath);
        scanner.handlePropertiesChanged(sig);
        runFor(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(runUntil([&]() { return created.size() == 1U; }));
    runFor(std::chrono::milliseconds(60));
    EXPECT_EQ(getSubTreeCalls, 3U);
    ASSERT_EQ(created.size(), 1U);
    EXPECT_EQ(
        std::get<std::string>(created.at(kTestConfigPath).at("LocationCode")),
        "U10");
}

TEST_F(GetConfigTest, GetConfigurationSkipsKnownPathsAndRetriesGetAll)
{
    /* Register only kOtherConfigPath initially; kTestConfigPath is absent. */
    auto knownIface = registerEmmcConfigObject(kOtherConfigPath, "known_emmc");
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
            return {
                {kOtherConfigPath,
                 {{emBusName, {estoraged::emmcConfigInterface}}}},
                {kTestConfigPath,
                 {{emBusName, {estoraged::emmcConfigInterface}}}},
            };
        });
    mapperIface->initialize();

    mapperConfig.retryInterval = std::chrono::milliseconds(100);
    mapperConfig.maxRetries = 3;

    boost::container::flat_set<std::string> knownPaths{kOtherConfigPath};
    boost::container::flat_map<std::string, estoraged::StorageData> created;

    estoraged::StorageConfigScanner scanner(
        clientConn,
        [&created, &knownPaths](const std::string& path,
                                const estoraged::StorageData& data) {
            created.emplace(path, data);
            knownPaths.insert(path);
            return true;
        },
        [&knownPaths](const std::string& path) {
            return knownPaths.find(path) != knownPaths.end();
        },
        mapperConfig, std::chrono::milliseconds(5));

    /*
     * Start scan while kTestConfigPath does not exist yet on serverConn, so
     * Properties.GetAll fails on attempt 1 and schedules a retry.
     */
    scanner.startScan();
    ASSERT_TRUE(runUntil([&]() { return getSubTreeCalls == 1U; }));
    runFor(std::chrono::milliseconds(20));
    EXPECT_TRUE(created.empty());

    /* Publish kTestConfigPath before the retry fires. */
    delayedIface = registerEmmcConfigObject(kTestConfigPath, "delayed_emmc");
    ASSERT_TRUE(runUntil([&]() { return created.size() == 1U; }));

    /* kOtherConfigPath was skipped because it was already known. */
    EXPECT_EQ(getSubTreeCalls, 1U);
    ASSERT_EQ(created.size(), 1U);
    ASSERT_TRUE(created.find(kTestConfigPath) != created.end());
    EXPECT_EQ(std::get<std::string>(created.at(kTestConfigPath).at("Name")),
              "delayed_emmc");
}

} // namespace
} // namespace estoraged_test
