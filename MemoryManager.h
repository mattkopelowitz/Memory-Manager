#include <functional>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <climits>
using namespace std;

int bestFit(int sizeInWords, void* list);
int worstFit(int sizeInWords, void* list);

struct Chunk {
    int offset;
    int length;
    bool isHole;

    Chunk(int off, int len, bool hole) {
        offset = off;
        length = len;
        isHole = hole;
    }
};

class MemoryManager {
    char* block = nullptr; //memory block
    size_t blockSize;
    unsigned wordSize;
    function<int(int, void*)> allocator; //allocator function being used
    vector<Chunk> chunks; //vector of data chunks
    bool initialized = false;

public:
    MemoryManager(unsigned wordSize, std::function<int(int, void *)> allocator);

    ~MemoryManager();

    void initialize(size_t sizeInWords);

    void shutdown();

    void* allocate(size_t sizeInBytes);

    void free(void *address);

    void setAllocator(std::function<int(int, void *)> allocator);

    int dumpMemoryMap(char *filename);

    void* getList();

    void* getBitmap();

    unsigned getWordSize();

    void* getMemoryStart();

    unsigned getMemoryLimit();
};
