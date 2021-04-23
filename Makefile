all:
	clang++ -o PoolAllocatorTest 		-std=c++17 -I include/ tests/PoolAllocatorTest.cpp
	clang++ -o SubtypeAllocatorTest 	-std=c++17 -I include/ tests/SubtypeAllocatorTest.cpp
	clang++ -o FactoryTestRefc 		-std=c++17 -I include/ tests/FactoryTestRefc.cpp
	clang++ -o FactoryTestShared 		-std=c++17 -I include/ tests/FactoryTestShared.cpp

clean:
	rm -f PoolAllocatorTest
	rm -f SubtypeAllocatorTest
	rm -f FactoryTestRefc
	rm -f FactoryTestShared
