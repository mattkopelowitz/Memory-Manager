MemoryManager: MemoryManager.cpp
	g++ -c -o MemoryManager.o MemoryManager.cpp
	ar cr libMemoryManager.a MemoryManager.o