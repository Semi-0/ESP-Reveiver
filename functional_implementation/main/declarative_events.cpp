#include "declarative_events.h"
#include <iostream>

// Global declarative event system instance
DeclarativeEventSystem g_declarativeEvents;

// Implementation of EventChain methods
void EventChain::execute(const Event& event, void* user_data) {
    for (auto& action : actions) {
        EventResult result = action->execute(event, user_data);
        
        if (condition) {
            EventResult cond_result = condition(event, user_data);
            if (cond_result.success) {
                if (success_handler) success_handler(result);
            } else {
                if (failure_handler) failure_handler(result);
            }
        } else {
            if (result.success && success_handler) {
                success_handler(result);
            } else if (!result.success && failure_handler) {
                failure_handler(result);
            }
        }
    }
}

std::string EventChain::get_description() const {
    std::string desc = "Chain: ";
    for (const auto& action : actions) {
        desc += action->get_name() + " -> ";
    }
    if (!desc.empty() && desc.length() > 4) {
        desc = desc.substr(0, desc.length() - 4);
    }
    return desc;
}

// Implementation of DeclarativeEventSystem methods
DeclarativeEventSystem& DeclarativeEventSystem::when(uint32_t event_mask, std::shared_ptr<EventChain> chain) {
    event_chains.emplace_back(event_mask, chain);
    return *this;
}

void DeclarativeEventSystem::handle_event(const Event& event) {
    for (auto& [mask, chain] : event_chains) {
        if (mask & (1u << event.type)) {
            chain->execute(event, nullptr);
        }
    }
}

const std::vector<std::pair<uint32_t, std::shared_ptr<EventChain>>>& DeclarativeEventSystem::get_chains() const {
    return event_chains;
}

// Implementation of LogAction execute method
EventResult LogAction::execute(const Event& event, void* user_data) {
    if (log_generator) {
        std::string log_message = log_generator(event, user_data);
        // In a real implementation, this would log
        std::cout << "LogAction: " << component << " - " << log_message << std::endl;
        return EventResult::success_result("Logged: " + log_message, nullptr);
    }
    return EventResult::failure_result("No log generator");
}

// Implementation of PublishAction execute method
EventResult PublishAction::execute(const Event& event, void* user_data) {
    if (message_generator) {
        std::string message = message_generator(event, user_data);
        // In a real implementation, this would publish to the event bus
        std::cout << "PublishAction: " << topic << " - " << message << std::endl;
        return EventResult::success_result("Published to " + topic, nullptr);
    }
    return EventResult::failure_result("No message generator");
}

// Implementation of QueryAction execute method
EventResult QueryAction::execute(const Event& event, void* user_data) {
    if (query_func) {
        return query_func(event, user_data);
    }
    return EventResult::failure_result("No query function");
}
