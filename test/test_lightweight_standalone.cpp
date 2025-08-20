#include <iostream>
#include <functional>
#include <vector>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <optional>
#include <cassert>

// Mock ESP32 functions
#define ESP_LOGI(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("[%s] ERROR: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("[%s] WARN: " fmt "\n", tag, ##__VA_ARGS__)

// Simple functional utilities (C++17 compatible)
namespace fp {

// Identity function
template<typename T>
T id(T&& x) {
    return std::forward<T>(x);
}

// Pipe operator (simplified for C++17)
template<typename F>
auto pipe(F&& f) {
    return std::forward<F>(f);
}

template<typename F, typename G>
auto pipe(F&& f, G&& g) {
    return [f = std::forward<F>(f), g = std::forward<G>(g)](auto&& x) {
        return g(f(std::forward<decltype(x)>(x)));
    };
}

template<typename F, typename G, typename H>
auto pipe(F&& f, G&& g, H&& h) {
    return [f = std::forward<F>(f), g = std::forward<G>(g), h = std::forward<H>(h)](auto&& x) {
        return h(g(f(std::forward<decltype(x)>(x))));
    };
}

// Maybe monad
template<typename T>
class Maybe {
private:
    std::optional<T> value_;

public:
    Maybe() : value_(std::nullopt) {}
    explicit Maybe(const T& value) : value_(value) {}
    
    static Maybe<T> Just(const T& value) {
        return Maybe<T>(value);
    }
    
    static Maybe<T> Nothing() {
        return Maybe<T>();
    }
    
    bool isJust() const { return value_.has_value(); }
    bool isNothing() const { return !value_.has_value(); }
    
    T fromJust() const { return value_.value(); }
    T fromMaybe(const T& default_value) const {
        return value_.value_or(default_value);
    }
    
    template<typename F>
    auto fmap(F&& f) const -> Maybe<std::invoke_result_t<F, T>> {
        if (isNothing()) {
            return Maybe<std::invoke_result_t<F, T>>::Nothing();
        }
        return Maybe<std::invoke_result_t<F, T>>::Just(f(fromJust()));
    }
};

// Either monad
template<typename L, typename R>
class Either {
private:
    enum class Tag { Left, Right };
    Tag tag_;
    union {
        L left_;
        R right_;
    };
    
    void destroy() {
        if (tag_ == Tag::Left) {
            left_.~L();
        } else if (tag_ == Tag::Right) {
            right_.~R();
        }
    }
    
public:
    Either(const L& left) : tag_(Tag::Left), left_(left) {}
    Either(const R& right) : tag_(Tag::Right), right_(right) {}
    
    ~Either() {
        destroy();
    }
    
    static Either<L, R> Left(const L& left) {
        return Either<L, R>(left);
    }
    
    static Either<L, R> Right(const R& right) {
        return Either<L, R>(right);
    }
    
    bool isLeft() const { return tag_ == Tag::Left; }
    bool isRight() const { return tag_ == Tag::Right; }
    
    L fromLeft(const L& default_value) const {
        return isLeft() ? left_ : default_value;
    }
    
    R fromRight(const R& default_value) const {
        return isRight() ? right_ : default_value;
    }
};

} // namespace fp

// Lightweight Observable (simplified)
namespace lwr {

template<typename T>
class Observable {
private:
    std::function<void(std::function<void(const T&)>)> subscribe_func_;

public:
    Observable() = default;
    
    explicit Observable(std::function<void(std::function<void(const T&)>)> subscribe_func)
        : subscribe_func_(std::move(subscribe_func)) {}
    
    void subscribe(std::function<void(const T&)> observer) {
        if (subscribe_func_) {
            subscribe_func_(observer);
        }
    }
    
    template<typename F>
    auto map(F&& mapper) const -> Observable<std::invoke_result_t<F, T>> {
        return Observable<std::invoke_result_t<F, T>>([this, mapper = std::forward<F>(mapper)](std::function<void(const std::invoke_result_t<F, T>&)> observer) {
            subscribe_func_([observer, mapper](const T& value) {
                observer(mapper(value));
            });
        });
    }
    
    template<typename F>
    auto filter(F&& predicate) const -> Observable<T> {
        return Observable<T>([this, predicate = std::forward<F>(predicate)](std::function<void(const T&)> observer) {
            subscribe_func_([observer, predicate](const T& value) {
                if (predicate(value)) {
                    observer(value);
                }
            });
        });
    }
    
    Observable<T> take(int count) const {
        return Observable<T>([this, count](std::function<void(const T&)> observer) {
            int taken = 0;
            subscribe_func_([observer, count, &taken](const T& value) {
                if (taken < count) {
                    observer(value);
                    taken++;
                }
            });
        });
    }
    
    static Observable<T> just(const T& value) {
        return Observable<T>([value](std::function<void(const T&)> observer) {
            observer(value);
        });
    }
    
    static Observable<T> from_vector(const std::vector<T>& values) {
        return Observable<T>([values](std::function<void(const T&)> observer) {
            for (const auto& value : values) {
                observer(value);
            }
        });
    }
};

template<typename T>
class Subject {
private:
    std::vector<std::function<void(const T&)>> observers_;
    std::mutex observers_mutex_;

public:
    void on_next(const T& value) {
        std::lock_guard<std::mutex> lock(observers_mutex_);
        for (auto& observer : observers_) {
            observer(value);
        }
    }
    
    void subscribe(std::function<void(const T&)> observer) {
        std::lock_guard<std::mutex> lock(observers_mutex_);
        observers_.push_back(observer);
    }
    
    Observable<T> as_observable() const {
        return Observable<T>([this](std::function<void(const T&)> observer) {
            const_cast<Subject<T>*>(this)->subscribe(observer);
        });
    }
};

} // namespace lwr

void test_functional_utils() {
    std::cout << "Testing Functional Utils..." << std::endl;
    
    // Test pipe operator
    auto add_one = [](int x) { return x + 1; };
    auto multiply_by_two = [](int x) { return x * 2; };
    auto square = [](int x) { return x * x; };
    
    auto pipeline = fp::pipe(add_one, multiply_by_two, square);
    int result = pipeline(3); // (3 + 1) * 2 = 8, then 8^2 = 64
    assert(result == 64);
    std::cout << "✓ Pipe operator works" << std::endl;
    
    // Test Maybe monad
    auto just_value = fp::Maybe<int>::Just(42);
    auto nothing_value = fp::Maybe<int>::Nothing();
    
    assert(just_value.isJust());
    assert(nothing_value.isNothing());
    assert(just_value.fromJust() == 42);
    assert(nothing_value.fromMaybe(0) == 0);
    std::cout << "✓ Maybe monad works" << std::endl;
    
    // Test Either monad
    auto right_value = fp::Either<std::string, int>::Right(42);
    auto left_value = fp::Either<std::string, int>::Left("error");
    
    assert(right_value.isRight());
    assert(left_value.isLeft());
    assert(right_value.fromRight(0) == 42);
    assert(left_value.fromLeft("default") == "error");
    std::cout << "✓ Either monad works" << std::endl;
}

void test_lightweight_observable() {
    std::cout << "Testing Lightweight Observable..." << std::endl;
    
    // Test just
    auto single_obs = lwr::Observable<int>::just(42);
    int received_value = 0;
    single_obs.subscribe([&received_value](const int& value) {
        received_value = value;
    });
    assert(received_value == 42);
    std::cout << "✓ Observable::just works" << std::endl;
    
    // Test from_vector
    std::vector<int> values = {1, 2, 3, 4, 5};
    auto vector_obs = lwr::Observable<int>::from_vector(values);
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
}

void test_lightweight_subject() {
    std::cout << "Testing Lightweight Subject..." << std::endl;
    
    lwr::Subject<int> subject;
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

int main() {
    std::cout << "=== Testing Lightweight Reactive Library (Standalone) ===" << std::endl;
    
    try {
        test_functional_utils();
        test_lightweight_observable();
        test_lightweight_subject();
        
        std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
