#include <unity.h>
#include "eventbus/TinyEventBus.h"
#include "eventbus/EventProtocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string>
#include <vector>

// Test fixtures
static TinyEventBus test_bus;
static std::vector<int> received_events;
static int test_counter = 0;

// Test event handler
void test_event_handler(const Event& e, void* user) {
    received_events.push_back(e.i32);
    test_counter++;
}

// Test predicate
bool test_predicate(const Event& e, void* user) {
    return e.i32 > 5; // Only accept events with i32 > 5
}

// Test destructor
void test_destructor(void* ptr) {
    int* value = static_cast<int*>(ptr);
    *value = -1; // Mark as freed
    delete value;
}

TEST_CASE("TinyEventBus - Basic subscription and publishing", "[eventbus]") {
    received_events.clear();
    test_counter = 0;
    
    // Start the event bus
    TEST_ASSERT_TRUE(test_bus.begin("test-bus", 2048, tskIDLE_PRIORITY + 1));
    
    // Subscribe to all events
    auto handle = test_bus.subscribe(test_event_handler, nullptr, MASK_ALL);
    TEST_ASSERT_GREATER_OR_EQUAL(0, handle);
    
    // Publish some test events
    test_bus.publish(Event{TOPIC_TIMER, 1, nullptr});
    test_bus.publish(Event{TOPIC_TIMER, 2, nullptr});
    test_bus.publish(Event{TOPIC_TIMER, 3, nullptr});
    
    // Give some time for processing
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Check that events were received
    TEST_ASSERT_EQUAL(3, test_counter);
    TEST_ASSERT_EQUAL(1, received_events[0]);
    TEST_ASSERT_EQUAL(2, received_events[1]);
    TEST_ASSERT_EQUAL(3, received_events[2]);
    
    // Unsubscribe
    test_bus.unsubscribe(handle);
    
    // Publish another event - should not be received
    test_bus.publish(Event{TOPIC_TIMER, 4, nullptr});
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Counter should still be 3
    TEST_ASSERT_EQUAL(3, test_counter);
}

TEST_CASE("TinyEventBus - Topic mask filtering", "[eventbus]") {
    received_events.clear();
    test_counter = 0;
    
    // Subscribe only to TIMER events
    auto handle = test_bus.subscribe(test_event_handler, nullptr, bit(TOPIC_TIMER));
    TEST_ASSERT_GREATER_OR_EQUAL(0, handle);
    
    // Publish different types of events
    test_bus.publish(Event{TOPIC_TIMER, 10, nullptr});
    test_bus.publish(Event{TOPIC_WIFI_CONNECTED, 20, nullptr});
    test_bus.publish(Event{TOPIC_MQTT_CONNECTED, 30, nullptr});
    test_bus.publish(Event{TOPIC_TIMER, 40, nullptr});
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Should only receive TIMER events
    TEST_ASSERT_EQUAL(2, test_counter);
    TEST_ASSERT_EQUAL(10, received_events[0]);
    TEST_ASSERT_EQUAL(40, received_events[1]);
    
    test_bus.unsubscribe(handle);
}

TEST_CASE("TinyEventBus - Event predicate filtering", "[eventbus]") {
    received_events.clear();
    test_counter = 0;
    
    // Subscribe with predicate (only i32 > 5)
    auto handle = test_bus.subscribe(test_event_handler, nullptr, MASK_ALL, test_predicate, nullptr);
    TEST_ASSERT_GREATER_OR_EQUAL(0, handle);
    
    // Publish events with different values
    test_bus.publish(Event{TOPIC_TIMER, 1, nullptr});
    test_bus.publish(Event{TOPIC_TIMER, 10, nullptr});
    test_bus.publish(Event{TOPIC_TIMER, 3, nullptr});
    test_bus.publish(Event{TOPIC_TIMER, 15, nullptr});
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Should only receive events with i32 > 5
    TEST_ASSERT_EQUAL(2, test_counter);
    TEST_ASSERT_EQUAL(10, received_events[0]);
    TEST_ASSERT_EQUAL(15, received_events[1]);
    
    test_bus.unsubscribe(handle);
}

TEST_CASE("TinyEventBus - Destructor cleanup", "[eventbus]") {
    received_events.clear();
    test_counter = 0;
    
    auto handle = test_bus.subscribe(test_event_handler, nullptr, MASK_ALL);
    TEST_ASSERT_GREATER_OR_EQUAL(0, handle);
    
    // Create an event with a destructor
    int* test_value = new int(42);
    Event test_event{TOPIC_TIMER, 100, test_value, test_destructor};
    
    // Publish the event
    test_bus.publish(test_event);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Check that event was received
    TEST_ASSERT_EQUAL(1, test_counter);
    TEST_ASSERT_EQUAL(100, received_events[0]);
    
    // Check that destructor was called (value should be -1)
    TEST_ASSERT_EQUAL(-1, *test_value);
    
    test_bus.unsubscribe(handle);
}

TEST_CASE("TinyMailbox - Latest-only semantics", "[mailbox]") {
    TinyMailbox mailbox;
    Event received_event;
    
    // Publish first event
    mailbox.publish(Event{TOPIC_TIMER, 1, nullptr});
    TEST_ASSERT_TRUE(mailbox.hasEvent());
    
    // Publish second event (should overwrite first)
    mailbox.publish(Event{TOPIC_TIMER, 2, nullptr});
    TEST_ASSERT_TRUE(mailbox.hasEvent());
    
    // Receive the event
    TEST_ASSERT_TRUE(mailbox.receive(received_event));
    TEST_ASSERT_EQUAL(2, received_event.i32);
    
    // Should be empty now
    TEST_ASSERT_FALSE(mailbox.hasEvent());
    TEST_ASSERT_FALSE(mailbox.receive(received_event));
}

TEST_CASE("TinyMailbox - Destructor cleanup", "[mailbox]") {
    TinyMailbox mailbox;
    
    // Create first event with destructor
    int* value1 = new int(10);
    mailbox.publish(Event{TOPIC_TIMER, 1, value1, test_destructor});
    
    // Create second event with destructor (should free first)
    int* value2 = new int(20);
    mailbox.publish(Event{TOPIC_TIMER, 2, value2, test_destructor});
    
    // First value should be freed
    TEST_ASSERT_EQUAL(-1, *value1);
    
    // Receive second event
    Event received_event;
    TEST_ASSERT_TRUE(mailbox.receive(received_event));
    TEST_ASSERT_EQUAL(2, received_event.i32);
    
    // Second value should be freed
    TEST_ASSERT_EQUAL(-1, *value2);
}

TEST_CASE("TinyEventBus - ISR safety (simulated)", "[eventbus]") {
    received_events.clear();
    test_counter = 0;
    
    auto handle = test_bus.subscribe(test_event_handler, nullptr, MASK_ALL);
    TEST_ASSERT_GREATER_OR_EQUAL(0, handle);
    
    // Simulate ISR publishing
    BaseType_t hpw = pdFALSE;
    test_bus.publishFromISR(Event{TOPIC_TIMER, 100, nullptr}, &hpw);
    test_bus.publishFromISR(Event{TOPIC_TIMER, 200, nullptr}, &hpw);
    test_bus.publishFromISR(Event{TOPIC_TIMER, 300, nullptr}, &hpw);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Should receive all events
    TEST_ASSERT_EQUAL(3, test_counter);
    TEST_ASSERT_EQUAL(100, received_events[0]);
    TEST_ASSERT_EQUAL(200, received_events[1]);
    TEST_ASSERT_EQUAL(300, received_events[2]);
    
    test_bus.unsubscribe(handle);
}

// Test queue overflow behavior (DROP_OLDEST)
TEST_CASE("TinyEventBus - Queue overflow DROP_OLDEST", "[eventbus]") {
    received_events.clear();
    test_counter = 0;
    
    auto handle = test_bus.subscribe(test_event_handler, nullptr, MASK_ALL);
    TEST_ASSERT_GREATER_OR_EQUAL(0, handle);
    
    // Fill the queue with events
    for (int i = 0; i < EBUS_DISPATCH_QUEUE_LEN + 5; i++) {
        test_bus.publishFromISR(Event{TOPIC_TIMER, i, nullptr});
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Should receive events, but some older ones might be dropped
    TEST_ASSERT_GREATER_THAN(0, test_counter);
    
    // The last few events should definitely be received
    bool found_last_event = false;
    for (int event : received_events) {
        if (event >= EBUS_DISPATCH_QUEUE_LEN) {
            found_last_event = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_last_event);
    
    test_bus.unsubscribe(handle);
}

extern "C" void app_main(void) {
    UNITY_BEGIN();
    
    // Run all test cases
    RUN_TEST_CASE("TinyEventBus - Basic subscription and publishing");
    RUN_TEST_CASE("TinyEventBus - Topic mask filtering");
    RUN_TEST_CASE("TinyEventBus - Event predicate filtering");
    RUN_TEST_CASE("TinyEventBus - Destructor cleanup");
    RUN_TEST_CASE("TinyMailbox - Latest-only semantics");
    RUN_TEST_CASE("TinyMailbox - Destructor cleanup");
    RUN_TEST_CASE("TinyEventBus - ISR safety (simulated)");
    RUN_TEST_CASE("TinyEventBus - Queue overflow DROP_OLDEST");
    
    UNITY_END();
}
