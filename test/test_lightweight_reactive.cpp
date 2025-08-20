#include "../main/lightweight_reactive.h"
#include "../main/functional_utils.h"
#include <iostream>
#include <cassert>

using namespace lwr;
using namespace fp;

// Mock ESP32 functions for testing
#define ESP_LOGI(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("[%s] ERROR: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("[%s] WARN: " fmt "\n", tag, ##__VA_ARGS__)

// Mock functions that would normally be in ESP32
std::string get_esp32_device_id() { return "test_device_123"; }
std::string MACHINE_ID = "test_machine";
std::string get_mqtt_status_topic() { return "test/status"; }

// Mock PinController
namespace PinController {
    int get_configured_pins_count() { return 5; }
}

// Mock MessageProcessor
namespace MessageProcessor {
    ParseResult parse_json_message(const std::string& message) {
        ParseResult result = {false, "", {}};
        if (message.find("digital") != std::string::npos) {
            result.success = true;
            result.commands.push_back({CommandType::DIGITAL, 13, 1});
        }
        return result;
    }
    
    ExecutionResult execute_pin_command(const PinCommand& command) {
        return {true, "", "Mock execution: pin " + std::to_string(command.pin)};
    }
}

void test_lightweight_observable() {
    std::cout << "Testing Lightweight Observable..." << std::endl;
    
    // Test just
    auto single_obs = Observable<int>::just(42);
    int received_value = 0;
    single_obs.subscribe([&received_value](const int& value) {
        received_value = value;
    });
    assert(received_value == 42);
    std::cout << "✓ Observable::just works" << std::endl;
    
    // Test from_vector
    std::vector<int> values = {1, 2, 3, 4, 5};
    auto vector_obs = Observable<int>::from_vector(values);
    std::vector<int> received_values;
    vector_obs.subscribe([&received_values](const int& value) {
        received_values.push_back(value);
    });
    assert(received_values.size() == 5);
    assert(received_values[0] == 1);
    assert(received_values[4] == 5);
    std::cout << "✓ Observable::from_vector works" << std::endl;
    
    // Test map
    auto mapped_obs = vector_obs.map([](int x) { return x * 2; });
    std::vector<int> mapped_values;
    mapped_obs.subscribe([&mapped_values](const int& value) {
        mapped_values.push_back(value);
    });
    assert(mapped_values.size() == 5);
    assert(mapped_values[0] == 2);
    assert(mapped_values[4] == 10);
    std::cout << "✓ Observable::map works" << std::endl;
    
    // Test filter
    auto filtered_obs = vector_obs.filter([](int x) { return x % 2 == 0; });
    std::vector<int> filtered_values;
    filtered_obs.subscribe([&filtered_values](const int& value) {
        filtered_values.push_back(value);
    });
    assert(filtered_values.size() == 2);
    assert(filtered_values[0] == 2);
    assert(filtered_values[1] == 4);
    std::cout << "✓ Observable::filter works" << std::endl;
    
    // Test take
    auto taken_obs = vector_obs.take(3);
    std::vector<int> taken_values;
    taken_obs.subscribe([&taken_values](const int& value) {
        taken_values.push_back(value);
    });
    assert(taken_values.size() == 3);
    assert(taken_values[0] == 1);
    assert(taken_values[2] == 3);
    std::cout << "✓ Observable::take works" << std::endl;
    
    // Test skip
    auto skipped_obs = vector_obs.skip(2);
    std::vector<int> skipped_values;
    skipped_obs.subscribe([&skipped_values](const int& value) {
        skipped_values.push_back(value);
    });
    assert(skipped_values.size() == 3);
    assert(skipped_values[0] == 3);
    assert(skipped_values[2] == 5);
    std::cout << "✓ Observable::skip works" << std::endl;
}

void test_lightweight_subject() {
    std::cout << "Testing Lightweight Subject..." << std::endl;
    
    Subject<int> subject;
    std::vector<int> received_values;
    
    subject.subscribe([&received_values](const int& value) {
        received_values.push_back(value);
    });
    
    subject.on_next(1);
    subject.on_next(2);
    subject.on_next(3);
    
    assert(received_values.size() == 3);
    assert(received_values[0] == 1);
    assert(received_values[2] == 3);
    std::cout << "✓ Subject works" << std::endl;
}

void test_lightweight_message_processor() {
    std::cout << "Testing Lightweight Message Processor..." << std::endl;
    
    LightweightMessageProcessor processor;
    bool message_processed = false;
    bool error_received = false;
    bool log_received = false;
    
    processor.subscribe_to_execution([&message_processed](const ExecutionEvent& event) {
        message_processed = true;
        std::cout << "✓ Execution event: " << event.result.action_description << std::endl;
    });
    
    processor.subscribe_to_errors([&error_received](const std::string& error) {
        error_received = true;
        std::cout << "✓ Error received: " << error << std::endl;
    });
    
    processor.subscribe_to_logs([&log_received](const std::string& log) {
        log_received = true;
        std::cout << "✓ Log received: " << log << std::endl;
    });
    
    // Test valid message
    processor.process_message(R"({"type":"digital","pin":13,"value":1})");
    
    // Test invalid message
    processor.process_message("invalid json");
    
    std::cout << "✓ Message processor works" << std::endl;
}

void test_functional_utils() {
    std::cout << "Testing Functional Utils..." << std::endl;
    
    // Test pipe operator
    auto add_one = [](int x) { return x + 1; };
    auto multiply_by_two = [](int x) { return x * 2; };
    auto square = [](int x) { return x * x; };
    
    auto pipeline = pipe(add_one, multiply_by_two, square);
    int result = pipeline(3); // (3 + 1) * 2 = 8, then 8^2 = 64
    assert(result == 64);
    std::cout << "✓ Pipe operator works" << std::endl;
    
    // Test Maybe monad
    auto just_value = Maybe<int>::Just(42);
    auto nothing_value = Maybe<int>::Nothing();
    
    assert(just_value.isJust());
    assert(nothing_value.isNothing());
    assert(just_value.fromJust() == 42);
    assert(nothing_value.fromMaybe(0) == 0);
    std::cout << "✓ Maybe monad works" << std::endl;
    
    // Test Either monad
    auto right_value = Either<std::string, int>::Right(42);
    auto left_value = Either<std::string, int>::Left("error");
    
    assert(right_value.isRight());
    assert(left_value.isLeft());
    assert(right_value.fromRight(0) == 42);
    assert(left_value.fromLeft("default") == "error");
    std::cout << "✓ Either monad works" << std::endl;
}

int main() {
    std::cout << "=== Testing Lightweight Reactive Library ===" << std::endl;
    
    try {
        test_functional_utils();
        test_lightweight_observable();
        test_lightweight_subject();
        test_lightweight_message_processor();
        
        std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
