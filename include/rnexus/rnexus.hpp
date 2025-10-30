#pragma once
#include <unordered_map>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>
#include <memory>
#include <typeindex>
#include <iostream>
#include <any>
#include <optional>

namespace rnexus {

// ============================================================================
// Topic
// ----------------------------------------------------------------------------
// A strongly-typed topic name wrapper used for compile-time type safety.
// ============================================================================

template<typename T>
struct Topic {
    std::string name;
    explicit Topic(const std::string& n) : name(n) {}
};

// ============================================================================
// TopicHandle
// ----------------------------------------------------------------------------
// A handle returned by `subscribe()` used for unsubscription.
// ============================================================================

struct TopicHandle {
    std::string topicName;
    size_t id;
    bool operator==(const TopicHandle& other) const {
        return id == other.id && topicName == other.topicName;
    }
};

// ============================================================================
// WorkerQueue
// ----------------------------------------------------------------------------
// Simple thread pool for asynchronous task execution.
// Used internally by PubSub to deliver messages in parallel.
// ============================================================================

class WorkerQueue {
public:
    explicit WorkerQueue(size_t n = std::thread::hardware_concurrency()) : stop(false) {
        for (size_t i = 0; i < n; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> job;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        cond.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        job = std::move(tasks.front());
                        tasks.pop();
                    }
                    job();
                }
            });
        }
    }

    template<typename F>
    void enqueue(F&& f) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            tasks.emplace(std::forward<F>(f));
        }
        cond.notify_one();
    }

    ~WorkerQueue() {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stop = true;
        }
        cond.notify_all();
        for (auto& w : workers) w.join();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable cond;
    bool stop;
};

// ============================================================================
// PubSub Core
// ----------------------------------------------------------------------------
// Central message bus that manages all topics and subscribers.
// Supports typed publish/subscribe with asynchronous dispatch.
// ============================================================================

class PubSub {
public:
    static PubSub& instance() {
        static PubSub inst;
        return inst;
    }

    // Publish a message to a topic.
    template<typename T>
    void publish(const Topic<T>& topic, const T& message) {
        std::unordered_map<size_t, std::function<void(const T&)>> subsCopy;
        {
            std::lock_guard<std::mutex> lock(mtx);
            auto it = subscribers.find(topic.name);
            if (it != subscribers.end()) {
                for (auto& entry : it->second) {
                    auto func = std::any_cast<std::function<void(const T&)>>(&entry.second);
                    if (func) subsCopy[entry.first] = *func;
                }
            }
        }
        for (auto& pair : subsCopy) {
            auto sub = pair.second;
            worker.enqueue([sub, message]() { sub(message); });
        }
    }

    // Subscribe a callback to a topic.
    template<typename T, typename F>
    TopicHandle subscribe(const Topic<T>& topic, F&& handler) {
        std::lock_guard<std::mutex> lock(mtx);
        size_t id = ++counter;
        subscribers[topic.name][id] = std::function<void(const T&)>(std::forward<F>(handler));
        return {topic.name, id};
    }

    // Remove a subscription using its handle.
    void unsubscribe(const TopicHandle& handle) {
        std::lock_guard<std::mutex> lock(mtx);
        if (subscribers.count(handle.topicName))
            subscribers[handle.topicName].erase(handle.id);
    }

private:
    PubSub() = default;
    std::mutex mtx;
    std::unordered_map<std::string, std::unordered_map<size_t, std::any>> subscribers;
    std::atomic<size_t> counter{0};
    WorkerQueue worker;
};

// ============================================================================
// Shorthand API
// ----------------------------------------------------------------------------
// Convenient global wrappers for the singleton PubSub instance.
// ============================================================================

template<typename T, typename F>
TopicHandle subscribe(const Topic<T>& topic, F&& handler) {
    return PubSub::instance().subscribe(topic, std::forward<F>(handler));
}

template<typename T>
void publish(const Topic<T>& topic, const T& message) {
    PubSub::instance().publish(topic, message);
}

inline void unsubscribe(const TopicHandle& handle) {
    PubSub::instance().unsubscribe(handle);
}

} // namespace rnexus


// ============================================================================
// Service RPC Framework
// ----------------------------------------------------------------------------
// Provides request/response (service) abstraction built on top of PubSub.
// - ServiceServer handles incoming requests and publishes responses.
// - ServiceClient provides synchronous and asynchronous service calls.
// ============================================================================

namespace rnexus {

// --- Common message headers -------------------------------------------------

struct ServiceHeader {
    uint64_t req_id;  // Unique request identifier
};

// Message wrappers for request and response types
template <typename Req>
struct ServiceRequest {
    ServiceHeader header;
    Req data;
};

template <typename Res>
struct ServiceResponse {
    ServiceHeader header;
    Res data;
};

// ============================================================================
// ServiceServer
// ----------------------------------------------------------------------------
// Listens to "srv_req_<name>", executes handler(req), and publishes
// the response on "srv_res_<name>".
// ============================================================================

template <typename Req, typename Res>
class ServiceServer {
public:
    ServiceServer(const std::string& name, std::function<Res(const Req&)> handler)
        : name_(name),
          handler_(std::move(handler)),
          req_topic_("srv_req_" + name),
          res_topic_("srv_res_" + name)
    {
        handle_ = PubSub::instance().subscribe<ServiceRequest<Req>>(req_topic_,
            [this](const ServiceRequest<Req>& req_msg) {
                Res res_data = handler_(req_msg.data);
                ServiceResponse<Res> res_msg{ {req_msg.header.req_id}, std::move(res_data) };
                PubSub::instance().publish(res_topic_, res_msg);
            });
    }

    ~ServiceServer() {
        // Automatically clean up the subscription when destroyed
        PubSub::instance().unsubscribe(handle_);
    }

private:
    std::string name_;
    std::function<Res(const Req&)> handler_;
    Topic<ServiceRequest<Req>> req_topic_;
    Topic<ServiceResponse<Res>> res_topic_;
    TopicHandle handle_;
};

// ============================================================================
// ServiceClient
// ----------------------------------------------------------------------------
// Provides synchronous (blocking) and asynchronous (callback-based)
// service request interfaces.
// ============================================================================

template <typename Req, typename Res>
class ServiceClient {
public:
    explicit ServiceClient(const std::string& name)
        : name_(name),
          req_topic_("srv_req_" + name),
          res_topic_("srv_res_" + name),
          next_id_(1)
    {}

    // ------------------------------------------------------------------------
    // call()
    // ------------------------------------------------------------------------
    // Sends a request and waits for the corresponding response.
    // Returns std::optional<Res>, which is empty if timed out.
    // ------------------------------------------------------------------------
    std::optional<Res> call(const Req& req, int timeout_ms = 1000) {
        uint64_t req_id = next_id_.fetch_add(1);

        ServiceRequest<Req> req_msg{ {req_id}, req };

        std::mutex mtx;
        std::condition_variable cv;
        bool received = false;
        Res response{};

        TopicHandle handle = PubSub::instance().subscribe<ServiceResponse<Res>>(res_topic_,
            [&](const ServiceResponse<Res>& res_msg) {
                if (res_msg.header.req_id != req_id) return;
                {
                    std::lock_guard<std::mutex> lk(mtx);
                    response = res_msg.data;
                    received = true;
                }
                cv.notify_one();
            });

        PubSub::instance().publish(req_topic_, req_msg);

        std::unique_lock<std::mutex> lk(mtx);
        bool ok = cv.wait_for(lk, std::chrono::milliseconds(timeout_ms),
                              [&] { return received; });

        PubSub::instance().unsubscribe(handle);

        if (!ok)
            return std::nullopt;  // Timeout
        else
            return response;      // Success
    }

    // ------------------------------------------------------------------------
    // async_call()
    // ------------------------------------------------------------------------
    // Sends a request and invokes the callback when the matching response
    // is received. Automatically unsubscribes afterward.
    // ------------------------------------------------------------------------
    void async_call(const Req& req, std::function<void(const Res&)> callback) {
        uint64_t req_id = next_id_.fetch_add(1);
        ServiceRequest<Req> req_msg{ {req_id}, req };

        auto handlePtr = std::make_shared<TopicHandle>();

        TopicHandle h = PubSub::instance().subscribe<ServiceResponse<Res>>(res_topic_,
            [handlePtr, req_id, callback](const ServiceResponse<Res>& res_msg) {
                if (res_msg.header.req_id != req_id) return;
                callback(res_msg.data);
                if (!handlePtr->topicName.empty())
                    PubSub::instance().unsubscribe(*handlePtr);
            });

        *handlePtr = h;
        PubSub::instance().publish(req_topic_, req_msg);
    }

private:
    std::string name_;
    Topic<ServiceRequest<Req>> req_topic_;
    Topic<ServiceResponse<Res>> res_topic_;
    std::atomic<uint64_t> next_id_;
};

} // namespace rnexus
