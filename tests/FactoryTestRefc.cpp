#include <cassert>
#include <iostream>

#include "mem/SubtypeAllocator/SubtypeFactory.hpp"

struct DoubleWrapper : public mem::enable_refc_from_this<DoubleWrapper> {
  double value;

public:
  DoubleWrapper(double d) : value(d) {}
  friend std::ostream &operator<<(std::ostream &os, const DoubleWrapper &dw) {
    return os << dw.value;
  }
};

struct A {
  virtual void printA() = 0;
  virtual ~A() = default;
};
struct B {
  virtual void printB() = 0;
  virtual ~B() = default;
};
struct C : public A, public B {
  void printA() override { std::cout << "Hello from A\n"; }
  void printB() override { std::cout << "Hello from B\n"; }
};

void foo() {
  mem::RefcFactory<1024, C> Factory;
  auto shared_C = Factory.create<C>();

  mem::refc<A> shared_A = shared_C;
  // mem::refc<B> shared_B = shared_C;

  shared_C->printA();
  shared_A->printA();
  shared_C->printB();
}

int main() {

  mem::RefcFactory<1024, int, long long, DoubleWrapper> Factory;

  {
    auto shared_int = Factory.create<int>(42);
    auto shared_long = Factory.create<long long>(44567);

    std::cout << "value1:  " << *shared_int << std::endl;
    std::cout << "value2:  " << *shared_long << std::endl;
  }
  auto shared_double = Factory.create<DoubleWrapper>(24.42);
  std::cout << "value3:  " << *shared_double << std::endl;

  auto *Ptr = shared_double.get();

  auto shared_double_cpy = Ptr->refc_from_this();

  assert(shared_double == shared_double_cpy);

  std::cout << "value3': " << *shared_double_cpy << std::endl;

  static mem::refc<int>::singleton static_int(435761);
  mem::refc<int> shared_static_int(static_int);
  std::cout << "value4:  " << *shared_static_int << std::endl;

  foo();
}