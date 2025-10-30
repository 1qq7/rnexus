# rnexus

**RNexus — A minimal ROS-like communication core for embedded robots.**

---

## 🚀 Overview

**rnexus** provides a minimal communication layer inspired by ROS 2's *pub/sub* and *service* model — but with **zero DDS**, **no external dependencies**, and **tiny runtime overhead**.

It’s designed for embedded or resource-constrained robotics systems such as:
- mobile robots, small UAVs
- smart speakers / IoT robots
- robotic arms on custom RTOS or micro-Linux

> Think of rnexus as “ROS-like messaging without ROS”.

---

## ✨ Features

- 🧩 **Typed Topics** — `Topic<T>` guarantees compile-time type safety.
- ⚙️ **Async Delivery** — messages dispatched via worker thread pool.
- 🧵 **Thread-Safe** — lock-guarded subscription table and queue.
- 📬 **Service RPC** — request/response built on top of topics.
- ⚡ **Lightweight** — single header, no dependencies.
- 🔌 **Easy to integrate** — drop-in for any C++17 project.

---

## 📦 Project Layout

```

rnexus/
├── include/
│   └── rnexus/rnexus.hpp   # main header (core + service)
├── examples/
│   └── basic_pubsub.cpp     # usage example for pub/sub
│   └── basic_service.cpp    # usage example for service
├── LICENSE
└── README.md

```

---

## 🧠 Example: Pub/Sub

```cpp
#include "rnexus/rnexus.hpp"
using namespace rnexus;

struct ImageFrame { int id; };

int main() {
    Topic<ImageFrame> imageTopic{"camera.image"};

    auto handle = subscribe(imageTopic, [](const ImageFrame& frame) {
        std::cout << "Received frame " << frame.id << std::endl;
    });

    for (int i = 0; i < 3; ++i)
        publish(imageTopic, ImageFrame{i});

    unsubscribe(handle);
}
````

---

## ⚙️ Example: Service

```cpp
#include "rnexus/rnexus.hpp"
using namespace rnexus;

// Define service request and response
struct AddRequest { int a, b; };
struct AddResponse { int sum; };

int main() {
    // Server: provides addition service
    ServiceServer<AddRequest, AddResponse> server("add", [](const AddRequest& req) {
        return AddResponse{ req.a + req.b };
    });

    // Client: calls the service synchronously
    ServiceClient<AddRequest, AddResponse> client("add");
    auto result = client.call({2, 3});

    if (result)
        std::cout << "Result = " << result->sum << std::endl;
    else
        std::cout << "Timeout" << std::endl;
}
```

---

## 🧩 API Overview

### Core Pub/Sub

| Function                    | Description                               |
| --------------------------- | ----------------------------------------- |
| `publish(topic, message)`   | Publish a message asynchronously.         |
| `subscribe(topic, handler)` | Register a callback for a specific topic. |
| `unsubscribe(handle)`       | Remove a subscription.                    |

### Services

| Type                        | Description                                               |
| --------------------------- | --------------------------------------------------------- |
| `ServiceServer<Req, Res>`   | Receives requests and publishes responses.                |
| `ServiceClient<Req, Res>`   | Sends requests (sync or async) and receives responses.    |
| `call(req, timeout_ms)`     | Blocking call with timeout, returns `std::optional<Res>`. |
| `async_call(req, callback)` | Non-blocking call, callback on response.                  |

---

## 🛠️ Build Example

```bash
mkdir build && cd build
cmake ..
make
./basic_pubsub
```

Requires **C++17 or higher**.

---

## 📄 License

Apache License © 2025 [[1qq7](https://github.com/1qq7)]

---

## 💡 Future Work

* [ ] Parameter & Action wrappers (ROS2-style)
* [ ] Logging and tracing utilities

---

> For robots too small to run ROS 2 — but still big enough to talk like one. 🤖
