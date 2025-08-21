#include "eventbus/TinyEventBus.h"
#include "eventbus/EventProtocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

static const char* TAG = "EVENTBUS_TEST";

// Test fixtures
static TinyEventBus test_bus;
static int test_counter = 0;
static bool test_passed = false;
static bool memory_test_passed = false;

// Test event handler
void test_event_handler(const Event& e, void* user) {
    ESP_LOGI(TAG, "Test event received: type=%d, i32=%d", e.type, e.i32);
    test_counter++;
    
    if (e.type == TOPIC_TIMER && e.i32 == 42) {
        test_passed = true;
        ESP_LOGI(TAG, "Test PASSED: Expected event received");
    }
}

// Test destructor
void test_destructor(void* ptr) {
    int* value = static_cast<int*>(ptr);
    ESP_LOGI(TAG, "Destructor called for value: %d", *value);
    *value = -1; // Mark as freed
    delete value;
    memory_test_passed = true;
}

// Simple test function
void run_basic_test() {
    ESP_LOGI(TAG, "Starting EventBus basic functionality test");
    
    test_counter = 0;
    test_passed = false;
    
    // Start the event bus
    if (!test_bus.begin("test-bus", 2048, tskIDLE_PRIORITY + 1)) {
        ESP_LOGE(TAG, "Failed to start event bus");
        return;
    }
    
    // Subscribe to all events
    auto handle = test_bus.subscribe(test_event_handler, nullptr, MASK_ALL);
    if (handle < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to events");
        return;
    }
    
    // Publish some test events
    test_bus.publish(Event{TOPIC_TIMER, 1, nullptr});
    test_bus.publish(Event{TOPIC_TIMER, 42, nullptr});
    test_bus.publish(Event{TOPIC_WIFI_CONNECTED, 3, nullptr});
    
    // Give some time for processing
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Check results
    if (test_counter >= 3 && test_passed) {
        ESP_LOGI(TAG, "✓ Basic functionality test PASSED");
    } else {
        ESP_LOGE(TAG, "✗ Basic functionality test FAILED");
    }
    
    // Unsubscribe
    test_bus.unsubscribe(handle);
}

// Memory management test
void run_memory_test() {
    ESP_LOGI(TAG, "Starting EventBus memory management test");
    
    test_counter = 0;
    memory_test_passed = false;
    
    // Start the event bus
    if (!test_bus.begin("test-bus", 2048, tskIDLE_PRIORITY + 1)) {
        ESP_LOGE(TAG, "Failed to start event bus");
        return;
    }
    
    auto handle = test_bus.subscribe(test_event_handler, nullptr, MASK_ALL);
    if (handle < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to events");
        return;
    }
    
    // Create an event with a destructor
    int* test_value = new int(123);
    Event test_event{TOPIC_TIMER, 100, test_value, test_destructor};
    
    // Publish the event
    test_bus.publish(test_event);
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Check results
    if (test_counter == 1 && memory_test_passed && *test_value == -1) {
        ESP_LOGI(TAG, "✓ Memory management test PASSED");
    } else {
        ESP_LOGE(TAG, "✗ Memory management test FAILED");
    }
    
    test_bus.unsubscribe(handle);
}

// Mailbox test
void run_mailbox_test() {
    ESP_LOGI(TAG, "Starting TinyMailbox test");
    
    TinyMailbox mailbox;
    Event received_event;
    
    // Publish first event
    mailbox.publish(Event{TOPIC_TIMER, 1, nullptr});
    if (!mailbox.hasEvent()) {
        ESP_LOGE(TAG, "✗ Mailbox test FAILED: First event not stored");
        return;
    }
    
    // Publish second event (should overwrite first)
    mailbox.publish(Event{TOPIC_TIMER, 2, nullptr});
    if (!mailbox.hasEvent()) {
        ESP_LOGE(TAG, "✗ Mailbox test FAILED: Second event not stored");
        return;
    }
    
    // Receive the event
    if (!mailbox.receive(received_event) || received_event.i32 != 2) {
        ESP_LOGE(TAG, "✗ Mailbox test FAILED: Wrong event received");
        return;
    }
    
    // Should be empty now
    if (mailbox.hasEvent() || mailbox.receive(received_event)) {
        ESP_LOGE(TAG, "✗ Mailbox test FAILED: Mailbox not empty after receive");
        return;
    }
    
    ESP_LOGI(TAG, "✓ Mailbox test PASSED");
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== EventBus Implementation Test Starting ===");
    ESP_LOGI(TAG, "ESP32 Chip ID: %s", esp_get_idf_version());
    
    // Wait a bit for system to stabilize
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Run all tests
    run_basic_test();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    run_memory_test();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    run_mailbox_test();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "=== EventBus Implementation Test Complete ===");
    ESP_LOGI(TAG, "Check the logs above for test results.");
    
    // Keep the system running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "EventBus test system still running...");
    }
}
