# Pool Allocator

This repository contains two custom allocators for C++17 that can speedup node-based STL containers such as `std::list`, `std::set`, etc. as well as custom object-graphs using `std::shared_ptr`.
This is a header-only library, so just add the `include` folder to your project's include-path.

The small tests can be compiled using the provided Makefile.
Note, it assumes a clang compiler, so if you prefer using a different C++ compiler, just make the changes directly in the makefile.

# Features

- `PoolAllocator`: Drop-in replacement for `std::allocator` in STL node-based containers. Allocates a fixed chunk of memory at once and optionally uses a free-list to manage deallocated objects.
- `SubtypeAllocator`: Similar to `PoolAllocator`, but allows reusing the same memory-pool with multiple `SubtypeAllocator`s. Can be used with `std::allocate_shared`.
- `SubtypeAllocatorDriver`: A memory-pool that can be shared across multiple `SubtypeAllocator`s. Always uses a free-list for deallocated objects.
- `refc`: A custom implementation of `std::shared_ptr` optimized for use with `SubtypeAllocatorDriver`. Is faster and consumes less memory compared to a `std::shared_ptr` used with a custom allocator, but has a restriction: 

    `refc` does not work with multiple inheritance, i.e. if `U` is subtype of `T`, then a `refc<U>` can only be assigned to `refc<T>`, if `T` is the first base class in `U`'s inheritance list (or recursively the first one in `U`'s first base-class' inheritance list).
    This is, because `refc` requires `static_cast` not to do any pointer arithmetics.
    Note: Virtual inheritance is also problematic.
- `RefcFactory`: A factory class that can allocate objects of a fixed set of types with a self-managed `SubtypeAllocatorDriver` returning a `refc`
- `SharedPtrFactory`: A factory class similar to `RefcFactory`, but returns `std::shared_ptr`s created with `std::allocate_shared`. It uses a special-purpose allocator wrapper similar to `SubtypeAllocator` under the hood that increases the (de-)allocation performance compared to `std::allocatr_shared` with a normal `SubtypeAllocator`.
- `DefaultSharedPtrFactory`: A compatibility-class that can allocate objects of a fixed set of types with `std::make_shared`.

All provided allocators can customize the size of objects allocated at once using a template parameter.
Currently, this parameter defaults to 1024 objects.

Caution: If you use an allocator that takes a pointer to `SubtypeAllocatorDriver` in its constructor, make sure that the `SubtypeAllocatorDriver` lives longer than all of the objects allocated through it.
Similarly, make sure that the `RefcFactory` and `SharedPtrFactory` objects live longer than all objects allocated with them.

# When to use which allocator?

This library provides a number of pool allocators.
The below list gives a hint on when to use which of them:

- For speeding up standard node-based containers, use `PoolAllocator`. It will create a separate pool for each container-object which is fine in most cases. If the pool should be shared for multiple containers, use `SubtypeAllocator` with a shared `SubtypeAllocatorDriver`.
- For allocating objects that should be managed with a smart-pointer, ask the following questions:
    - Do you need virtual inheritance? If yes, use `SharedPtrFactory`.
    - Do you need multiple inheritance? If no, use `RefcFactory`.
    - If yes, do you need upcasts to arbitrary base-classes? If no, use `RefcFactory`.
    - Otherwise use `SharedPtrFactory`.
    - Do you want to prepare your project that currently uses `std::shared_ptr` with `std::make_shared` for migration with pool-allocators? Start with `DefaultSharedPtrFactory` and then switch to either `SharedPtrFactory` or `RefcFactory`.

A note about `std::unique_ptr`: You can use `SubtypeAllocator` to allocate your objects and then wrap them in a `std::unique_ptr` with a custom deleter that delegates to the `SubtypeAllocator`'s `deallocate` function. 
However, the same restrictions for multiple inheritance exist like for `refc`.

# Examples

```C++
#include <mem/PoolAllocator.hpp>
...

std::list<T, mem::PoolAllocator<T>> myList;
std::set<T, std::less<T>, mem::PoolAllocator<T>> mySet;
std::unordered_map<T, U, std::hash<T>, std::equal_to<T>, mem::PoolAllocator<std::pair<const T, U>>> myHashMap;
...
```

```C++
#include <mem/SubtypeAllocator/SubtypeAllocator.hpp>

...

mem::SubtypeAllocatorDriver<> driver;

// Reuse the same driver for allocating different types
// Note, that this takes linear time in the number of different types used with th driver in the worst case
// for both allocation and deallocation.
auto shared_T = std::allocate_shared<T>(mem::SubtypeAllocator<T>(&Driver), ...)
auto shared_U = std::allocate_shared<U>(mem::SubtypeAllocator<U>(&Driver), ...)
```

```C++
#include <mem/SubtypeAllocator/SubtypeFactory.hpp>

...

mem::SharedPtrFactory<1024, T, U> factory;

// Same as the example above with allocate_shared, but but allocation and 
// deallocation here take (armortized) constant time.
// Note, that the allocation-block size must be specified explicitly here
auto shared_T = factory.create<T>(...);
auto shared_U = factory.create<U>(...);
```

```C++
#include <mem/SubtypeAllocator/SubtypeFactory.hpp>

...

mem::RefcFactory<1024, T, U> factory;

// Same as the example above with the SharedPtrFactory, but
// the refc<T>/refc<U> take less memory than shared_ptr<T>/shared_ptr<U>
auto shared_T = factory.create<T>(...);
auto shared_U = factory.create<U>(...);
```

```C++
#include <mem/SubtypeAllocator/SubtypeFactory.hpp>

...

class A{...};
class B{...};
class C: public A, public B{...}
...

mem::RefcFactory<1024, A, B, C> factory;

mem::refc<A> shared_A = factory.create<A>(...);
mem::refc<B> shared_B = factory.create<B>(...);
mem::refc<C> shared_C = factory.create<C>(...);

shared_A = shared_C; // works well
shared_B = shared_C; // error: B is second in the inheritance-list of C, so beware assigning a refc<C> to refc<B>.
                     // In debug mode, it will trigger an assertion.
                     // To enable this conversion, use mem::SharedPtrFactory and std::shared_ptr instead
```
