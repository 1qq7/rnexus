#include <iostream>
#include <thread>
#include <chrono>
#include <rnexus/rnexus.hpp>

struct ImageFrame {
    int id;
};

int main() {
    using namespace rnexus;

    Topic<ImageFrame> imageTopic("camera.image");

    auto handle = subscribe(imageTopic, [](const ImageFrame& f) {
        std::cout << "[Subscriber] Received frame " << f.id << std::endl;
    });

    for (int i = 0; i < 5; ++i) {
        publish(imageTopic, ImageFrame{i});
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    unsubscribe(handle);
    std::cout << "Done." << std::endl;
}
