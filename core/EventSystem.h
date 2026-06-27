#pragma once
// =============================================================================
// core/EventSystem.h — C++23 Type-Safe Event Bus
// std::move_only_function, std::type_index, std::format
// =============================================================================
#include <vector>
#include <map>
#include <mutex>
#include <typeindex>
#include <memory>
#include <functional>
#include <format>
#include <string_view>

// =============================================================================
// EVENT BUS
// =============================================================================
class EventBus {
    struct IHandler {
        virtual ~IHandler() = default;
        [[nodiscard]] virtual std::type_index EventType() const noexcept = 0;
    };

    template<typename T>
    struct Handler : IHandler {
        std::move_only_function<void(const T&)> callback;
        explicit Handler(std::move_only_function<void(const T&)> cb) : callback(std::move(cb)) {}
        [[nodiscard]] std::type_index EventType() const noexcept override { return typeid(T); }
    };

    mutable std::mutex mutex;
    std::map<std::type_index, std::vector<std::unique_ptr<IHandler>>> subscribers;

public:
    EventBus() = default;
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    template<typename T>
    void Subscribe(std::move_only_function<void(const T&)> callback) {
        static_assert(std::is_copy_constructible_v<T> || std::is_trivially_copyable_v<T>,
                      "Event type must be copyable");
        std::lock_guard lock(mutex);
        auto it = subscribers.find(typeid(T));
        if (it == subscribers.end())
            it = subscribers.emplace(typeid(T), std::vector<std::unique_ptr<IHandler>>{}).first;
        it->second.push_back(std::make_unique<Handler<T>>(std::move(callback)));
    }

    template<typename T>
    void Publish(const T& event) const {
        std::lock_guard lock(mutex);
        auto it = subscribers.find(typeid(T));
        if (it == subscribers.end()) return;
        auto handlers = it->second;
        for (auto& h : handlers) {
            auto* typed = static_cast<Handler<T>*>(h.get());
            if (typed && typed->callback) typed->callback(event);
        }
    }

    template<typename T>
    [[nodiscard]] bool HasSubscribers() const {
        std::lock_guard lock(mutex);
        auto it = subscribers.find(typeid(T));
        return it != subscribers.end() && !it->second.empty();
    }

    void Clear() {
        std::lock_guard lock(mutex);
        subscribers.clear();
    }
};

// Global Event Bus
inline EventBus gEventBus;
