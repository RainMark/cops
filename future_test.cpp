#include <iostream>
#include "future.h"

int main() {
    auto future = std::make_shared<cops::future_t<int>>();
    std::cout << future->has_value() << std::endl;
    future->set_value(100);
    std::cout << future->has_value() << std::endl;
    future->set_callback([]() {
        std::cout << "callback" << std::endl;
    });
    future->set_value(101);
    future->set_callback([]() {
        std::cout << "callback" << std::endl;
    });
    std::cout << future->value() << std::endl;

    auto future2 = std::make_shared<cops::future_t<float>>();
    future2->set_callback([]() {
        std::cout << "callback2" << std::endl;
    });
    future2->set_value(10.1);
    std::cout << future2->value() << std::endl;
    return 0;
}
