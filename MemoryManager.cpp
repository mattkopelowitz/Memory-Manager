#include "MemoryManager.h"

int bestFit(int sizeInWords, void* list) {
    uint16_t* holeList = (uint16_t*)list;
    if (holeList == nullptr) {
        return -1;
    }

    //iterates through each hole given in list and determines if it is large enough for sizeInWords and if it is the smallest hole
    int offset = -1;
    int size = INT_MAX;
    int index = 2;
    for (int i = 0; i < holeList[0]; i++) {
        if (holeList[index] >= sizeInWords && holeList[index] <= size) {
            size = holeList[index];
            offset = holeList[index-1];
        }
        index += 2;
    }
    return offset;
}
int worstFit(int sizeInWords, void* list) {
    uint16_t* holeList = (uint16_t*)list;
    if (holeList == nullptr) {
        return -1;
    }

    //iterates through each hole given in list and determines if it is large enough for sizeInWords and if it is the biggest hole
    int offset = -1;
    int size = 0;
    int index = 2;
    for (int i = 0; i < holeList[0]; i++) {
        if (holeList[index] >= sizeInWords && holeList[index] >= size) {
            size = holeList[index];
            offset = holeList[index-1];
        }
        index += 2;
    }
    return offset;
}



//constructor
MemoryManager::MemoryManager(unsigned wordSize, std::function<int(int, void*)> allocator) {
    this->wordSize = wordSize;
    this->allocator = allocator;
}

//destructor
MemoryManager::~MemoryManager() {
    shutdown();
}

//initialize the block of memory
void MemoryManager::initialize(size_t sizeInWords) {
    if (block != nullptr) {
        shutdown();
    }
    if (initialized) {
        shutdown();
    }
    //set the block size and create a new memory block and chunk of memory as a hole in the vector
    if (sizeInWords < 65536) {
        blockSize = wordSize * sizeInWords;
        block = new char[blockSize];
        Chunk chunk = Chunk(0, sizeInWords, true);
        chunks.push_back(chunk);
        initialized = true;
    }
}

//dynamically deletes all memory allocated
void MemoryManager::shutdown() {
    if (initialized) {
        delete[] block;
        block = nullptr;
        chunks.clear();
        initialized = false;
    } 
}

//allocate memory on the block and determines holes
void* MemoryManager::allocate(size_t sizeInBytes) {
    //calculates size on the memory block to allocate
    int wordsToAllocate;
    if (sizeInBytes % getWordSize() != 0) {
        wordsToAllocate = sizeInBytes/getWordSize() + 1;
    } else {
        wordsToAllocate = sizeInBytes/getWordSize();
    }

    uint16_t* holeList = (uint16_t*)getList();

    //set the fit algorithm and find the offset of where to place the new chunk
    int offset = allocator(wordsToAllocate, holeList);
    delete[] holeList;
    if (offset == -1) {
        return nullptr;
    }

    Chunk chunk = Chunk(offset, wordsToAllocate, false);
    
    //finds the hole chunk in chunks vector
    int index;
    for (int i = 0; i < chunks.size(); i++) {
        if (chunks[i].offset == chunk.offset) {
            index = i;
            break;
        }
    }
    
    //if the new chunk takes up the length of the hole (delete hole)
    if (chunk.length == chunks[index].length) {
        chunks.insert(chunks.begin()+index, chunk);
        chunks.erase(chunks.begin() + (index+1));
    } else {
        //shorten hole
        chunks[index].offset += chunk.length;
        chunks[index].length -= chunk.length;
        chunks.insert(chunks.begin()+index, chunk);
    }

    return block + (offset*getWordSize());
}

//frees the provided memory block and merges any holes
void MemoryManager::free(void* address) {
    int offset = ((char*)address - block)/getWordSize();

    //finds the chunk at the offset
    int index = -1;
    for (int i = 0; i < chunks.size(); i++) {
        if (!chunks[i].isHole && chunks[i].offset == offset) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return;
    }
    
    chunks[index].isHole = true;

    //if to the right is a hole merge    
    if (index+1 < chunks.size() && chunks[index+1].isHole) {
        chunks[index].length += chunks[index+1].length;
        chunks.erase(chunks.begin() + index + 1);
        chunks[index].isHole = true;
    }
    
    //if to the left is a hole merge
    if (index-1 >= 0 && chunks[index-1].isHole) {
        chunks[index].offset = chunks[index-1].offset;
        chunks[index].length += chunks[index-1].length;
        chunks[index].isHole = true;
        chunks.erase(chunks.begin()+index-1);
    }

}

void MemoryManager::setAllocator(std::function<int(int, void*)> allocator) {
    this->allocator = allocator;
}

int MemoryManager::dumpMemoryMap(char* filename) {
    //open file descriptor
    int fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0777);
    if (fd == -1) {
        return -1;
    }

    //get list of holes and format the output string
    uint16_t* holeList = (uint16_t*)getList();
    string output;
    int index = 1;
    for (int i = 0; i < holeList[0]; i++) {
        int start = holeList[index++];
        int length = holeList[index++];
        output += "[" + to_string(start) + ", " + to_string(length) + "] - ";
    }
    output = output.substr(0, output.size()-3);

    //write the posix call
    const char* call = output.c_str();
    write(fd, call, output.length());

    close(fd);
    delete[] holeList;

    return 0;
}

void* MemoryManager::getList() {
    //determines how many holes
    int holesCount = 0;
    vector<Chunk> holes;
    for (int i = 0; i < chunks.size(); i++) {
        if (chunks[i].isHole) {
            holesCount++;
            holes.push_back(chunks[i]);
        }
    }

    int size = holesCount * 2 + 1;

    uint16_t* ret = new uint16_t[size];
    ret[0] = holesCount;

    int index = 1;
    for (int i = 0; i < holes.size(); i++) {
        ret[index++] = holes[i].offset;
        ret[index++] = holes[i].length;
    }

    return ret;
}

void* MemoryManager::getBitmap() {
    //records all the bits into a vector
    vector<int> bitmap;
    for (int i = 0; i < chunks.size(); i++) {
        for (int j = 0; j < chunks[i].length; j++) {
            if (chunks[i].isHole) {
                bitmap.push_back(0);
            } else {
                bitmap.push_back(1);
            }
        }
    }

    //extends the bitmap to a multiple of 8 if needed
    if (bitmap.size() % 8 != 0) {
        int bit = 0;
        if (bitmap[bitmap.size()-1] == 1) {
            bit = 1;
        }
        for (int i = 0; i < (bitmap.size()%8); i++) {
            bitmap.push_back(bit);
        }
    }

    uint16_t numBytes = bitmap.size()/8;

    //reverse each byte
    auto iterator = bitmap.begin();
    for (int i = 0; i < numBytes; i++) {
        reverse(iterator, iterator+8);
        iterator += 8;
    }

    //creates the output stream with the size in the first 2 indexes
    uint8_t* bitStream = new uint8_t[numBytes+2];
    bitStream[0] = numBytes;
    bitStream[1] = numBytes >> 8;

    //creates each byte and adds it to the bitStream
    int index = 0;
    uint8_t byte = 0;
    for (int i = 0; i < numBytes; i++) {
        for (int j = 0; j < 8; j++) {
            byte = byte << 1;
            byte = byte + bitmap[index];
            index++;
        }
        bitStream[i+2] = byte;
        byte = 0;
    }

    return bitStream;
}

unsigned MemoryManager::getWordSize() {
    return wordSize;
}

void* MemoryManager::getMemoryStart() {
    return block;
}

unsigned MemoryManager::getMemoryLimit() {
    return blockSize;
}