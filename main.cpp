#include <iostream>
#include <context.h>

int main() {
  std::unique_ptr<cops::coro_t> coro;
  std::unique_ptr<cops::coro_t> sub_coro;
  coro = cops::make_coro([&sub_coro]() {
      std::cout << "coro" << std::endl;
      cops::current->switch_out();
      sub_coro = cops::make_coro([]() {
          std::cout << "sub_coro" << std::endl;
          cops::current->switch_out();
          std::cout << "sub_coro exit" << std::endl;
        });
      sub_coro->switch_in();
      std::cout << "coro exit" << std::endl;
    });
  std::cout << "main" << std::endl;
  coro->switch_in();
  std::cout << "main" << std::endl;
  coro->switch_in();
  std::cout << "main" << std::endl;
  sub_coro->switch_in();
  std::cout << "exit" << std::endl;
  return 0;
}
