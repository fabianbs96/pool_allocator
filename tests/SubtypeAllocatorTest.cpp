#include <iostream>
#include <memory>

#include "mem/SubtypeAllocator/SubtypeAllocator.hpp"

int main() {
  mem::SubtypeAllocatorDriver<1024> Driver;
  {
    auto shared_int =
        std::allocate_shared<int>(mem::SubtypeAllocator<int>(&Driver), 42);
    auto shared_long = std::allocate_shared<long long>(
        mem::SubtypeAllocator<long long>(&Driver), 44567);
    std::cout << "value1: " << *shared_int << std::endl;
    std::cout << "value2: " << *shared_long << std::endl;
  }
  auto shared_double = std::allocate_shared<double>(
      mem::SubtypeAllocator<double>(&Driver), 24.42);
  std::cout << "value3: " << *shared_double << std::endl;
}