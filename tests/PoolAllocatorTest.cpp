#include <list>

#include "mem/PoolAllocator.hpp"

int main() {
  std::list<mem::PoolAllocator<int>> pool;
  pool.push_back(4);
}