#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include "main/eventbus/TinyEventBus.h"
#include "main/eventbus/EventProtocol.h"

// Example demonstrating manual event cleanup patterns in workers
class ManualCleanupExamples {
private:
    TinyEventBus bus_;
    int processed_count_ = 0;
    
public:
    bool initialize() {
        if (!bus_.begin("manual-cleanup-example", 4096, 5)) {
            std::cout << "Failed to initialize event bus" << std::endl;
            return false;
        }
        return true;
    }
    
    // Pattern 1: Manual cleanup with early extraction
    static bool worker_pattern1(const Event& trigger_event, void** out) {
        std::cout << "Pattern 1: Manual cleanup with early extraction" << std::endl;
        
        // Extract data early
        const char* data = static_cast<const char*>(trigger_event.ptr);
        if (!data) {
            std::cout << "  No data provided" << std::endl;
            return false;
        }
        
        // Copy the data we need
        std::string extracted_data(data);
        std::cout << "  Extracted: " << extracted_data << std::endl;
        
        // Manual cleanup immediately after extraction
        if (trigger_event.cleanup && trigger_event.ptr) {
            trigger_event.cleanup(trigger_event.ptr);
            std::cout << "  Manually cleaned up event payload" << std::endl;
        }
        
        // Use the copied data for processing
        *out = strdup(("Processed: " + extracted_data).c_str());
        return true;
    }
    
    // Pattern 2: Deferred cleanup with RAII wrapper
    static bool worker_pattern2(const Event& trigger_event, void** out) {
        std::cout << "Pattern 2: Deferred cleanup with RAII wrapper" << std::endl;
        
        // Create a RAII wrapper for manual cleanup
        class EventCleanupGuard {
        private:
            const Event& event_;
            bool cleaned_ = false;
        public:
            EventCleanupGuard(const Event& event) : event_(event) {}
            ~EventCleanupGuard() {
                if (!cleaned_ && event_.cleanup && event_.ptr) {
                    event_.cleanup(event_.ptr);
                    std::cout << "  Auto-cleanup via RAII guard" << std::endl;
                }
            }
            void cleanup_now() {
                if (!cleaned_ && event_.cleanup && event_.ptr) {
                    event_.cleanup(event_.ptr);
                    cleaned_ = true;
                    std::cout << "  Manual cleanup via guard" << std::endl;
                }
            }
            const Event& event() const { return event_; }
        };
        
        EventCleanupGuard guard(trigger_event);
        
        // Process the event
        const char* data = static_cast<const char*>(guard.event().ptr);
        if (!data) {
            std::cout << "  No data provided" << std::endl;
            return false;
        }
        
        std::string result = "Processed: " + std::string(data);
        *out = strdup(result.c_str());
        
        // Cleanup happens automatically when guard goes out of scope
        return true;
    }
    
    // Pattern 3: Conditional cleanup based on processing result
    static bool worker_pattern3(const Event& trigger_event, void** out) {
        std::cout << "Pattern 3: Conditional cleanup based on result" << std::endl;
        
        const char* data = static_cast<const char*>(trigger_event.ptr);
        if (!data) {
            std::cout << "  No data provided" << std::endl;
            return false;
        }
        
        std::string input_data(data);
        
        // Process the data
        bool success = !input_data.empty();
        
        if (success) {
            // Success case: cleanup immediately
            if (trigger_event.cleanup && trigger_event.ptr) {
                trigger_event.cleanup(trigger_event.ptr);
                std::cout << "  Cleaned up after successful processing" << std::endl;
            }
            
            *out = strdup(("Success: " + input_data).c_str());
        } else {
            // Failure case: defer cleanup or handle differently
            std::cout << "  Deferring cleanup due to processing failure" << std::endl;
            *out = nullptr;
        }
        
        return success;
    }
    
    // Pattern 4: Batch cleanup for multiple events
    static bool worker_pattern4_batch(const std::vector<Event>& events, void** out) {
        std::cout << "Pattern 4: Batch processing with cleanup" << std::endl;
        
        std::vector<std::string> extracted_data;
        
        // Extract all data first
        for (const auto& event : events) {
            const char* data = static_cast<const char*>(event.ptr);
            if (data) {
                extracted_data.push_back(data);
            }
        }
        
        // Manual cleanup all events
        for (const auto& event : events) {
            if (event.cleanup && event.ptr) {
                event.cleanup(event.ptr);
            }
        }
        std::cout << "  Batch cleaned up " << events.size() << " events" << std::endl;
        
        // Process the extracted data
        std::string result = "Batch: ";
        for (const auto& data : extracted_data) {
            result += data + ", ";
        }
        
        *out = strdup(result.c_str());
        return true;
    }
    
    // Pattern 5: Custom cleanup functions
    static bool worker_pattern5_custom(const Event& trigger_event, void** out) {
        std::cout << "Pattern 5: Custom cleanup functions" << std::endl;
        
        // Custom cleanup function that logs before freeing
        auto custom_cleanup = [](void* ptr) {
            if (ptr) {
                std::cout << "  Custom cleanup: freeing " << static_cast<const char*>(ptr) << std::endl;
                free(ptr);
            }
        };
        
        const char* data = static_cast<const char*>(trigger_event.ptr);
        if (!data) {
            std::cout << "  No data provided" << std::endl;
            return false;
        }
        
        std::string extracted_data(data);
        
        // Use custom cleanup instead of the event's cleanup function
        custom_cleanup(trigger_event.ptr);
        
        *out = strdup(("Custom cleaned: " + extracted_data).c_str());
        return true;
    }
    
    void demonstratePatterns() {
        std::cout << "=== Manual Cleanup Patterns in Workers ===" << std::endl;
        
        // Test Pattern 1
        std::cout << "\n--- Testing Pattern 1 ---" << std::endl;
        char* data1 = strdup("test data 1");
        Event event1{TOPIC_TIMER, 1, data1, free};
        void* result1 = nullptr;
        worker_pattern1(event1, &result1);
        if (result1) {
            std::cout << "Result: " << static_cast<const char*>(result1) << std::endl;
            free(result1);
        }
        
        // Test Pattern 2
        std::cout << "\n--- Testing Pattern 2 ---" << std::endl;
        char* data2 = strdup("test data 2");
        Event event2{TOPIC_TIMER, 2, data2, free};
        void* result2 = nullptr;
        worker_pattern2(event2, &result2);
        if (result2) {
            std::cout << "Result: " << static_cast<const char*>(result2) << std::endl;
            free(result2);
        }
        
        // Test Pattern 3
        std::cout << "\n--- Testing Pattern 3 ---" << std::endl;
        char* data3 = strdup("test data 3");
        Event event3{TOPIC_TIMER, 3, data3, free};
        void* result3 = nullptr;
        worker_pattern3(event3, &result3);
        if (result3) {
            std::cout << "Result: " << static_cast<const char*>(result3) << std::endl;
            free(result3);
        }
        
        // Test Pattern 4 (batch)
        std::cout << "\n--- Testing Pattern 4 (Batch) ---" << std::endl;
        std::vector<Event> batch_events;
        for (int i = 0; i < 3; i++) {
            char* batch_data = strdup(("batch data " + std::to_string(i)).c_str());
            batch_events.emplace_back(TOPIC_TIMER, i, batch_data, free);
        }
        void* result4 = nullptr;
        worker_pattern4_batch(batch_events, &result4);
        if (result4) {
            std::cout << "Result: " << static_cast<const char*>(result4) << std::endl;
            free(result4);
        }
        
        // Test Pattern 5
        std::cout << "\n--- Testing Pattern 5 (Custom Cleanup) ---" << std::endl;
        char* data5 = strdup("test data 5");
        Event event5{TOPIC_TIMER, 5, data5, free};
        void* result5 = nullptr;
        worker_pattern5_custom(event5, &result5);
        if (result5) {
            std::cout << "Result: " << static_cast<const char*>(result5) << std::endl;
            free(result5);
        }
    }
    
    void showBestPractices() {
        std::cout << "\n=== Manual Cleanup Best Practices ===" << std::endl;
        
        std::cout << "1. Extract data early before cleanup" << std::endl;
        std::cout << "2. Use RAII guards for complex cleanup scenarios" << std::endl;
        std::cout << "3. Consider conditional cleanup based on processing results" << std::endl;
        std::cout << "4. Use batch cleanup for multiple events" << std::endl;
        std::cout << "5. Implement custom cleanup functions when needed" << std::endl;
        std::cout << "6. Always check for null pointers before cleanup" << std::endl;
        std::cout << "7. Log cleanup operations for debugging" << std::endl;
        std::cout << "8. Use helper functions to reduce code duplication" << std::endl;
    }
};

int main() {
    ManualCleanupExamples example;
    
    if (!example.initialize()) {
        std::cout << "Failed to initialize example" << std::endl;
        return 1;
    }
    
    example.demonstratePatterns();
    example.showBestPractices();
    
    std::cout << "\nManual cleanup examples completed!" << std::endl;
    return 0;
}

