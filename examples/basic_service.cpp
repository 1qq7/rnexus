#include <rnexus/rnexus.hpp>
#include <iostream>
#include <thread>

struct AddRequest {
    int a, b;
};

struct AddResponse {
    int sum;
};

int main() {
    using namespace rnexus;

    // 启动服务端
    ServiceServer<AddRequest, AddResponse> server("add_two_ints", [](const AddRequest& req) {
        AddResponse res;
        res.sum = req.a + req.b;
        std::cout << "[Server] Received: " << req.a << " + " << req.b << " = " << res.sum << std::endl;
        return res;
    });

    // 启动客户端
    ServiceClient<AddRequest, AddResponse> client("add_two_ints");

    // 异步调用
    client.async_call({1, 2}, [](const AddResponse& res) {
        std::cout << "[Client] Got response (async): " << res.sum << std::endl;
    });

    // 同步调用
    auto res = client.call({5, 7});
    if (res.has_value()) {
        std::cout << "[Client] Got response (sync): " << res.value().sum << std::endl;
    } else {
        std::cout << "[Client] Sync call timed out or failed." << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return 0;
}
