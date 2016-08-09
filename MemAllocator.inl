
template <typename T, typename _Allocator>
class MemAllocator<T*, _Allocator>
{
public:
    static MemAllocator< typename trait<T>::typeName >& Instance() {
        return MemAllocator< typename trait<T>::typeName >::Instance();
    }

private:
    MemAllocator() {}
};


template<typename _Allocator>
class MemAllocator<void, _Allocator> {
public:
    static MemAllocator<void>& Instance() {
        static MemAllocator<void> theOneAndOnly;
        return theOneAndOnly;
    }

    void* getBuffer(size_t bytes) {
        std::unique_lock<std::mutex> lock(accessMutex);

        if(availableBuffers.find(bytes) != availableBuffers.end()) {
            std::deque<void*>& availableBuffer = availableBuffers.at(bytes);
            if(availableBuffer.empty()) {
                void *buf = _Allocator::allocate(bytes);
                bufferSizes.insert(std::make_pair(buf, bytes));

                return buf;
            }

            else {
                void *buffer = availableBuffer.back();
                availableBuffer.pop_back();

                return buffer;
            }
        }

        else {
            void* buf = _Allocator::allocate(bytes);
            bufferSizes.insert(std::make_pair(buf, bytes));
            return buf;
        }

    }

    void releaseBuffer(void* buffer, size_t byte = 1) {
        if(buffer == 0)
            return ;
        if(bufferSizes.find(buffer) != bufferSizes.end()) {
            if(accessMutex.try_lock()) {
                bufferSizes.erase(buffer);
                accessMutex.unlock();
            }
            else
                bufferSizes.erase(buffer);
        }
        _Allocator::deallocate((void*)buffer, byte);
    }

    void releaseBuffers() {
        std::unique_lock<std::mutex>  lock(accessMutex);
        for(auto p : availableBuffers) {
            for(size_t i = 0; i < p.second.size(); ++i) {
                releaseBuffer(p.second[i], p.first);
            }
            p.second.clear();
        }

        availableBuffers.clear();
    }

    void returnBuffer(void* buffer) {
        if(buffer == 0)
            return;
        std::unique_lock<std::mutex> lock(accessMutex);
        if(bufferSizes.find(buffer) == bufferSizes.end()) {
            printf("this buffer is not in our list!\n");
            return;
        }
        size_t size = bufferSizes.at(buffer);
        if(availableBuffers.find(size) != availableBuffers.end())
            availableBuffers.at(size).push_back(buffer);
        else {
            std::deque<void*> availableOfSize;
            availableOfSize.push_back(buffer);
            availableBuffers.insert(std::make_pair(size, availableOfSize));
        }
    }

private:
    MemAllocator() {}

private:
    /////////////////////////////////////////////////////
    /// \brief memory pool
    ////////////////////////////////////////////////////
    std::unordered_map<void*, size_t>               bufferSizes;
    std::unordered_map<size_t, std::deque<void*> >  availableBuffers;
    std::mutex                                      accessMutex;
};

template <typename _Allocator>
class MemAllocator<void*, _Allocator>
{
public:
    static MemAllocator<void>& Instance() {
        return MemAllocator<void>::Instance();
    }

private:
    MemAllocator() {}
};


#define __GEN_MEMALLOC_(TYPE) \
    template<typename _Allocator> \
    class MemAllocator<TYPE, _Allocator> { \
    public: \
        static MemAllocator<TYPE>& Instance() { \
            static MemAllocator<TYPE> theOneAndOnly; \
            return theOneAndOnly; \
        } \
        TYPE* getBuffer(size_t num) { \
            std::unique_lock<std::mutex> lock(accessMutex); \
            if(availableBuffers.find(num) != availableBuffers.end()) { \
                std::deque<TYPE*>& availableBuffer = availableBuffers.at(num); \
                if(availableBuffer.empty()) { \
                    void *buf = _Allocator::allocate(num * sizeof(TYPE)); \
                    TYPE* buffer = (TYPE*)buf; \
                    bufferSizes.insert(std::make_pair(buffer, num)); \
                    return buffer; \
                } \
                else { \
                    TYPE *buffer = availableBuffer.back(); \
                    availableBuffer.pop_back(); \
                    return buffer; \
                } \
            } \
            else { \
                void* buf = _Allocator::allocate(num * sizeof(TYPE)); \
                TYPE* buffer = (TYPE*)buf; \
                bufferSizes.insert(std::make_pair(buffer, num)); \
                return buffer; \
            } \
        } \
        void releaseBuffer(TYPE* buffer, size_t num = 1) { \
            if(buffer == 0) \
                return ; \
            if(bufferSizes.find(buffer) != bufferSizes.end()) { \
                if(accessMutex.try_lock()) { \
                    bufferSizes.erase(buffer);\
                    accessMutex.unlock(); \
                } \
                else \
                    bufferSizes.erase(buffer); \
            } \
            _Allocator::deallocate((void*)buffer, sizeof(TYPE) * num); \
        } \
        void releaseBuffers() { \
            std::unique_lock<std::mutex>  lock(accessMutex); \
            for(auto p : availableBuffers) { \
                for(size_t i = 0; i < p.second.size(); ++i) { \
                    releaseBuffer(p.second[i], p.first); \
                } \
                p.second.clear(); \
            } \
            availableBuffers.clear(); \
        } \
        void returnBuffer(TYPE* buffer) { \
            if(buffer == 0) \
                return; \
            std::unique_lock<std::mutex> lock(accessMutex); \
            if(bufferSizes.find(buffer) == bufferSizes.end()) { \
                printf("this buffer is not in our list!\n"); \
                return; \
            } \
            size_t size = bufferSizes.at(buffer); \
            if(availableBuffers.find(size) != availableBuffers.end()) \
                availableBuffers.at(size).push_back(buffer); \
            else { \
                std::deque<TYPE*> availableOfSize; \
                availableOfSize.push_back(buffer); \
                availableBuffers.insert(std::make_pair(size, availableOfSize)); \
            } \
        } \
    private: \
        MemAllocator() {} \
    private: \
        std::unordered_map<TYPE*, size_t>               bufferSizes; \
        std::unordered_map<size_t, std::deque<TYPE*> >  availableBuffers; \
        std::mutex                                     accessMutex; \
    };

#define __GEN_PT_MEMALLOC_(TYPE) \
template <typename _Allocator> \
class MemAllocator<TYPE*, _Allocator> { \
public: \
    static MemAllocator< typename trait<TYPE>::typeName >& Instance() { \
        return MemAllocator< typename trait<TYPE>::typeName >::Instance(); \
    } \
private: \
    MemAllocator() {} \
};
