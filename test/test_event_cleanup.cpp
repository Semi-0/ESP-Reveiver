#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <memory>
#include "main/eventbus/TinyEventBus.h"
#include "main/eventbus/EventProtocol.h"

// Test fixture for event cleanup testing
class EventCleanupTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(bus.begin("test-cleanup", 4096, 5));
        
        // Track cleanup calls
        cleanup_calls = 0;
        last_cleaned_ptr = nullptr;
    }
    
    void TearDown() override {
        cleanup_calls = 0;
        last_cleaned_ptr = nullptr;
    }
    
    // Cleanup function for testing
    static void testCleanup(void* ptr) {
        cleanup_calls++;
        last_cleaned_ptr = ptr;
        free(ptr); // Clean up the allocated memory
    }
    
    TinyEventBus bus;
    static int cleanup_calls;
    static void* last_cleaned_ptr;
};

int EventCleanupTest::cleanup_calls = 0;
void* EventCleanupTest::last_cleaned_ptr = nullptr;

// Test that events with cleanup functions are properly cleaned up when dropped
TEST_F(EventCleanupTest, DropOldestWithCleanup) {
    // Fill the queue with events that have cleanup functions
    for (int i = 0; i < EBUS_DISPATCH_QUEUE_LEN; i++) {
        char* data = strdup("test data");
        Event event{TOPIC_TIMER, i, data, testCleanup};
        bus.publishToQueue(event);
    }
    
    // Verify queue is full
    auto stats = bus.getQueueStats();
    EXPECT_EQ(stats.messages_waiting, EBUS_DISPATCH_QUEUE_LEN);
    
    // Add one more event - this should trigger drop oldest
    char* new_data = strdup("new data");
    Event new_event{TOPIC_TIMER, 999, new_data, testCleanup};
    bus.publishToQueue(new_event);
    
    // Wait for processing
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Verify that cleanup was called for the dropped event
    EXPECT_GT(cleanup_calls, 0);
    EXPECT_EQ(last_cleaned_ptr, new_data); // The dropped event's data should be cleaned up
}

// Test that events without cleanup functions don't cause issues
TEST_F(EventCleanupTest, DropOldestWithoutCleanup) {
    // Fill the queue with events that don't need cleanup
    for (int i = 0; i < EBUS_DISPATCH_QUEUE_LEN; i++) {
        Event event{TOPIC_TIMER, i, nullptr, nullptr};
        bus.publishToQueue(event);
    }
    
    // Add one more event - this should trigger drop oldest
    Event new_event{TOPIC_TIMER, 999, nullptr, nullptr};
    bus.publishToQueue(new_event);
    
    // Wait for processing
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Should not crash and cleanup_calls should remain 0
    EXPECT_EQ(cleanup_calls, 0);
}

// Test that events are cleaned up when processed normally
TEST_F(EventCleanupTest, NormalProcessingWithCleanup) {
    // Subscribe to events
    int processed_count = 0;
    bus.subscribe([](const Event& e, void* user) {
        auto* count = static_cast<int*>(user);
        (*count)++;
    }, &processed_count);
    
    // Publish events with cleanup functions
    for (int i = 0; i < 5; i++) {
        char* data = strdup("processed data");
        Event event{TOPIC_TIMER, i, data, testCleanup};
        bus.publishToQueue(event);
    }
    
    // Wait for processing
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Verify events were processed
    EXPECT_EQ(processed_count, 5);
    
    // Verify cleanup was called for each processed event
    EXPECT_EQ(cleanup_calls, 5);
}

// Test RAII behavior with copy constructor
TEST_F(EventCleanupTest, CopyConstructorCleanup) {
    // Create an event with cleanup
    char* original_data = strdup("original data");
    Event original{TOPIC_TIMER, 1, original_data, testCleanup};
    
    // Copy the event (transfers ownership)
    Event copy = original;
    
    // Original should have null pointers now
    EXPECT_EQ(original.ptr, nullptr);
    EXPECT_EQ(original.cleanup, nullptr);
    
    // Copy should have the data
    EXPECT_EQ(copy.ptr, original_data);
    EXPECT_EQ(copy.cleanup, testCleanup);
    
    // When copy goes out of scope, it should clean up
    // (This happens automatically in the destructor)
}

// Test assignment operator cleanup
TEST_F(EventCleanupTest, AssignmentOperatorCleanup) {
    // Create first event
    char* data1 = strdup("data 1");
    Event event1{TOPIC_TIMER, 1, data1, testCleanup};
    
    // Create second event
    char* data2 = strdup("data 2");
    Event event2{TOPIC_TIMER, 2, data2, testCleanup};
    
    // Assign event2 to event1 (should clean up data1)
    event1 = event2;
    
    // event1 should now have data2, event2 should be cleared
    EXPECT_EQ(event1.ptr, data2);
    EXPECT_EQ(event2.ptr, nullptr);
    EXPECT_EQ(event2.cleanup, nullptr);
    
    // data1 should have been cleaned up
    EXPECT_EQ(cleanup_calls, 1);
    EXPECT_EQ(last_cleaned_ptr, data1);
}

// Test ISR context drop oldest with cleanup
TEST_F(EventCleanupTest, ISRContextDropOldestWithCleanup) {
    // Fill the queue
    for (int i = 0; i < EBUS_DISPATCH_QUEUE_LEN; i++) {
        char* data = strdup("isr test data");
        Event event{TOPIC_TIMER, i, data, testCleanup};
        bus.publishToQueue(event);
    }
    
    // Verify queue is full
    auto stats = bus.getQueueStats();
    EXPECT_EQ(stats.spaces_available, 0);
    
    // Try to publish from ISR context
    char* isr_data = strdup("isr new data");
    Event isr_event{TOPIC_TIMER, 999, isr_data, testCleanup};
    BaseType_t higher_priority_woken = pdFALSE;
    bus.publishFromISR(isr_event, &higher_priority_woken);
    
    // Wait for processing
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Verify cleanup was called
    EXPECT_GT(cleanup_calls, 0);
}

// Test memory leak prevention
TEST_F(EventCleanupTest, NoMemoryLeaks) {
    const int num_events = 100;
    
    // Publish many events with dynamic data
    for (int i = 0; i < num_events; i++) {
        char* data = strdup("leak test data");
        Event event{TOPIC_TIMER, i, data, testCleanup};
        bus.publishToQueue(event);
    }
    
    // Wait for processing
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // All events should have been cleaned up
    EXPECT_EQ(cleanup_calls, num_events);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

