#ifndef DECLARATIVE_EVENTS_H
#define DECLARATIVE_EVENTS_H

#include "event_bus_interface.h"
#include <functional>
#include <memory>
#include <vector>
#include <string>

// Forward declarations
class EventChain;
class EventCondition;
class EventAction;

// Event handler function type
using EventHandlerFunc = std::function<void(const Event&, void*)>;

// Event condition function type
using EventConditionFunc = std::function<bool(const Event&, void*)>;

// Event action function type
using EventActionFunc = std::function<void(const Event&, void*)>;

// Event result for chaining
struct EventResult {
    bool success;
    std::string message;
    void* data;
    
    EventResult(bool s = true, const std::string& msg = "", void* d = nullptr)
        : success(s), message(msg), data(d) {}
    
    static EventResult success_result(const std::string& msg = "", void* data = nullptr) {
        return EventResult(true, msg, data);
    }
    
    static EventResult failure_result(const std::string& msg = "", void* data = nullptr) {
        return EventResult(false, msg, data);
    }
};

// Event action that can be chained
class EventAction {
public:
    virtual ~EventAction() = default;
    virtual EventResult execute(const Event& event, void* user_data) = 0;
    virtual std::string get_name() const = 0;
};

// Concrete event actions
class PublishAction : public EventAction {
private:
    std::string topic;
    std::function<std::string(const Event&, void*)> message_generator;
    
public:
    PublishAction(const std::string& t, std::function<std::string(const Event&, void*)> generator)
        : topic(t), message_generator(generator) {}
    
    EventResult execute(const Event& event, void* user_data) override;
    
    std::string get_name() const override {
        return "publish(" + topic + ")";
    }
};

class LogAction : public EventAction {
private:
    std::string component;
    std::function<std::string(const Event&, void*)> log_generator;
    
public:
    LogAction(const std::string& comp, std::function<std::string(const Event&, void*)> generator)
        : component(comp), log_generator(generator) {}
    
    EventResult execute(const Event& event, void* user_data) override;
    
    std::string get_name() const override {
        return "log(" + component + ")";
    }
};

class QueryAction : public EventAction {
private:
    std::string query_type;
    std::function<EventResult(const Event&, void*)> query_func;
    
public:
    QueryAction(const std::string& type, std::function<EventResult(const Event&, void*)> func)
        : query_type(type), query_func(func) {}
    
    EventResult execute(const Event& event, void* user_data) override;
    
    std::string get_name() const override {
        return "query(" + query_type + ")";
    }
};

// Event chain for composable operations
class EventChain {
private:
    std::vector<std::shared_ptr<EventAction>> actions;
    std::function<EventResult(const Event&, void*)> condition;
    std::function<void(const EventResult&)> success_handler;
    std::function<void(const EventResult&)> failure_handler;
    
public:
    EventChain() = default;
    
    // Add action to chain
    EventChain& do_action(std::shared_ptr<EventAction> action) {
        actions.push_back(action);
        return *this;
    }
    
    // Set condition
    EventChain& if_condition(std::function<EventResult(const Event&, void*)> cond) {
        condition = cond;
        return *this;
    }
    
    // Set success handler
    EventChain& if_succeeded(std::function<void(const EventResult&)> handler) {
        success_handler = handler;
        return *this;
    }
    
    // Set failure handler
    EventChain& if_failed(std::function<void(const EventResult&)> handler) {
        failure_handler = handler;
        return *this;
    }
    
    // Execute the chain
    void execute(const Event& event, void* user_data);
    
    std::string get_description() const;
};

// Declarative event system
class DeclarativeEventSystem {
private:
    std::vector<std::pair<uint32_t, std::shared_ptr<EventChain>>> event_chains;
    
public:
    // When operator - declares what to do when an event occurs
    DeclarativeEventSystem& when(uint32_t event_mask, std::shared_ptr<EventChain> chain);
    
    // Execute all chains for a given event
    void handle_event(const Event& event);
    
    // Get all registered chains
    const std::vector<std::pair<uint32_t, std::shared_ptr<EventChain>>>& get_chains() const;
};

// Factory functions for creating actions
namespace EventActions {
    // Create a publish action
    inline std::shared_ptr<EventAction> publish(const std::string& topic, 
                                               std::function<std::string(const Event&, void*)> generator) {
        return std::make_shared<PublishAction>(topic, generator);
    }
    
    // Create a log action
    inline std::shared_ptr<EventAction> log(const std::string& component,
                                           std::function<std::string(const Event&, void*)> generator) {
        return std::make_shared<LogAction>(component, generator);
    }
    
    // Create a query action
    inline std::shared_ptr<EventAction> query(const std::string& query_type,
                                             std::function<EventResult(const Event&, void*)> func) {
        return std::make_shared<QueryAction>(query_type, func);
    }
}

// Factory functions for creating conditions
namespace EventConditions {
    // Success condition
    inline std::function<EventResult(const Event&, void*)> success() {
        return [](const Event&, void*) { return EventResult::success_result(); };
    }
    
    // Failure condition
    inline std::function<EventResult(const Event&, void*)> failure() {
        return [](const Event&, void*) { return EventResult::failure_result(); };
    }
    
    // Custom condition
    inline std::function<EventResult(const Event&, void*)> custom(std::function<bool(const Event&, void*)> func) {
        return [func](const Event& event, void* user_data) {
            return func(event, user_data) ? EventResult::success_result() : EventResult::failure_result();
        };
    }
}

// Global declarative event system instance
extern DeclarativeEventSystem g_declarativeEvents;

// Convenience macros for cleaner syntax
#define WHEN(event_mask) g_declarativeEvents.when(event_mask, std::make_shared<EventChain>())
#define DO(action) do_action(action)
#define IF_SUCCEEDED(handler) if_succeeded(handler)
#define IF_FAILED(handler) if_failed(handler)

#endif // DECLARATIVE_EVENTS_H
