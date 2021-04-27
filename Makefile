CXX := clang++

all:
	$(CXX) -o PoolAllocatorTest 		-std=c++17 -I include/ tests/PoolAllocatorTest.cpp
	$(CXX) -o SubtypeAllocatorTest 	-std=c++17 -I include/ tests/SubtypeAllocatorTest.cpp
	$(CXX) -o FactoryTestRefc 		-std=c++17 -I include/ tests/FactoryTestRefc.cpp
	$(CXX) -o FactoryTestShared 		-std=c++17 -I include/ tests/FactoryTestShared.cpp

clean:
	rm -f PoolAllocatorTest
	rm -f SubtypeAllocatorTest
	rm -f FactoryTestRefc
	rm -f FactoryTestShared
