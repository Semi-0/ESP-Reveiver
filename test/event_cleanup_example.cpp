#include <iostream>
#include <string>
#include "main/eventbus/TinyEventBus.h"
#include "main/eventbus/EventProtocol.h"

// Example showing proper event cleanup usage
class EventCleanupExample {
private:
    TinyEventBus bus_;
    int processed_count_ = 0;
    
public:
    bool initialize() {
        if (!bus_.begin("cleanup-example", 4096, 5)) {
            std::cout << "Failed to initialize event bus" << std::endl;
            return false;
        }
        
        // Subscribe to events
        bus_.subscribe([](const Event& e, void* user) {
            auto* self = static_cast<EventCleanupExample*>(user);
            self->processed_count_++;
            
            if (e.ptr) {
                std::cout << "Processed event: " << e.i32 
                          << " with data: " << static_cast<const char*>(e.ptr) << std::endl;
            } else {
                std::cout << "Processed event: " << e.i32 << " (no data)" << std::endl;
            }
        }, this);
        
        return true;
    }
    
    void demonstrateCleanup() {
        std::cout << "=== Event Cleanup Example ===" << std::endl;
        
        // Example 1: Event with dynamic string data
        std::cout << "\n1. Publishing event with string data..." << std::endl;
        char* message = strdup("Hello, World!");
        Event string_event{TOPIC_TIMER, 1, message, free};
        bus_.publishToQueue(string_event);
        
        // Example 2: Event with custom cleanup function
        std::cout << "\n2. Publishing event with custom cleanup..." << std::endl;
        char* custom_data = strdup("Custom data");
        Event custom_event{TOPIC_TIMER, 2, custom_data, [](void* ptr) {
            std::cout << "Custom cleanup called for: " << static_cast<const char*>(ptr) << std::endl;
            free(ptr);
        }};
        bus_.publishToQueue(custom_event);
        
        // Example 3: Event without cleanup (static data)
        std::cout << "\n3. Publishing event without cleanup..." << std::endl;
        static const char* static_message = "Static message";
        Event static_event{TOPIC_TIMER, 3, const_cast<char*>(static_message), nullptr};
        bus_.publishToQueue(static_event);
        
        // Example 4: Event with allocated struct
        std::cout << "\n4. Publishing event with struct data..." << std::endl;
        struct TestStruct {
            int id;
            char name[32];
        };
        
        TestStruct* struct_data = static_cast<TestStruct*>(malloc(sizeof(TestStruct)));
        struct_data->id = 42;
        strcpy(struct_data->name, "TestStruct");
        
        Event struct_event{TOPIC_TIMER, 4, struct_data, [](void* ptr) {
            auto* data = static_cast<TestStruct*>(ptr);
            std::cout << "Cleaning up struct: id=" << data->id 
                      << ", name=" << data->name << std::endl;
            free(ptr);
        }};
        bus_.publishToQueue(struct_event);
        
        // Wait for processing
        vTaskDelay(pdMS_TO_TICKS(100));
        
        std::cout << "\nProcessed " << processed_count_ << " events" << std::endl;
    }
    
    void demonstrateDropOldest() {
        std::cout << "\n=== Drop Oldest with Cleanup ===" << std::endl;
        
        // Fill the queue to trigger drop oldest
        std::cout << "Filling queue to trigger drop oldest..." << std::endl;
        for (int i = 0; i < EBUS_DISPATCH_QUEUE_LEN + 3; i++) {
            char* data = strdup(("Event " + std::to_string(i)).c_str());
            Event event{TOPIC_TIMER, i, data, [](void* ptr) {
                std::cout << "Dropped/processed: " << static_cast<const char*>(ptr) << std::endl;
                free(ptr);
            }};
            bus_.publishToQueue(event);
        }
        
        // Wait for processing
        vTaskDelay(pdMS_TO_TICKS(200));
        
        std::cout << "Drop oldest demonstration completed" << std::endl;
    }
    
    void showQueueStats() {
        auto stats = bus_.getQueueStats();
        std::cout << "\nQueue Stats: " << stats.messages_waiting 
                  << "/" << stats.total_spaces << " messages" << std::endl;
    }
};

int main() {
    EventCleanupExample example;
    
    if (!example.initialize()) {
        std::cout << "Failed to initialize example" << std::endl;
        return 1;
    }
    
    example.demonstrateCleanup();
    example.demonstrateDropOldest();
    example.showQueueStats();
    
    std::cout << "\nExample completed successfully!" << std::endl;
    return 0;
}

