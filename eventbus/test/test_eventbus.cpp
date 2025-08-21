#include "eventbus/TinyEventBus.h"
#include "eventbus/EventProtocol.h"
#include "eventbus/FlowGraph.h"
#include <stdio.h>
#include <string.h>
#include <cassert>
#include <vector>

// Test utilities
struct TestContext {
    std::vector<Event> received_events;
    std::vector<int> tap_calls;
    int error_count = 0;
    int success_count = 0;
};

static TestContext g_test_ctx;

// Test helper functions
void reset_test_context() {
    g_test_ctx.received_events.clear();
    g_test_ctx.tap_calls.clear();
    g_test_ctx.error_count = 0;
    g_test_ctx.success_count = 0;
}

// Test event handlers
void test_event_handler(const Event& e, void* user) {
    g_test_ctx.received_events.push_back(e);
}

void test_tap_handler(const Event& e, void* user) {
    g_test_ctx.tap_calls.push_back(e.i32);
}

// Test async worker functions
bool test_async_success_worker(void** out) {
    *out = strdup("success_result");
    return true;
}

bool test_async_failure_worker(void** out) {
    *out = strdup("failure_result");
    return false;
}

// Test functions
void test_basic_event_publishing() {
    printf("=== Testing Basic Event Publishing ===\n");
    reset_test_context();
    
    TinyEventBus bus;
    assert(bus.begin());
    
    // Subscribe to WiFi events
    auto handle = bus.subscribe(test_event_handler, nullptr, bit(TOPIC_WIFI_CONNECTED));
    assert(handle >= 0);
    
    // Publish WiFi connected event
    Event wifi_event{TOPIC_WIFI_CONNECTED, 192168001, nullptr};
    bus.publish(wifi_event);
    
    // Verify event was received
    assert(g_test_ctx.received_events.size() == 1);
    assert(g_test_ctx.received_events[0].type == TOPIC_WIFI_CONNECTED);
    assert(g_test_ctx.received_events[0].i32 == 192168001);
    
    printf("✓ Basic event publishing test passed\n");
}

void test_topic_masking() {
    printf("=== Testing Topic Masking ===\n");
    reset_test_context();
    
    TinyEventBus bus;
    assert(bus.begin());
    
    // Subscribe only to WiFi events
    auto handle = bus.subscribe(test_event_handler, nullptr, bit(TOPIC_WIFI_CONNECTED));
    assert(handle >= 0);
    
    // Publish WiFi event (should be received)
    Event wifi_event{TOPIC_WIFI_CONNECTED, 1, nullptr};
    bus.publish(wifi_event);
    
    // Publish MDNS event (should NOT be received)
    Event mdns_event{TOPIC_MDNS_FOUND, 2, nullptr};
    bus.publish(mdns_event);
    
    // Verify only WiFi event was received
    assert(g_test_ctx.received_events.size() == 1);
    assert(g_test_ctx.received_events[0].type == TOPIC_WIFI_CONNECTED);
    
    printf("✓ Topic masking test passed\n");
}

void test_event_predicates() {
    printf("=== Testing Event Predicates ===\n");
    reset_test_context();
    
    TinyEventBus bus;
    assert(bus.begin());
    
    // Predicate: only accept events with i32 > 100
    auto predicate = [](const Event& e, void* user) -> bool {
        return e.i32 > 100;
    };
    
    // Subscribe with predicate
    auto handle = bus.subscribe(test_event_handler, nullptr, MASK_ALL, predicate, nullptr);
    assert(handle >= 0);
    
    // Publish event with i32 = 50 (should be filtered out)
    Event low_event{TOPIC_WIFI_CONNECTED, 50, nullptr};
    bus.publish(low_event);
    
    // Publish event with i32 = 200 (should be received)
    Event high_event{TOPIC_WIFI_CONNECTED, 200, nullptr};
    bus.publish(high_event);
    
    // Verify only high event was received
    assert(g_test_ctx.received_events.size() == 1);
    assert(g_test_ctx.received_events[0].i32 == 200);
    
    printf("✓ Event predicates test passed\n");
}

void test_isr_publishing() {
    printf("=== Testing ISR Publishing ===\n");
    reset_test_context();
    
    TinyEventBus bus;
    assert(bus.begin());
    
    // Subscribe to timer events
    auto handle = bus.subscribe(test_event_handler, nullptr, bit(TOPIC_TIMER));
    assert(handle >= 0);
    
    // Simulate ISR publishing
    Event timer_event{TOPIC_TIMER, 12345, nullptr};
    BaseType_t hpw = pdFALSE;
    bus.publishFromISR(timer_event, &hpw);
    
    // Give some time for the dispatcher task to process
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Verify event was received
    assert(g_test_ctx.received_events.size() == 1);
    assert(g_test_ctx.received_events[0].type == TOPIC_TIMER);
    assert(g_test_ctx.received_events[0].i32 == 12345);
    
    printf("✓ ISR publishing test passed\n");
}

void test_declarative_flows() {
    printf("=== Testing Declarative Flows ===\n");
    reset_test_context();
    
    TinyEventBus bus;
    assert(bus.begin());
    
    FlowGraph G(bus);
    
    // Test basic when() flow
    G.when(TOPIC_WIFI_CONNECTED,
        FlowGraph::publish(TOPIC_MDNS_FOUND, 42, nullptr)
    );
    
    // Subscribe to MDNS_FOUND to verify flow worked
    bus.subscribe(test_event_handler, nullptr, bit(TOPIC_MDNS_FOUND));
    
    // Trigger the flow
    bus.publish(Event{TOPIC_WIFI_CONNECTED, 0, nullptr});
    
    // Verify MDNS_FOUND was published
    assert(g_test_ctx.received_events.size() == 1);
    assert(g_test_ctx.received_events[0].type == TOPIC_MDNS_FOUND);
    assert(g_test_ctx.received_events[0].i32 == 42);
    
    printf("✓ Declarative flows test passed\n");
}

void test_tap_operator() {
    printf("=== Testing Tap Operator ===\n");
    reset_test_context();
    
    TinyEventBus bus;
    assert(bus.begin());
    
    FlowGraph G(bus);
    
    // Test tap operator - should observe but not publish
    G.when(TOPIC_WIFI_CONNECTED,
        FlowGraph::tap(test_tap_handler)
    );
    
    // Subscribe to MDNS_FOUND to verify tap doesn't publish
    bus.subscribe(test_event_handler, nullptr, bit(TOPIC_MDNS_FOUND));
    
    // Trigger the flow
    bus.publish(Event{TOPIC_WIFI_CONNECTED, 999, nullptr});
    
    // Verify tap was called
    assert(g_test_ctx.tap_calls.size() == 1);
    assert(g_test_ctx.tap_calls[0] == 999);
    
    // Verify no MDNS_FOUND was published (tap only observes)
    assert(g_test_ctx.received_events.size() == 0);
    
    printf("✓ Tap operator test passed\n");
}

void test_flow_composition() {
    printf("=== Testing Flow Composition ===\n");
    reset_test_context();
    
    TinyEventBus bus;
    assert(bus.begin());
    
    FlowGraph G(bus);
    
    // Test seq operator
    auto flow1 = FlowGraph::publish(TOPIC_MDNS_FOUND, 1, nullptr);
    auto flow2 = FlowGraph::publish(TOPIC_MQTT_CONNECTED, 2, nullptr);
    auto composed = FlowGraph::seq(flow1, flow2);
    
    G.when(TOPIC_WIFI_CONNECTED, composed);
    
    // Subscribe to both topics
    bus.subscribe(test_event_handler, nullptr, bit(TOPIC_MDNS_FOUND) | bit(TOPIC_MQTT_CONNECTED));
    
    // Trigger the flow
    bus.publish(Event{TOPIC_WIFI_CONNECTED, 0, nullptr});
    
    // Verify both events were published in sequence
    assert(g_test_ctx.received_events.size() == 2);
    assert(g_test_ctx.received_events[0].type == TOPIC_MDNS_FOUND);
    assert(g_test_ctx.received_events[0].i32 == 1);
    assert(g_test_ctx.received_events[1].type == TOPIC_MQTT_CONNECTED);
    assert(g_test_ctx.received_events[1].i32 == 2);
    
    printf("✓ Flow composition test passed\n");
}

void test_async_blocking() {
    printf("=== Testing Async Blocking ===\n");
    reset_test_context();
    
    TinyEventBus bus;
    assert(bus.begin());
    
    FlowGraph G(bus);
    
    // Test async_blocking with success
    G.when(TOPIC_WIFI_CONNECTED,
        G.async_blocking("test-async", test_async_success_worker,
            FlowGraph::publish(TOPIC_MDNS_FOUND, 1, nullptr),  // onOk
            FlowGraph::publish(TOPIC_MDNS_FAILED, 0, nullptr)  // onErr
        )
    );
    
    // Subscribe to both success and failure topics
    bus.subscribe(test_event_handler, nullptr, bit(TOPIC_MDNS_FOUND) | bit(TOPIC_MDNS_FAILED));
    
    // Trigger the flow
    bus.publish(Event{TOPIC_WIFI_CONNECTED, 0, nullptr});
    
    // Give time for async task to complete
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Verify success event was published
    assert(g_test_ctx.received_events.size() == 1);
    assert(g_test_ctx.received_events[0].type == TOPIC_MDNS_FOUND);
    assert(g_test_ctx.received_events[0].i32 == 1);
    
    printf("✓ Async blocking test passed\n");
}

void test_filter_operator() {
    printf("=== Testing Filter Operator ===\n");
    reset_test_context();
    
    TinyEventBus bus;
    assert(bus.begin());
    
    FlowGraph G(bus);
    
    // Test filter operator - only publish if i32 > 100
    auto filter_pred = [](const Event& e) -> bool {
        return e.i32 > 100;
    };
    
    G.when(TOPIC_WIFI_CONNECTED,
        FlowGraph::filter(filter_pred,
            FlowGraph::publish(TOPIC_MDNS_FOUND, 42, nullptr)
        )
    );
    
    // Subscribe to MDNS_FOUND
    bus.subscribe(test_event_handler, nullptr, bit(TOPIC_MDNS_FOUND));
    
    // Trigger with low value (should be filtered out)
    bus.publish(Event{TOPIC_WIFI_CONNECTED, 50, nullptr});
    
    // Trigger with high value (should pass through)
    bus.publish(Event{TOPIC_WIFI_CONNECTED, 200, nullptr});
    
    // Verify only high value triggered the flow
    assert(g_test_ctx.received_events.size() == 1);
    assert(g_test_ctx.received_events[0].type == TOPIC_MDNS_FOUND);
    assert(g_test_ctx.received_events[0].i32 == 42);
    
    printf("✓ Filter operator test passed\n");
}

void test_branch_operator() {
    printf("=== Testing Branch Operator ===\n");
    reset_test_context();
    
    TinyEventBus bus;
    assert(bus.begin());
    
    FlowGraph G(bus);
    
    // Test branch operator - different flows based on condition
    auto branch_pred = [](const Event& e) -> bool {
        return e.i32 > 100;
    };
    
    G.when(TOPIC_WIFI_CONNECTED,
        FlowGraph::branch(branch_pred,
            FlowGraph::publish(TOPIC_MDNS_FOUND, 1, nullptr),    // onTrue
            FlowGraph::publish(TOPIC_MDNS_FAILED, 0, nullptr)    // onFalse
        )
    );
    
    // Subscribe to both topics
    bus.subscribe(test_event_handler, nullptr, bit(TOPIC_MDNS_FOUND) | bit(TOPIC_MDNS_FAILED));
    
    // Trigger with low value (should go to onFalse)
    bus.publish(Event{TOPIC_WIFI_CONNECTED, 50, nullptr});
    
    // Trigger with high value (should go to onTrue)
    bus.publish(Event{TOPIC_WIFI_CONNECTED, 200, nullptr});
    
    // Verify both branches were taken
    assert(g_test_ctx.received_events.size() == 2);
    assert(g_test_ctx.received_events[0].type == TOPIC_MDNS_FAILED);  // low value
    assert(g_test_ctx.received_events[0].i32 == 0);
    assert(g_test_ctx.received_events[1].type == TOPIC_MDNS_FOUND);   // high value
    assert(g_test_ctx.received_events[1].i32 == 1);
    
    printf("✓ Branch operator test passed\n");
}

// Main test runner
extern "C" void app_main(void) {
    printf("=== EventBus Unit Tests ===\n");
    
    test_basic_event_publishing();
    test_topic_masking();
    test_event_predicates();
    test_isr_publishing();
    test_declarative_flows();
    test_tap_operator();
    test_flow_composition();
    test_async_blocking();
    test_filter_operator();
    test_branch_operator();
    
    printf("\n=== All Tests Passed! ===\n");
    
    // Keep running for a bit to allow async operations to complete
    vTaskDelay(pdMS_TO_TICKS(1000));
}


