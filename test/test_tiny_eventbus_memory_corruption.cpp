#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>

// Include the event bus and flow graph headers
#include "main/eventbus/TinyEventBus.h"
#include "main/eventbus/FlowGraph.h"
#include "main/eventbus/EventProtocol.h"

// Mock worker functions for testing
static bool mock_mdns_query_worker(void** out) {
    // Simulate finding an IP address
    char* ip = strdup("192.168.1.100");
    *out = ip;
    return true;
}

static bool mock_mqtt_connection_worker_with_event(const Event& trigger_event, void** out) {
    const char* host = static_cast<const char*>(trigger_event.ptr);
    printf("MQTT worker received hostname: %s\n", host ? host : "null");
    
    // Simulate successful connection
    *out = nullptr;
    return true;
}

// Test fixture for TinyEventBus
class TinyEventBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize the event bus
        ASSERT_TRUE(bus.begin("test-event-dispatch", 4096, 5));
        
        // Set up logging handler to capture events
        log_events.clear();
        bus.subscribe([](const Event& e, void* user) {
            auto* events = static_cast<std::vector<std::string>*>(user);
            std::string event_str = "Event{type=" + std::to_string(e.type) + 
                                   ", i32=" + std::to_string(e.i32) + 
                                   ", ptr=" + std::to_string(reinterpret_cast<uintptr_t>(e.ptr)) + "}";
            events->push_back(event_str);
        }, &log_events);
    }
    
    void TearDown() override {
        // Clean up
        log_events.clear();
    }
    
    TinyEventBus bus;
    std::vector<std::string> log_events;
};

// Test basic event publishing and receiving
TEST_F(TinyEventBusTest, BasicEventPublishing) {
    // Publish a simple event
    Event test_event{TOPIC_WIFI_CONNECTED, 42, nullptr};
    bus.publish(test_event);
    
    // Give some time for the event to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check that the event was received
    ASSERT_FALSE(log_events.empty());
    EXPECT_EQ(log_events[0], "Event{type=1, i32=42, ptr=0}");
}

// Test string data passing through events
TEST_F(TinyEventBusTest, StringDataPassing) {
    // Create a string and publish it
    char* test_string = strdup("test-hostname.local");
    Event test_event{TOPIC_MDNS_FOUND, 0, test_string, free};
    
    printf("Publishing event with string: %s at ptr: %p\n", test_string, test_string);
    bus.publish(test_event);
    
    // Give some time for the event to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check that the event was received
    ASSERT_FALSE(log_events.empty());
    printf("Received event: %s\n", log_events[0].c_str());
}

// Test FlowGraph async operations
class FlowGraphTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize the event bus
        ASSERT_TRUE(bus.begin("test-flow-dispatch", 4096, 5));
        
        // Set up logging handler
        log_events.clear();
        bus.subscribe([](const Event& e, void* user) {
            auto* events = static_cast<std::vector<std::string>*>(user);
            std::string event_str = "Event{type=" + std::to_string(e.type) + 
                                   ", i32=" + std::to_string(e.i32) + 
                                   ", ptr=" + std::to_string(reinterpret_cast<uintptr_t>(e.ptr)) + "}";
            events->push_back(event_str);
            printf("Received event: %s\n", event_str.c_str());
        }, &log_events);
        
        // Initialize FlowGraph
        flow_graph = std::make_unique<FlowGraph>(bus);
    }
    
    void TearDown() override {
        log_events.clear();
        flow_graph.reset();
    }
    
    TinyEventBus bus;
    std::unique_ptr<FlowGraph> flow_graph;
    std::vector<std::string> log_events;
};

// Test the exact flow that's failing in production
TEST_F(FlowGraphTest, MDNSQueryToMQTTConnectionFlow) {
    // Set up the flow: WiFi connected -> mDNS query -> MQTT connection
    flow_graph->when(TOPIC_WIFI_CONNECTED,
        flow_graph->async_blocking("test-mdns-query", mock_mdns_query_worker,
            // onOk: publish MDNS_FOUND with hostname
            [](const Event& e, IEventBus& bus) {
                const char* hostname = static_cast<const char*>(e.ptr);
                printf("MDNS success handler - Found hostname: %s\n", hostname ? hostname : "null");
                
                char* hostname_copy = hostname ? strdup(hostname) : nullptr;
                printf("MDNS success handler - About to publish with hostname: %s\n", hostname_copy ? hostname_copy : "null");
                
                Event event{TOPIC_MDNS_FOUND, 0, hostname_copy, free};
                printf("MDNS success handler - Event created with ptr: %p\n", event.ptr);
                bus.publish(event);
                printf("MDNS success handler - Event published\n");
            },
            // onErr: publish MDNS_FAILED
            [](const Event& e, IEventBus& bus) {
                printf("MDNS error handler\n");
                bus.publish(Event{TOPIC_MDNS_FAILED, 0, nullptr});
            },
            4096, 5
        )
    );
    
    // Set up MQTT connection flow
    flow_graph->when(TOPIC_MDNS_FOUND,
        flow_graph->async_blocking_with_event("test-mqtt-connect", mock_mqtt_connection_worker_with_event,
            // onOk: publish MQTT_CONNECTED
            [](const Event& e, IEventBus& bus) {
                printf("MQTT success handler\n");
                bus.publish(Event{TOPIC_MQTT_CONNECTED, 0, nullptr});
            },
            // onErr: publish MQTT_CONNECT_FAILED
            [](const Event& e, IEventBus& bus) {
                printf("MQTT error handler\n");
                bus.publish(Event{TOPIC_MQTT_CONNECT_FAILED, 0, nullptr});
            },
            4096, 5
        )
    );
    
    // Trigger the flow by publishing WiFi connected event
    printf("Triggering WiFi connected event\n");
    bus.publish(Event{TOPIC_WIFI_CONNECTED, 0, nullptr});
    
    // Wait for the flow to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // Check the sequence of events
    printf("Event sequence:\n");
    for (size_t i = 0; i < log_events.size(); ++i) {
        printf("  %zu: %s\n", i, log_events[i].c_str());
    }
    
    // Verify the flow completed successfully
    bool found_wifi_connected = false;
    bool found_mdns_found = false;
    bool found_mqtt_connected = false;
    
    for (const auto& event : log_events) {
        if (event.find("type=1") != std::string::npos) found_wifi_connected = true;
        if (event.find("type=2") != std::string::npos) found_mdns_found = true;
        if (event.find("type=3") != std::string::npos) found_mqtt_connected = true;
    }
    
    EXPECT_TRUE(found_wifi_connected) << "WiFi connected event not found";
    EXPECT_TRUE(found_mdns_found) << "MDNS found event not found";
    EXPECT_TRUE(found_mqtt_connected) << "MQTT connected event not found";
}

// Test memory corruption specifically
TEST_F(FlowGraphTest, MemoryCorruptionTest) {
    // Set up a simple flow that passes string data
    flow_graph->when(TOPIC_WIFI_CONNECTED,
        flow_graph->async_blocking("test-string-passing", 
            [](void** out) -> bool {
                // Create a test string
                char* test_string = strdup("192.168.1.100");
                printf("Worker: Created string '%s' at ptr %p\n", test_string, test_string);
                *out = test_string;
                return true;
            },
            [](const Event& e, IEventBus& bus) {
                const char* received_string = static_cast<const char*>(e.ptr);
                printf("Handler: Received string '%s' at ptr %p\n", 
                       received_string ? received_string : "null", e.ptr);
                
                // Check if the string is corrupted
                if (received_string && strcmp(received_string, "192.168.1.100") != 0) {
                    printf("ERROR: String corruption detected! Expected '192.168.1.100', got '%s'\n", received_string);
                    FAIL() << "String corruption detected";
                }
                
                // Pass it to the next handler
                char* copy = received_string ? strdup(received_string) : nullptr;
                Event next_event{TOPIC_MDNS_FOUND, 0, copy, free};
                bus.publish(next_event);
            },
            [](const Event& e, IEventBus& bus) {
                printf("Error handler called\n");
            },
            4096, 5
        )
    );
    
    // Set up the next handler to check for corruption
    flow_graph->when(TOPIC_MDNS_FOUND,
        [](const Event& e, IEventBus& bus) {
            const char* received_string = static_cast<const char*>(e.ptr);
            printf("Final handler: Received string '%s' at ptr %p\n", 
                   received_string ? received_string : "null", e.ptr);
            
            // Check if the string is corrupted
            if (received_string && strcmp(received_string, "192.168.1.100") != 0) {
                printf("ERROR: String corruption detected in final handler! Expected '192.168.1.100', got '%s'\n", received_string);
                FAIL() << "String corruption detected in final handler";
            }
        }
    );
    
    // Trigger the flow
    printf("Starting memory corruption test\n");
    bus.publish(Event{TOPIC_WIFI_CONNECTED, 0, nullptr});
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    printf("Memory corruption test completed\n");
}

// Test the exact production flow with retry mechanism
TEST_F(FlowGraphTest, ProductionRetryFlow) {
    // Flow 3: Retry driver – when asked to retry, run resolve again
    flow_graph->when(TOPIC_RETRY_RESOLVE,
        flow_graph->async_blocking("test-mdns-query", mock_mdns_query_worker,
            [](const Event& e, IEventBus& bus) {
                const char* hostname = static_cast<const char*>(e.ptr);
                printf("Flow 3 - Found hostname: %s\n", hostname ? hostname : "null");
                char* hostname_copy = hostname ? strdup(hostname) : nullptr;
                printf("Flow 3 - Publishing TOPIC_MDNS_FOUND with hostname: %s\n", hostname_copy ? hostname_copy : "null");
                bus.publish(Event{TOPIC_MDNS_FOUND, 0, hostname_copy, free});
            },
            [](const Event& e, IEventBus& bus) {
                printf("Flow 3 - MDNS failed\n");
                bus.publish(Event{TOPIC_MDNS_FAILED, 0, nullptr});
            },
            4096, 5
        )
    );

    // Flow 4: mDNS success – cache IP, reset backoff, then connect
    flow_graph->when(TOPIC_MDNS_FOUND,
        flow_graph->seq(
            flow_graph->tap([](const Event& e) {
                const char* ip = static_cast<const char*>(e.ptr);
                printf("Flow 4 - Caching IP: %s\n", ip ? ip : "null");
                // Simulate persist_broker_ip_if_any and reset_backoff
            }),
            flow_graph->async_blocking_with_event("test-mqtt-connect", mock_mqtt_connection_worker_with_event,
                [](const Event& e, IEventBus& bus) {
                    printf("Flow 4 - MQTT connected\n");
                    bus.publish(Event{TOPIC_MQTT_CONNECTED, 0, nullptr});
                },
                flow_graph->seq(
                    [](const Event& e, IEventBus& bus) {
                        printf("Flow 4 - MQTT connect failed\n");
                        bus.publish(Event{TOPIC_MQTT_CONNECT_FAILED, 0, nullptr});
                    },
                    [](const Event& e, IEventBus& bus) {
                        printf("Flow 4 - System error\n");
                        bus.publish(Event{TOPIC_SYSTEM_ERROR, 6, nullptr});
                    }
                ),
                4096, 5
            )
        )
    );
    
    // Trigger the retry flow
    printf("Triggering retry resolve flow\n");
    bus.publish(Event{TOPIC_RETRY_RESOLVE, 0, nullptr});
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // Check the event sequence
    printf("Production flow event sequence:\n");
    for (size_t i = 0; i < log_events.size(); ++i) {
        printf("  %zu: %s\n", i, log_events[i].c_str());
    }
    
    // Verify the flow
    bool found_retry_resolve = false;
    bool found_mdns_found = false;
    bool found_mqtt_connected = false;
    
    for (const auto& event : log_events) {
        if (event.find("type=4") != std::string::npos) found_retry_resolve = true;
        if (event.find("type=2") != std::string::npos) found_mdns_found = true;
        if (event.find("type=3") != std::string::npos) found_mqtt_connected = true;
    }
    
    EXPECT_TRUE(found_retry_resolve) << "Retry resolve event not found";
    EXPECT_TRUE(found_mdns_found) << "MDNS found event not found";
    EXPECT_TRUE(found_mqtt_connected) << "MQTT connected event not found";
}

// Test for race conditions and timing issues
TEST_F(FlowGraphTest, RaceConditionTest) {
    std::atomic<int> event_count{0};
    std::atomic<int> corruption_count{0};
    
    // Set up multiple concurrent flows
    for (int i = 0; i < 5; ++i) {
        flow_graph->when(TOPIC_WIFI_CONNECTED,
            flow_graph->async_blocking("concurrent-mdns-" + std::to_string(i), 
                [](void** out) -> bool {
                    char* test_string = strdup("192.168.1.100");
                    *out = test_string;
                    return true;
                },
                [&](const Event& e, IEventBus& bus) {
                    const char* received_string = static_cast<const char*>(e.ptr);
                    event_count++;
                    
                    if (received_string && strcmp(received_string, "192.168.1.100") != 0) {
                        corruption_count++;
                        printf("Corruption detected in concurrent test: expected '192.168.1.100', got '%s'\n", received_string);
                    }
                    
                    char* copy = received_string ? strdup(received_string) : nullptr;
                    bus.publish(Event{TOPIC_MDNS_FOUND, 0, copy, free});
                },
                [](const Event& e, IEventBus& bus) {
                    // Error handler
                },
                2048, 5
            )
        );
    }
    
    // Trigger multiple concurrent events
    for (int i = 0; i < 10; ++i) {
        bus.publish(Event{TOPIC_WIFI_CONNECTED, i, nullptr});
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    
    printf("Race condition test: %d events processed, %d corruptions detected\n", 
           event_count.load(), corruption_count.load());
    
    EXPECT_EQ(corruption_count.load(), 0) << "Memory corruption detected in race condition test";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
