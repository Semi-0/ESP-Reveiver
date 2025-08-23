#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "unity.h"

// Include the event bus and flow graph headers
#include "main/eventbus/TinyEventBus.h"
#include "main/eventbus/FlowGraph.h"
#include "main/eventbus/EventProtocol.h"

static const char* TAG = "TINY_EVENTBUS_TEST";

// Mock worker functions for testing
static bool mock_mdns_query_worker(void** out) {
    ESP_LOGI(TAG, "Mock mDNS worker: Creating test IP");
    char* ip = strdup("192.168.1.100");
    ESP_LOGI(TAG, "Mock mDNS worker: Created IP '%s' at ptr %p", ip, ip);
    *out = ip;
    return true;
}

static bool mock_mqtt_connection_worker_with_event(const Event& trigger_event, void** out) {
    const char* host = static_cast<const char*>(trigger_event.ptr);
    ESP_LOGI(TAG, "Mock MQTT worker: Received hostname '%s' at ptr %p", 
             host ? host : "null", trigger_event.ptr);
    
    // Check for corruption
    if (host && strcmp(host, "192.168.1.100") != 0) {
        ESP_LOGE(TAG, "CORRUPTION DETECTED! Expected '192.168.1.100', got '%s'", host);
        TEST_FAIL_MESSAGE("String corruption detected in MQTT worker");
    }
    
    *out = nullptr;
    return true;
}

// Test the exact production flow
static void test_production_flow(void) {
    ESP_LOGI(TAG, "=== Testing Production Flow ===");
    
    // Initialize event bus
    TinyEventBus bus;
    TEST_ASSERT_TRUE(bus.begin("test-event-dispatch", 4096, 5));
    
    // Initialize FlowGraph
    FlowGraph flow_graph(bus);
    
    // Set up event counters
    static int wifi_connected_count = 0;
    static int mdns_found_count = 0;
    static int mqtt_connected_count = 0;
    static int corruption_count = 0;
    
    // Reset counters
    wifi_connected_count = 0;
    mdns_found_count = 0;
    mqtt_connected_count = 0;
    corruption_count = 0;
    
    // Set up the production flow
    flow_graph.when(TOPIC_WIFI_CONNECTED,
        flow_graph.async_blocking("test-mdns-query", mock_mdns_query_worker,
            // onOk: publish MDNS_FOUND with hostname
            [](const Event& e, IEventBus& bus) {
                const char* hostname = static_cast<const char*>(e.ptr);
                ESP_LOGI(TAG, "MDNS success handler: Found hostname '%s' at ptr %p", 
                         hostname ? hostname : "null", e.ptr);
                
                // Check for corruption
                if (hostname && strcmp(hostname, "192.168.1.100") != 0) {
                    ESP_LOGE(TAG, "CORRUPTION DETECTED! Expected '192.168.1.100', got '%s'", hostname);
                    corruption_count++;
                }
                
                char* hostname_copy = hostname ? strdup(hostname) : nullptr;
                ESP_LOGI(TAG, "MDNS success handler: Publishing with hostname '%s'", 
                         hostname_copy ? hostname_copy : "null");
                
                Event event{TOPIC_MDNS_FOUND, 0, hostname_copy, free};
                bus.publish(event);
                mdns_found_count++;
            },
            // onErr: publish MDNS_FAILED
            [](const Event& e, IEventBus& bus) {
                ESP_LOGI(TAG, "MDNS error handler called");
                bus.publish(Event{TOPIC_MDNS_FAILED, 0, nullptr});
            },
            4096, 5
        )
    );
    
    // Set up MQTT connection flow
    flow_graph.when(TOPIC_MDNS_FOUND,
        flow_graph.async_blocking_with_event("test-mqtt-connect", mock_mqtt_connection_worker_with_event,
            // onOk: publish MQTT_CONNECTED
            [](const Event& e, IEventBus& bus) {
                ESP_LOGI(TAG, "MQTT success handler called");
                bus.publish(Event{TOPIC_MQTT_CONNECTED, 0, nullptr});
                mqtt_connected_count++;
            },
            // onErr: publish MQTT_CONNECT_FAILED
            [](const Event& e, IEventBus& bus) {
                ESP_LOGI(TAG, "MQTT error handler called");
                bus.publish(Event{TOPIC_MQTT_CONNECT_FAILED, 0, nullptr});
            },
            4096, 5
        )
    );
    
    // Set up event logging
    bus.subscribe([](const Event& e, void* user) {
        ESP_LOGI(TAG, "Event received: type=%d, i32=%d, ptr=%p", e.type, e.i32, e.ptr);
        if (e.type == TOPIC_WIFI_CONNECTED) wifi_connected_count++;
    }, nullptr);
    
    // Trigger the flow
    ESP_LOGI(TAG, "Triggering WiFi connected event");
    bus.publish(Event{TOPIC_WIFI_CONNECTED, 0, nullptr});
    
    // Wait for the flow to complete
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Check results
    ESP_LOGI(TAG, "Test results:");
    ESP_LOGI(TAG, "  WiFi connected events: %d", wifi_connected_count);
    ESP_LOGI(TAG, "  MDNS found events: %d", mdns_found_count);
    ESP_LOGI(TAG, "  MQTT connected events: %d", mqtt_connected_count);
    ESP_LOGI(TAG, "  Corruption count: %d", corruption_count);
    
    // Verify the flow completed successfully
    TEST_ASSERT_EQUAL(1, wifi_connected_count);
    TEST_ASSERT_EQUAL(1, mdns_found_count);
    TEST_ASSERT_EQUAL(1, mqtt_connected_count);
    TEST_ASSERT_EQUAL(0, corruption_count);
    
    ESP_LOGI(TAG, "=== Production Flow Test PASSED ===");
}

// Test the retry flow specifically
static void test_retry_flow(void) {
    ESP_LOGI(TAG, "=== Testing Retry Flow ===");
    
    // Initialize event bus
    TinyEventBus bus;
    TEST_ASSERT_TRUE(bus.begin("test-retry-dispatch", 4096, 5));
    
    // Initialize FlowGraph
    FlowGraph flow_graph(bus);
    
    // Set up event counters
    static int retry_resolve_count = 0;
    static int mdns_found_count = 0;
    static int mqtt_connected_count = 0;
    static int corruption_count = 0;
    
    // Reset counters
    retry_resolve_count = 0;
    mdns_found_count = 0;
    mqtt_connected_count = 0;
    corruption_count = 0;
    
    // Flow 3: Retry driver – when asked to retry, run resolve again
    flow_graph.when(TOPIC_RETRY_RESOLVE,
        flow_graph.async_blocking("test-mdns-query", mock_mdns_query_worker,
            [](const Event& e, IEventBus& bus) {
                const char* hostname = static_cast<const char*>(e.ptr);
                ESP_LOGI(TAG, "Flow 3 - Found hostname: %s", hostname ? hostname : "null");
                
                // Check for corruption
                if (hostname && strcmp(hostname, "192.168.1.100") != 0) {
                    ESP_LOGE(TAG, "CORRUPTION DETECTED! Expected '192.168.1.100', got '%s'", hostname);
                    corruption_count++;
                }
                
                char* hostname_copy = hostname ? strdup(hostname) : nullptr;
                ESP_LOGI(TAG, "Flow 3 - Publishing TOPIC_MDNS_FOUND with hostname: %s", 
                         hostname_copy ? hostname_copy : "null");
                bus.publish(Event{TOPIC_MDNS_FOUND, 0, hostname_copy, free});
            },
            [](const Event& e, IEventBus& bus) {
                ESP_LOGI(TAG, "Flow 3 - MDNS failed");
                bus.publish(Event{TOPIC_MDNS_FAILED, 0, nullptr});
            },
            4096, 5
        )
    );

    // Flow 4: mDNS success – cache IP, reset backoff, then connect
    flow_graph.when(TOPIC_MDNS_FOUND,
        flow_graph.seq(
            flow_graph.tap([](const Event& e) {
                const char* ip = static_cast<const char*>(e.ptr);
                ESP_LOGI(TAG, "Flow 4 - Caching IP: %s", ip ? ip : "null");
                
                // Check for corruption
                if (ip && strcmp(ip, "192.168.1.100") != 0) {
                    ESP_LOGE(TAG, "CORRUPTION DETECTED! Expected '192.168.1.100', got '%s'", ip);
                    corruption_count++;
                }
            }),
            flow_graph.async_blocking_with_event("test-mqtt-connect", mock_mqtt_connection_worker_with_event,
                [](const Event& e, IEventBus& bus) {
                    ESP_LOGI(TAG, "Flow 4 - MQTT connected");
                    bus.publish(Event{TOPIC_MQTT_CONNECTED, 0, nullptr});
                    mqtt_connected_count++;
                },
                flow_graph.seq(
                    [](const Event& e, IEventBus& bus) {
                        ESP_LOGI(TAG, "Flow 4 - MQTT connect failed");
                        bus.publish(Event{TOPIC_MQTT_CONNECT_FAILED, 0, nullptr});
                    },
                    [](const Event& e, IEventBus& bus) {
                        ESP_LOGI(TAG, "Flow 4 - System error");
                        bus.publish(Event{TOPIC_SYSTEM_ERROR, 6, nullptr});
                    }
                ),
                4096, 5
            )
        )
    );
    
    // Set up event logging
    bus.subscribe([](const Event& e, void* user) {
        ESP_LOGI(TAG, "Event received: type=%d, i32=%d, ptr=%p", e.type, e.i32, e.ptr);
        if (e.type == TOPIC_RETRY_RESOLVE) retry_resolve_count++;
        if (e.type == TOPIC_MDNS_FOUND) mdns_found_count++;
    }, nullptr);
    
    // Trigger the retry flow
    ESP_LOGI(TAG, "Triggering retry resolve flow");
    bus.publish(Event{TOPIC_RETRY_RESOLVE, 0, nullptr});
    
    // Wait for the flow to complete
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Check results
    ESP_LOGI(TAG, "Retry flow test results:");
    ESP_LOGI(TAG, "  Retry resolve events: %d", retry_resolve_count);
    ESP_LOGI(TAG, "  MDNS found events: %d", mdns_found_count);
    ESP_LOGI(TAG, "  MQTT connected events: %d", mqtt_connected_count);
    ESP_LOGI(TAG, "  Corruption count: %d", corruption_count);
    
    // Verify the flow completed successfully
    TEST_ASSERT_EQUAL(1, retry_resolve_count);
    TEST_ASSERT_EQUAL(1, mdns_found_count);
    TEST_ASSERT_EQUAL(1, mqtt_connected_count);
    TEST_ASSERT_EQUAL(0, corruption_count);
    
    ESP_LOGI(TAG, "=== Retry Flow Test PASSED ===");
}

// Test memory corruption specifically
static void test_memory_corruption(void) {
    ESP_LOGI(TAG, "=== Testing Memory Corruption ===");
    
    // Initialize event bus
    TinyEventBus bus;
    TEST_ASSERT_TRUE(bus.begin("test-corruption-dispatch", 4096, 5));
    
    // Initialize FlowGraph
    FlowGraph flow_graph(bus);
    
    // Set up corruption detection
    static bool corruption_detected = false;
    corruption_detected = false;
    
    // Set up a simple flow that passes string data
    flow_graph.when(TOPIC_WIFI_CONNECTED,
        flow_graph.async_blocking("test-string-passing", 
            [](void** out) -> bool {
                // Create a test string
                char* test_string = strdup("192.168.1.100");
                ESP_LOGI(TAG, "Worker: Created string '%s' at ptr %p", test_string, test_string);
                *out = test_string;
                return true;
            },
            [](const Event& e, IEventBus& bus) {
                const char* received_string = static_cast<const char*>(e.ptr);
                ESP_LOGI(TAG, "Handler: Received string '%s' at ptr %p", 
                         received_string ? received_string : "null", e.ptr);
                
                // Check if the string is corrupted
                if (received_string && strcmp(received_string, "192.168.1.100") != 0) {
                    ESP_LOGE(TAG, "CORRUPTION DETECTED! Expected '192.168.1.100', got '%s'", received_string);
                    corruption_detected = true;
                    TEST_FAIL_MESSAGE("String corruption detected");
                }
                
                // Pass it to the next handler
                char* copy = received_string ? strdup(received_string) : nullptr;
                Event next_event{TOPIC_MDNS_FOUND, 0, copy, free};
                bus.publish(next_event);
            },
            [](const Event& e, IEventBus& bus) {
                ESP_LOGI(TAG, "Error handler called");
            },
            4096, 5
        )
    );
    
    // Set up the next handler to check for corruption
    flow_graph.when(TOPIC_MDNS_FOUND,
        [](const Event& e, IEventBus& bus) {
            const char* received_string = static_cast<const char*>(e.ptr);
            ESP_LOGI(TAG, "Final handler: Received string '%s' at ptr %p", 
                     received_string ? received_string : "null", e.ptr);
            
            // Check if the string is corrupted
            if (received_string && strcmp(received_string, "192.168.1.100") != 0) {
                ESP_LOGE(TAG, "CORRUPTION DETECTED in final handler! Expected '192.168.1.100', got '%s'", received_string);
                corruption_detected = true;
                TEST_FAIL_MESSAGE("String corruption detected in final handler");
            }
        }
    );
    
    // Trigger the flow
    ESP_LOGI(TAG, "Starting memory corruption test");
    bus.publish(Event{TOPIC_WIFI_CONNECTED, 0, nullptr});
    
    // Wait for completion
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Check for corruption
    TEST_ASSERT_FALSE(corruption_detected);
    
    ESP_LOGI(TAG, "=== Memory Corruption Test PASSED ===");
}

// Main test runner
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== TinyEventBus Memory Corruption Tests ===");
    
    // Wait a bit for system to stabilize
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Run the tests
    UNITY_BEGIN();
    
    RUN_TEST(test_memory_corruption);
    RUN_TEST(test_production_flow);
    RUN_TEST(test_retry_flow);
    
    UNITY_END();
    
    ESP_LOGI(TAG, "=== All Tests Completed ===");
}
