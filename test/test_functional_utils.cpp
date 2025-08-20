#include "../main/functional_utils.h"
#include "../main/functional_message_processor.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace fp;
using namespace fmp;

// Test utilities
void assert_true(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        assert(false);
    } else {
        std::cout << "PASSED: " << message << std::endl;
    }
}

void assert_equal(const std::string& actual, const std::string& expected, const std::string& message) {
    if (actual != expected) {
        std::cerr << "FAILED: " << message << " (expected: " << expected << ", got: " << actual << ")" << std::endl;
        assert(false);
    } else {
        std::cout << "PASSED: " << message << std::endl;
    }
}

template<typename T>
void assert_equal(const T& actual, const T& expected, const std::string& message) {
    if (actual != expected) {
        std::cerr << "FAILED: " << message << " (expected: " << expected << ", got: " << actual << ")" << std::endl;
        assert(false);
    } else {
        std::cout << "PASSED: " << message << std::endl;
    }
}

// ===== PIPE OPERATOR TESTS =====

void test_pipe_operator() {
    std::cout << "\n=== Testing Pipe Operator ===" << std::endl;
    
    // Simple pipe
    auto add_one = [](int x) { return x + 1; };
    auto multiply_by_two = [](int x) { return x * 2; };
    auto square = [](int x) { return x * x; };
    
    auto pipeline = pipe(add_one, multiply_by_two, square);
    int result = pipeline(3); // (3 + 1) * 2 = 8, then 8^2 = 64
    
    assert_equal(result, 64, "Pipe operator with three functions");
    
    // Single function pipe
    auto single_pipe = pipe(add_one);
    result = single_pipe(5);
    assert_equal(result, 6, "Pipe operator with single function");
    
    // String transformation pipe
    auto to_upper = [](const std::string& s) {
        std::string result = s;
        for (char& c : result) {
            c = std::toupper(c);
        }
        return result;
    };
    
    auto add_exclamation = [](const std::string& s) { return s + "!"; };
    auto repeat_twice = [](const std::string& s) { return s + s; };
    
    auto string_pipeline = pipe(to_upper, add_exclamation, repeat_twice);
    std::string string_result = string_pipeline("hello");
    assert_equal(string_result, "HELLO!HELLO!", "String transformation pipe");
}

// ===== COMPOSE OPERATOR TESTS =====

void test_compose_operator() {
    std::cout << "\n=== Testing Compose Operator ===" << std::endl;
    
    auto add_one = [](int x) { return x + 1; };
    auto multiply_by_two = [](int x) { return x * 2; };
    auto square = [](int x) { return x * x; };
    
    // Compose is right-to-left: square(multiply_by_two(add_one(x)))
    auto composed = compose(square, multiply_by_two, add_one);
    int result = composed(3); // (3 + 1) * 2 = 8, then 8^2 = 64
    
    assert_equal(result, 64, "Compose operator with three functions");
    
    // Compare with pipe (should be different order)
    auto piped = pipe(add_one, multiply_by_two, square);
    int pipe_result = piped(3);
    assert_equal(pipe_result, 64, "Pipe operator (should match compose for this case)");
}

// ===== MAYBE MONAD TESTS =====

void test_maybe_monad() {
    std::cout << "\n=== Testing Maybe Monad ===" << std::endl;
    
    // Just and Nothing constructors
    auto just_value = Maybe<int>::Just(42);
    auto nothing_value = Maybe<int>::Nothing();
    
    assert_true(just_value.isJust(), "Just value should be Just");
    assert_true(nothing_value.isNothing(), "Nothing value should be Nothing");
    assert_true(!just_value.isNothing(), "Just value should not be Nothing");
    assert_true(!nothing_value.isJust(), "Nothing value should not be Just");
    
    // fromJust and fromMaybe
    assert_equal(just_value.fromJust(), 42, "fromJust should return the value");
    assert_equal(just_value.fromMaybe(0), 42, "fromMaybe should return the value when Just");
    assert_equal(nothing_value.fromMaybe(0), 0, "fromMaybe should return default when Nothing");
    
    // Functor: fmap
    auto double_value = [](int x) { return x * 2; };
    auto doubled_just = just_value.fmap(double_value);
    auto doubled_nothing = nothing_value.fmap(double_value);
    
    assert_true(doubled_just.isJust(), "fmap on Just should return Just");
    assert_equal(doubled_just.fromJust(), 84, "fmap should apply function to value");
    assert_true(doubled_nothing.isNothing(), "fmap on Nothing should return Nothing");
    
    // Monad: bind (>>=)
    auto safe_divide = [](int x) -> Maybe<int> {
        if (x == 0) return Maybe<int>::Nothing();
        return Maybe<int>::Just(100 / x);
    };
    
    auto result1 = just_value >>= safe_divide;
    auto result2 = Maybe<int>::Just(0) >>= safe_divide;
    
    assert_true(result1.isJust(), "bind should work with Just");
    assert_equal(result1.fromJust(), 2, "bind should chain functions");
    assert_true(result2.isNothing(), "bind should handle Nothing");
    
    // Alternative: alt (<|>)
    auto alternative = nothing_value.alt(Maybe<int>::Just(99));
    assert_true(alternative.isJust(), "alt should provide alternative");
    assert_equal(alternative.fromJust(), 99, "alt should use alternative value");
    
    auto alternative2 = just_value.alt(Maybe<int>::Just(99));
    assert_equal(alternative2.fromJust(), 42, "alt should prefer first value when Just");
}

// ===== EITHER MONAD TESTS =====

void test_either_monad() {
    std::cout << "\n=== Testing Either Monad ===" << std::endl;
    
    // Left and Right constructors
    auto left_value = Either<std::string, int>::Left("error");
    auto right_value = Either<std::string, int>::Right(42);
    
    assert_true(left_value.isLeft(), "Left value should be Left");
    assert_true(right_value.isRight(), "Right value should be Right");
    assert_true(!left_value.isRight(), "Left value should not be Right");
    assert_true(!right_value.isLeft(), "Right value should not be Left");
    
    // fromLeft and fromRight
    assert_equal(left_value.fromLeft("default"), "error", "fromLeft should return left value");
    assert_equal(left_value.fromRight(0), 0, "fromRight should return default for Left");
    assert_equal(right_value.fromRight(0), 42, "fromRight should return right value");
    assert_equal(right_value.fromLeft("default"), "default", "fromLeft should return default for Right");
    
    // Functor: fmap
    auto double_value = [](int x) { return x * 2; };
    auto doubled_right = right_value.fmap(double_value);
    auto doubled_left = left_value.fmap(double_value);
    
    assert_true(doubled_right.isRight(), "fmap on Right should return Right");
    assert_equal(doubled_right.fromRight(0), 84, "fmap should apply function to right value");
    assert_true(doubled_left.isLeft(), "fmap on Left should return Left");
    assert_equal(doubled_left.fromLeft(""), "error", "fmap should preserve left value");
    
    // Monad: bind (>>=)
    auto safe_divide = [](int x) -> Either<std::string, int> {
        if (x == 0) return Either<std::string, int>::Left("division by zero");
        return Either<std::string, int>::Right(100 / x);
    };
    
    auto result1 = right_value >>= safe_divide;
    auto result2 = Either<std::string, int>::Right(0) >>= safe_divide;
    
    assert_true(result1.isRight(), "bind should work with Right");
    assert_equal(result1.fromRight(0), 2, "bind should chain functions");
    assert_true(result2.isLeft(), "bind should handle Left");
    assert_equal(result2.fromLeft(""), "division by zero", "bind should propagate error");
    
    // Bifunctor: bimap
    auto left_transform = [](const std::string& s) { return "ERROR: " + s; };
    auto right_transform = [](int x) { return x + 10; };
    
    auto bimapped = right_value.bimap(left_transform, right_transform);
    assert_true(bimapped.isRight(), "bimap should preserve Right");
    assert_equal(bimapped.fromRight(0), 52, "bimap should transform right value");
}

// ===== IO MONAD TESTS =====

void test_io_monad() {
    std::cout << "\n=== Testing IO Monad ===" << std::endl;
    
    // Simple IO action
    auto get_five = IO<int>::pure(5);
    int result = get_five.unsafeRunIO();
    assert_equal(result, 5, "IO pure should return the value");
    
    // IO with side effect simulation
    int side_effect_counter = 0;
    auto increment_counter = IO<void>([&side_effect_counter]() {
        side_effect_counter++;
    });
    
    increment_counter.unsafeRunIO();
    assert_equal(side_effect_counter, 1, "IO should execute side effect");
    
    // Functor: fmap
    auto doubled_io = get_five.fmap([](int x) { return x * 2; });
    result = doubled_io.unsafeRunIO();
    assert_equal(result, 10, "IO fmap should apply function");
    
    // Monad: bind (>>=)
    auto io_with_bind = get_five >>= [](int x) -> IO<int> {
        return IO<int>::pure(x * 3);
    };
    result = io_with_bind.unsafeRunIO();
    assert_equal(result, 15, "IO bind should chain actions");
}

// ===== LIST MONAD TESTS =====

void test_list_monad() {
    std::cout << "\n=== Testing List Monad ===" << std::endl;
    
    // Empty and singleton
    auto empty_list = List<int>::empty();
    auto singleton_list = List<int>::singleton(42);
    
    assert_true(empty_list.isEmpty(), "Empty list should be empty");
    assert_true(!singleton_list.isEmpty(), "Singleton list should not be empty");
    assert_equal(singleton_list.size(), 1, "Singleton list should have size 1");
    
    // Functor: fmap
    auto double_value = [](int x) { return x * 2; };
    auto doubled_list = singleton_list.fmap(double_value);
    assert_equal(doubled_list.size(), 1, "fmap should preserve size");
    assert_equal(doubled_list.toVector()[0], 84, "fmap should apply function");
    
    // Monad: bind (>>=)
    auto replicate = [](int x) -> List<int> {
        std::vector<int> result;
        for (int i = 0; i < x; ++i) {
            result.push_back(x);
        }
        return List<int>(result);
    };
    
    auto bound_list = singleton_list >>= replicate;
    assert_equal(bound_list.size(), 42, "bind should flatten and transform");
    
    // MonadPlus: mplus
    auto list1 = List<int>({1, 2, 3});
    auto list2 = List<int>({4, 5, 6});
    auto combined = list1.mplus(list2);
    
    assert_equal(combined.size(), 6, "mplus should combine lists");
    auto combined_vec = combined.toVector();
    assert_equal(combined_vec[0], 1, "mplus should preserve order");
    assert_equal(combined_vec[5], 6, "mplus should include all elements");
}

// ===== FUNCTIONAL MESSAGE PROCESSOR TESTS =====

void test_functional_message_processor() {
    std::cout << "\n=== Testing Functional Message Processor ===" << std::endl;
    
    // Test parse_message function
    std::string valid_json = R"({"type":"digital","pin":13,"value":1})";
    auto maybe_command = parse_message(valid_json);
    
    assert_true(maybe_command.isJust(), "Valid JSON should parse successfully");
    auto command = maybe_command.fromJust();
    assert_equal(command.type, CommandType::DIGITAL, "Should parse digital command type");
    assert_equal(command.pin, 13, "Should parse pin number");
    assert_equal(command.value, 1, "Should parse value");
    
    // Test invalid JSON
    std::string invalid_json = "invalid json";
    auto maybe_invalid = parse_message(invalid_json);
    assert_true(maybe_invalid.isNothing(), "Invalid JSON should return Nothing");
    
    // Test validation functions
    auto valid_pin = validate_pin(13);
    auto invalid_pin = validate_pin(100);
    
    assert_true(valid_pin.isJust(), "Valid pin should be Just");
    assert_true(invalid_pin.isNothing(), "Invalid pin should be Nothing");
    
    auto valid_digital = validate_digital_value(1);
    auto invalid_digital = validate_digital_value(2);
    
    assert_true(valid_digital.isJust(), "Valid digital value should be Just");
    assert_true(invalid_digital.isNothing(), "Invalid digital value should be Nothing");
    
    auto valid_analog = validate_analog_value(128);
    auto invalid_analog = validate_analog_value(300);
    
    assert_true(valid_analog.isJust(), "Valid analog value should be Just");
    assert_true(invalid_analog.isNothing(), "Invalid analog value should be Nothing");
}

// ===== REACTIVE STREAMS TESTS =====

void test_reactive_streams() {
    std::cout << "\n=== Testing Reactive Streams ===" << std::endl;
    
    // Test Observable::just
    auto observable = Observable<int>::just(42);
    int received_value = 0;
    bool completed = false;
    
    auto subscription = observable.subscribe(
        [&received_value](const int& value) { received_value = value; },
        [](const std::string& error) { /* ignore errors */ },
        [&completed]() { completed = true; }
    );
    
    assert_equal(received_value, 42, "Observable::just should emit the value");
    assert_true(completed, "Observable::just should complete");
    
    // Test Observable::from_vector
    std::vector<int> values = {1, 2, 3, 4, 5};
    auto vector_observable = Observable<int>::from_vector(values);
    std::vector<int> received_values;
    
    auto vector_subscription = vector_observable.subscribe(
        [&received_values](const int& value) { received_values.push_back(value); }
    );
    
    assert_equal(received_values.size(), 5, "Should receive all values from vector");
    assert_equal(received_values[0], 1, "Should receive first value");
    assert_equal(received_values[4], 5, "Should receive last value");
    
    // Test map operator
    auto mapped_observable = vector_observable.map([](int x) { return x * 2; });
    std::vector<int> mapped_values;
    
    auto mapped_subscription = mapped_observable.subscribe(
        [&mapped_values](const int& value) { mapped_values.push_back(value); }
    );
    
    assert_equal(mapped_values.size(), 5, "Should receive all mapped values");
    assert_equal(mapped_values[0], 2, "Should receive doubled first value");
    assert_equal(mapped_values[4], 10, "Should receive doubled last value");
    
    // Test filter operator
    auto filtered_observable = vector_observable.filter([](int x) { return x % 2 == 0; });
    std::vector<int> filtered_values;
    
    auto filtered_subscription = filtered_observable.subscribe(
        [&filtered_values](const int& value) { filtered_values.push_back(value); }
    );
    
    assert_equal(filtered_values.size(), 2, "Should receive only even values");
    assert_equal(filtered_values[0], 2, "Should receive first even value");
    assert_equal(filtered_values[1], 4, "Should receive second even value");
}

// ===== INTEGRATION TESTS =====

void test_integration() {
    std::cout << "\n=== Testing Integration ===" << std::endl;
    
    // Test pipe with Maybe monad
    auto safe_parse_and_execute = pipe(
        parse_message,
        [](const Maybe<PinCommand>& maybe) -> Maybe<ExecutionResult> {
            return maybe >>= [](const PinCommand& cmd) -> Maybe<ExecutionResult> {
                auto result = execute_command(cmd);
                if (result.isRight()) {
                    return Maybe<ExecutionResult>::Just(result.fromRight(ExecutionResult{}));
                } else {
                    return Maybe<ExecutionResult>::Nothing();
                }
            };
        }
    );
    
    std::string valid_message = R"({"type":"digital","pin":13,"value":1})";
    auto result = safe_parse_and_execute(valid_message);
    
    assert_true(result.isJust(), "Integration pipeline should succeed with valid message");
    
    // Test pipe with Either monad
    auto safe_parse_with_error = pipe(
        parse_message,
        [](const Maybe<PinCommand>& maybe) -> Either<std::string, PinCommand> {
            if (maybe.isJust()) {
                return Either<std::string, PinCommand>::Right(maybe.fromJust());
            } else {
                return Either<std::string, PinCommand>::Left("Parse failed");
            }
        },
        [](const Either<std::string, PinCommand>& either) -> Either<std::string, ExecutionResult> {
            return either >>= execute_command;
        }
    );
    
    auto either_result = safe_parse_with_error(valid_message);
    assert_true(either_result.isRight(), "Either pipeline should succeed with valid message");
    
    auto error_result = safe_parse_with_error("invalid");
    assert_true(error_result.isLeft(), "Either pipeline should fail with invalid message");
}

// ===== MAIN TEST RUNNER =====

int main() {
    std::cout << "Starting Functional Programming Tests..." << std::endl;
    
    try {
        test_pipe_operator();
        test_compose_operator();
        test_maybe_monad();
        test_either_monad();
        test_io_monad();
        test_list_monad();
        test_functional_message_processor();
        test_reactive_streams();
        test_integration();
        
        std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
