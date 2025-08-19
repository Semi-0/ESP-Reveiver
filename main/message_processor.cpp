#include "message_processor.h"
#include "config.h"
#include "esp_log.h"
#include "pin_controller.h"
#include <sstream>
#include "cJSON.h"

// ===== PURE PARSING FUNCTIONS =====
// These functions only transform data, no side effects

std::optional<PinCommand> MessageProcessor::parse_single_json_command(const std::string& json_str) {
    cJSON* root = cJSON_Parse(json_str.c_str());
    if (!root) return std::nullopt;

    cJSON* type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type) || type->valuestring == nullptr) {
        cJSON_Delete(root);
        return std::nullopt;
    }

    cJSON* pin = cJSON_GetObjectItemCaseSensitive(root, "pin");
    if (!cJSON_IsNumber(pin)) {
        cJSON_Delete(root);
        return std::nullopt;
    }

    cJSON* value = cJSON_GetObjectItemCaseSensitive(root, "value");
    if (!cJSON_IsNumber(value)) {
        cJSON_Delete(root);
        return std::nullopt;
    }

    CommandType cmd_type;
    if (strcmp(type->valuestring, "digital") == 0) {
        cmd_type = CommandType::DIGITAL;
    } else if (strcmp(type->valuestring, "analog") == 0) {
        cmd_type = CommandType::ANALOG;
    } else {
        cJSON_Delete(root);
        return std::nullopt;
    }

    PinCommand command = {
        .type = cmd_type,
        .pin = pin->valueint,
        .value = value->valueint
    };

    cJSON_Delete(root);
    return command;
}

ParseResult MessageProcessor::parse_json_message(const std::string& message) {
    ParseResult result = {false, "", {}};
    
    cJSON* root = cJSON_Parse(message.c_str());
    if (!root) {
        result.error_message = "Failed to parse JSON";
        return result;
    }

    if (cJSON_IsArray(root)) {
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, root) {
            if (cJSON_IsObject(item)) {
                char* json_str = cJSON_Print(item);
                if (json_str) {
                    auto command = parse_single_json_command(json_str);
                    if (command) {
                        result.commands.push_back(*command);
                    }
                    free(json_str);
                }
            }
        }
    } else if (cJSON_IsObject(root)) {
        char* json_str = cJSON_Print(root);
        if (json_str) {
            auto command = parse_single_json_command(json_str);
            if (command) {
                result.commands.push_back(*command);
            }
            free(json_str);
        }
    } else {
        result.error_message = "JSON must be an object or array";
        cJSON_Delete(root);
        return result;
    }

    cJSON_Delete(root);
    result.success = !result.commands.empty();
    if (!result.success && result.error_message.empty()) {
        result.error_message = "No valid commands found";
    }
    return result;
}

// ===== PURE EXECUTION FUNCTIONS =====
// These functions have explicit inputs and outputs

ExecutionResult MessageProcessor::execute_pin_command(const PinCommand& command) {
    ExecutionResult result = {false, "", ""};
    
    switch (command.type) {
        case CommandType::DIGITAL: {
            bool low = command.value == 0;
            bool high = !low;
            PinController::digital_write(command.pin, high);
            result.success = true;
            result.action_description = "Digital write: pin " + std::to_string(command.pin) + 
                                      " = " + (high ? "HIGH" : "LOW");
            break;
        }
        case CommandType::ANALOG: {
            PinController::analog_write(command.pin, command.value);
            result.success = true;
            result.action_description = "Analog write: pin " + std::to_string(command.pin) + 
                                      " = " + std::to_string(command.value);
            break;
        }
        default: {
            result.error_message = "Unknown command type";
            break;
        }
    }
    
    return result;
}

std::vector<ExecutionResult> MessageProcessor::execute_commands(const std::vector<PinCommand>& commands) {
    std::vector<ExecutionResult> results;
    results.reserve(commands.size());
    
    for (const auto& command : commands) {
        results.push_back(execute_pin_command(command));
    }
    
    return results;
}

// ===== LEGACY FUNCTIONS (DEPRECATED) =====

void MessageProcessor::process_message(const std::string& message) {
    // Legacy function - use parse_json_message and execute_commands instead
    auto parse_result = parse_json_message(message);
    if (parse_result.success) {
        execute_commands(parse_result.commands);
    }
}

std::vector<std::string> MessageProcessor::split(const std::string& s, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }

    return result;
}

bool MessageProcessor::try_process_json(const std::string& message) {
    // Legacy function - use parse_json_message and execute_commands instead
    auto parse_result = parse_json_message(message);
    if (parse_result.success) {
        auto execution_results = execute_commands(parse_result.commands);
        return true;
    }
    return false;
}

void MessageProcessor::process_digital_command(const std::vector<std::string>& parts) {
    // Legacy function - not implemented
}

void MessageProcessor::process_analog_command(const std::vector<std::string>& parts) {
    // Legacy function - not implemented
}

void MessageProcessor::process_motor_command(const std::vector<std::string>& parts) {
    // Legacy function - not implemented
}

