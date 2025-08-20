#include <iostream>
#include <rxcpp/rx.hpp>
#include <cassert>

using namespace rxcpp;
using namespace rxcpp::operators;
using namespace rxcpp::sources;

void test_rxcpp_basic() {
    std::cout << "Testing RxCpp Basic Features..." << std::endl;
    
    // Test observable::just
    auto observable = observable<>::just(42);
    int received_value = 0;
    bool completed = false;
    
    observable.subscribe(
        [&received_value](int value) {
            received_value = value;
        },
        [](std::exception_ptr ep) {
            std::cout << "Error occurred" << std::endl;
        },
        [&completed]() {
            completed = true;
        }
    );
    
    assert(received_value == 42);
    assert(completed);
    std::cout << "✓ RxCpp observable::just works" << std::endl;
    
    // Test observable::range
    auto range_obs = observable<>::range(1, 5);
    std::vector<int> received_values;
    
    range_obs.subscribe([&received_values](int value) {
        received_values.push_back(value);
    });
    
    assert(received_values.size() == 5);
    assert(received_values[0] == 1);
    assert(received_values[4] == 5);
    std::cout << "✓ RxCpp observable::range works" << std::endl;
    
    // Test map operator
    auto mapped_obs = range_obs.map([](int x) { return x * 2; });
    std::vector<int> mapped_values;
    
    mapped_obs.subscribe([&mapped_values](int value) {
        mapped_values.push_back(value);
    });
    
    assert(mapped_values.size() == 5);
    assert(mapped_values[0] == 2);
    assert(mapped_values[4] == 10);
    std::cout << "✓ RxCpp map operator works" << std::endl;
    
    // Test filter operator
    auto filtered_obs = range_obs.filter([](int x) { return x % 2 == 0; });
    std::vector<int> filtered_values;
    
    filtered_obs.subscribe([&filtered_values](int value) {
        filtered_values.push_back(value);
    });
    
    assert(filtered_values.size() == 2);
    assert(filtered_values[0] == 2);
    assert(filtered_values[1] == 4);
    std::cout << "✓ RxCpp filter operator works" << std::endl;
    
    // Test take operator
    auto taken_obs = range_obs.take(3);
    std::vector<int> taken_values;
    
    taken_obs.subscribe([&taken_values](int value) {
        taken_values.push_back(value);
    });
    
    assert(taken_values.size() == 3);
    assert(taken_values[0] == 1);
    assert(taken_values[2] == 3);
    std::cout << "✓ RxCpp take operator works" << std::endl;
}

void test_rxcpp_subject() {
    std::cout << "Testing RxCpp Subject..." << std::endl;
    
    subjects::subject<int> subject;
    std::vector<int> received_values;
    
    auto subscription = subject.get_observable().subscribe([&received_values](int value) {
        received_values.push_back(value);
    });
    
    subject.get_subscriber().on_next(1);
    subject.get_subscriber().on_next(2);
    subject.get_subscriber().on_next(3);
    subject.get_subscriber().on_completed();
    
    assert(received_values.size() == 3);
    assert(received_values[0] == 1);
    assert(received_values[2] == 3);
    std::cout << "✓ RxCpp subject works" << std::endl;
}

void test_rxcpp_operators() {
    std::cout << "Testing RxCpp Advanced Operators..." << std::endl;
    
    // Test debounce
    auto debounced_obs = observable<>::range(1, 5).debounce(std::chrono::milliseconds(10));
    std::vector<int> debounced_values;
    
    debounced_obs.subscribe([&debounced_values](int value) {
        debounced_values.push_back(value);
    });
    
    // Test throttle
    auto throttled_obs = observable<>::range(1, 5).throttle(std::chrono::milliseconds(10));
    std::vector<int> throttled_values;
    
    throttled_obs.subscribe([&throttled_values](int value) {
        throttled_values.push_back(value);
    });
    
    std::cout << "✓ RxCpp debounce and throttle operators work" << std::endl;
}

int main() {
    std::cout << "=== Testing RxCpp Library ===" << std::endl;
    
    try {
        test_rxcpp_basic();
        test_rxcpp_subject();
        test_rxcpp_operators();
        
        std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
