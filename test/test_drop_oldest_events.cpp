#include <gtest/gtest.h>
#include <vector>
#include <string>
#include "main/eventbus/TinyEventBus.h"
#include "main/eventbus/EventProtocol.h"

class DropOldestEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize the event bus with a small queue for testing
        ASSERT_TRUE(bus.begin("test-drop-oldest", 4096, 5));
        
        // Set up event counter
        event_count = 0;
        bus.subscribe([](const Event& e, void* user) {
            auto* count = static_cast<int*>(user);
            (*count)++;
        }, &event_count);
    }
    
    void TearDown() override {
        event_count = 0;
    }
    
    TinyEventBus bus;
    int event_count;
};

// Test basic drop oldest functionality
TEST_F(DropOldestEventTest, DropOldestWhenQueueFull) {
    // Create events with unique identifiers
    std::vector<Event> events;
    for (int i = 0; i < EBUS_DISPATCH_QUEUE_LEN + 5; i++) {
        events.push_back(Event{TOPIC_TIMER, i, nullptr});
    }
    
    // Publish all events to the queue (this should trigger drop oldest)
    for (const auto& event : events) {
        bus.publishToQueue(event);
    }
    
    // Wait a bit for processing
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Check that we received events (some may have been dropped)
    EXPECT_GT(event_count, 0);
    
    // Verify queue stats
    auto stats = bus.getQueueStats();
    EXPECT_EQ(stats.total_spaces, EBUS_DISPATCH_QUEUE_LEN);
    EXPECT_LE(stats.messages_waiting, EBUS_DISPATCH_QUEUE_LEN);
}

// Test that newer events are preserved over older ones
TEST_F(DropOldestEventTest, NewerEventsPreserved) {
    // Create a sequence of events with unique values
    std::vector<int> expected_values;
    for (int i = 0; i < EBUS_DISPATCH_QUEUE_LEN + 3; i++) {
        expected_values.push_back(i);
    }
    
    // Track received values
    std::vector<int> received_values;
    bus.unsubscribe(0); // Remove the counter subscription
    bus.subscribe([](const Event& e, void* user) {
        auto* values = static_cast<std::vector<int>*>(user);
        values->push_back(e.i32);
    }, &received_values);
    
    // Publish events in sequence
    for (int value : expected_values) {
        bus.publishToQueue(Event{TOPIC_TIMER, value, nullptr});
    }
    
    // Wait for processing
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Check that we received some events
    EXPECT_GT(received_values.size(), 0);
    
    // Verify that the last few events are preserved (newer events)
    // The exact number depends on queue size and processing speed
    if (received_values.size() >= 3) {
        // The last few events should be the highest values
        int last_received = received_values.back();
        EXPECT_GE(last_received, EBUS_DISPATCH_QUEUE_LEN - 1);
    }
}

// Test ISR context drop oldest
TEST_F(DropOldestEventTest, ISRContextDropOldest) {
    // Fill the queue first
    for (int i = 0; i < EBUS_DISPATCH_QUEUE_LEN; i++) {
        bus.publishToQueue(Event{TOPIC_TIMER, i, nullptr});
    }
    
    // Verify queue is full
    auto stats_before = bus.getQueueStats();
    EXPECT_EQ(stats_before.spaces_available, 0);
    
    // Try to publish from ISR context (simulated)
    // This should trigger drop oldest
    BaseType_t higher_priority_woken = pdFALSE;
    bus.publishFromISR(Event{TOPIC_TIMER, 999, nullptr}, &higher_priority_woken);
    
    // Wait for processing
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Check that the new event was accepted
    auto stats_after = bus.getQueueStats();
    EXPECT_EQ(stats_after.messages_waiting, EBUS_DISPATCH_QUEUE_LEN);
}

// Test queue statistics
TEST_F(DropOldestEventTest, QueueStatistics) {
    // Check initial stats
    auto stats_empty = bus.getQueueStats();
    EXPECT_EQ(stats_empty.messages_waiting, 0);
    EXPECT_EQ(stats_empty.spaces_available, EBUS_DISPATCH_QUEUE_LEN);
    EXPECT_EQ(stats_empty.total_spaces, EBUS_DISPATCH_QUEUE_LEN);
    
    // Fill queue partially
    for (int i = 0; i < 5; i++) {
        bus.publishToQueue(Event{TOPIC_TIMER, i, nullptr});
    }
    
    auto stats_partial = bus.getQueueStats();
    EXPECT_EQ(stats_partial.messages_waiting, 5);
    EXPECT_EQ(stats_partial.spaces_available, EBUS_DISPATCH_QUEUE_LEN - 5);
    
    // Fill queue completely
    for (int i = 5; i < EBUS_DISPATCH_QUEUE_LEN; i++) {
        bus.publishToQueue(Event{TOPIC_TIMER, i, nullptr});
    }
    
    auto stats_full = bus.getQueueStats();
    EXPECT_EQ(stats_full.messages_waiting, EBUS_DISPATCH_QUEUE_LEN);
    EXPECT_EQ(stats_full.spaces_available, 0);
}

// Test that drop oldest doesn't cause crashes
TEST_F(DropOldestEventTest, DropOldestStability) {
    // Rapidly publish events to trigger drop oldest
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < EBUS_DISPATCH_QUEUE_LEN + 2; i++) {
            bus.publishToQueue(Event{TOPIC_TIMER, round * 100 + i, nullptr});
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // If we get here without crashing, the test passes
    EXPECT_TRUE(true);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

