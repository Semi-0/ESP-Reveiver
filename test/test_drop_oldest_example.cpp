#include <iostream>
#include <vector>
#include <string>
#include "main/eventbus/TinyEventBus.h"
#include "main/eventbus/EventProtocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Example demonstrating drop oldest functionality
class DropOldestExample {
private:
    TinyEventBus bus_;
    int processed_events_ = 0;
    int dropped_events_ = 0;
    
public:
    bool initialize() {
        if (!bus_.begin("drop-oldest-example", 4096, 5)) {
            std::cout << "Failed to initialize event bus" << std::endl;
            return false;
        }
        
        // Subscribe to events
        bus_.subscribe([](const Event& e, void* user) {
            auto* self = static_cast<DropOldestExample*>(user);
            self->processed_events_++;
            std::cout << "Processed event: " << e.i32 << std::endl;
        }, this);
        
        return true;
    }
    
    void runExample() {
        std::cout << "=== Drop Oldest Event Example ===" << std::endl;
        
        // Show initial queue stats
        auto stats = bus_.getQueueStats();
        std::cout << "Initial queue: " << stats.messages_waiting 
                  << "/" << stats.total_spaces << " messages" << std::endl;
        
        // Fill the queue to capacity
        std::cout << "\nFilling queue to capacity..." << std::endl;
        for (int i = 0; i < EBUS_DISPATCH_QUEUE_LEN; i++) {
            bus_.publishToQueue(Event{TOPIC_TIMER, i, nullptr});
        }
        
        stats = bus_.getQueueStats();
        std::cout << "Queue full: " << stats.messages_waiting 
                  << "/" << stats.total_spaces << " messages" << std::endl;
        
        // Now try to add more events - this should trigger drop oldest
        std::cout << "\nAdding more events (should trigger drop oldest)..." << std::endl;
        for (int i = EBUS_DISPATCH_QUEUE_LEN; i < EBUS_DISPATCH_QUEUE_LEN + 5; i++) {
            bus_.publishToQueue(Event{TOPIC_TIMER, i, nullptr});
            std::cout << "Added event: " << i << std::endl;
        }
        
        // Wait for processing
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Show final stats
        stats = bus_.getQueueStats();
        std::cout << "\nFinal queue: " << stats.messages_waiting 
                  << "/" << stats.total_spaces << " messages" << std::endl;
        std::cout << "Processed events: " << processed_events_ << std::endl;
        
        // Demonstrate ISR context drop oldest
        std::cout << "\n=== ISR Context Drop Oldest ===" << std::endl;
        
        // Fill queue again
        for (int i = 0; i < EBUS_DISPATCH_QUEUE_LEN; i++) {
            bus_.publishToQueue(Event{TOPIC_TIMER, 1000 + i, nullptr});
        }
        
        stats = bus_.getQueueStats();
        std::cout << "Queue full again: " << stats.messages_waiting 
                  << "/" << stats.total_spaces << " messages" << std::endl;
        
        // Simulate ISR context publishing
        BaseType_t higher_priority_woken = pdFALSE;
        bus_.publishFromISR(Event{TOPIC_TIMER, 9999, nullptr}, &higher_priority_woken);
        std::cout << "Published from ISR context: 9999" << std::endl;
        
        // Wait for processing
        vTaskDelay(pdMS_TO_TICKS(50));
        
        stats = bus_.getQueueStats();
        std::cout << "After ISR publish: " << stats.messages_waiting 
                  << "/" << stats.total_spaces << " messages" << std::endl;
    }
    
    void showQueueMonitoring() {
        std::cout << "\n=== Queue Monitoring ===" << std::endl;
        
        // Monitor queue usage over time
        for (int round = 0; round < 5; round++) {
            auto stats = bus_.getQueueStats();
            float usage_percent = (float)stats.messages_waiting / stats.total_spaces * 100.0f;
            
            std::cout << "Round " << round << ": "
                      << stats.messages_waiting << "/" << stats.total_spaces
                      << " (" << usage_percent << "%)" << std::endl;
            
            // Add some events
            for (int i = 0; i < 3; i++) {
                bus_.publishToQueue(Event{TOPIC_TIMER, round * 10 + i, nullptr});
            }
            
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
};

int main() {
    DropOldestExample example;
    
    if (!example.initialize()) {
        std::cout << "Failed to initialize example" << std::endl;
        return 1;
    }
    
    example.runExample();
    example.showQueueMonitoring();
    
    std::cout << "\nExample completed successfully!" << std::endl;
    return 0;
}

