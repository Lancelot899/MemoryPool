/**
 * @file  MemAllocator.h
 * @brief this file define a memory allocator and a memory manager
 * @author lancelot
 * @Email  3128243880@qq.com
 * @date   20160727
 */

#ifndef MEMALLOCATOR_H
#define MEMALLOCATOR_H

#include <mutex>
#include <thread>

#include <unordered_map>
#include <deque>
#include <unistd.h>
#include <iostream>
#include <new>

namespace pi {


/////////////////////////////////////////////////////////////
/// \brief when you need some memory, please use this class
/////////////////////////////////////////////////////////////
class Alloc {
public:
    /**
     * @brief allocate some memory
     * @param  memory size you need
     * @return pointer to the memory
     */
    static void* allocate(size_t n);

    /**
     * @brief deallocate memory pointed by pointer
     * @param pointer to the memory
     * @param the size of this memory
     */
    static void  deallocate(void* p, size_t n);

    /**
     * @brief reallocate some memory to your pointer
     * @param pointer
     * @param old size
     * @param new size
     * @return the pointer to the new memory
     */
    static void* reallocate(void*p, size_t old_sz, size_t new_sz);

    /**
     * @brief set a function to deal with the condition that the physical memory is not adequate
     * @param the function pointer whose format is void(*)()
     * @return the old function pointer which was used to deal with above-mentioned condition
     */
    static void (*set_oom_malloc_handler(void (*f)())) ();


    static int setDefaultNodeNum(int nn);
    static int setInitPoolSize(int ps);
};



template <typename T>
struct trait {
    typedef T  typeName;
    typedef T* pointer;
    typedef T& refference;
};



/////////////////////////////////////////////////////////////////
/// \brief when you need a manager to manage your memory,
///        in order to improve some performance of your code,
///        please use this class.
///        there exist memory list for users to forbid allocate
///        and deallocate memory frequently
/////////////////////////////////////////////////////////////////
template <typename T>
class MemAllocator
{
public:
    /**
     * @brief you are allow to use this function to get a object to manage your memory
     * @return the memory manager
     */
    static MemAllocator<T>& Instance() {
        static MemAllocator<T> theOneAndOnly;
        return theOneAndOnly;
    }

    /**
     * @brief you can get object array you want
     * @param the number of object you should get
     * @return the object array
     *
     * @example in this example you can get 12 DataType objects
     *      DataType data* = MemAllocator<DataType>::getBuffer(12);
     *
     * @note the object is constructed by default construct function
     */
    T* getBuffer(size_t num) {
        std::unique_lock<std::mutex> lock(accessMutex);

        if(availableBuffers.find(num) != availableBuffers.end()) {
            std::deque<T*>& availableBuffer = availableBuffers.at(num);
            if(availableBuffer.empty()) {
                void *buf = Alloc::allocate(num * sizeof(T));
                T* buffer = (T*)buf;
                new(buffer)T[num];

                bufferSizes.insert(std::make_pair(buffer, num));

                return buffer;
            }

            else {
                T *buffer = availableBuffer.back();
                availableBuffer.pop_back();

                return buffer;
            }
        }

        else {
            void* buf = Alloc::allocate(num * sizeof(T));
            T* buffer = (T*)buf;
            new(buffer)T[num];

            bufferSizes.insert(std::make_pair(buffer, num));
            return buffer;
        }

    }

    /**
     * @brief release Buffer
     * @param the pointer to the object buffer you want to release
     * @param the number of objects you want to release
     *
     * @example in this example, 12 objects get by getBuffer(12) will be release
     *      MemAllocator<DataType>::releaseBuffer(data, 12)
     *
     * @note the function will call destruction function
     */
    void releaseBuffer(T* buffer, size_t num = 1) {
        if(buffer == 0)
            return ;
        if(num == 1)
            buffer->~T();
        else {
            for(int i = 0; i < num; ++i) {
                buffer[i].~T();
            }
        }
        if(bufferSizes.find(buffer) != bufferSizes.end()) {
            if(accessMutex.try_lock()) {
                bufferSizes.erase(buffer);
                accessMutex.unlock();
            }
            else
                bufferSizes.erase(buffer);
        }
        Alloc::deallocate((void*)buffer, sizeof(T) * num);
    }

    /**
     * @brief release all the buffers in memory list(it will call destruction)
     */
    void releaseBuffers() {
        std::unique_lock<std::mutex>  lock(accessMutex);
        for(auto p = availableBuffers.begin(); p != availableBuffers.end(); ++p) {
            for(size_t i = 0; i < p->second.size(); ++i) {
                releaseBuffer(p->second[i], p->first);
            }
            p->second.clear();
        }

        availableBuffers.clear();
    }

    /**
     * @brief return the buffer getted by getBuffer to memory list
     * @param buffer
     * @note  this function will not call destruction function, if you
     *        want to reset your object, please realize it by yourself
     */
    void returnBuffer(T* buffer) {
        if(buffer == 0)
            return;
        std::unique_lock<std::mutex> lock(accessMutex);
        if(bufferSizes.find(buffer) == bufferSizes.end()) {
            printf("this buffer is not in our list!\n");
            return;
        }
        size_t size = bufferSizes.at(buffer);
        if(availableBuffers.find(size) != availableBuffers.end()) {
            availableBuffers.at(size).push_back(buffer);
        }
        else {
            std::deque<T*> availableOfSize;
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
    std::unordered_map<T*, size_t>               bufferSizes;
    std::unordered_map<size_t, std::deque<T*> >  availableBuffers;
    std::mutex                                   accessMutex;
};


template <typename T>
class MemAllocator<T*>
{
public:
    static MemAllocator< typename trait<T>::typeName >& Instance() {
        return MemAllocator< typename trait<T>::typeName >::Instance();
    }

private:
    MemAllocator() {}
};

template<>
class MemAllocator<int> {
public:
    static MemAllocator<int>& Instance() {
        static MemAllocator<int> theOneAndOnly;
        return theOneAndOnly;
    }

    int* getBuffer(size_t num) {
        std::unique_lock<std::mutex> lock(accessMutex);

        if(availableBuffers.find(num) != availableBuffers.end()) {
            std::deque<int*>& availableBuffer = availableBuffers.at(num);
            if(availableBuffer.empty()) {
                void *buf = Alloc::allocate(num * sizeof(int));
                int* buffer = (int*)buf;

                bufferSizes.insert(std::make_pair(buffer, num));

                return buffer;
            }

            else {
                int *buffer = availableBuffer.back();
                availableBuffer.pop_back();

                return buffer;
            }
        }

        else {
            void* buf = Alloc::allocate(num * sizeof(int));
            int* buffer = (int*)buf;
            bufferSizes.insert(std::make_pair(buffer, num));
            return buffer;
        }

    }

    void releaseBuffer(int* buffer, size_t num = 1) {
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
        Alloc::deallocate((void*)buffer, sizeof(int) * num);
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

    void returnBuffer(int* buffer) {
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
            std::deque<int*> availableOfSize;
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
    std::unordered_map<int*, size_t>               bufferSizes;
    std::unordered_map<size_t, std::deque<int*> >  availableBuffers;
    std::mutex                                     accessMutex;
};

template <>
class MemAllocator<int*>
{
public:
    static MemAllocator< typename trait<int>::typeName >& Instance() {
        return MemAllocator< typename trait<int>::typeName >::Instance();
    }

private:
    MemAllocator() {}
};


template<>
class MemAllocator<float> {
public:
    static MemAllocator<float>& Instance() {
        static MemAllocator<float> theOneAndOnly;
        return theOneAndOnly;
    }

    float* getBuffer(size_t num) {
        std::unique_lock<std::mutex> lock(accessMutex);

        if(availableBuffers.find(num) != availableBuffers.end()) {
            std::deque<float*>& availableBuffer = availableBuffers.at(num);
            if(availableBuffer.empty()) {
                void *buf = Alloc::allocate(num * sizeof(float));
                float* buffer = (float*)buf;

                bufferSizes.insert(std::make_pair(buffer, num));

                return buffer;
            }

            else {
                float *buffer = availableBuffer.back();
                availableBuffer.pop_back();

                return buffer;
            }
        }

        else {
            void* buf = Alloc::allocate(num * sizeof(float));
            float* buffer = (float*)buf;
            bufferSizes.insert(std::make_pair(buffer, num));
            return buffer;
        }

    }

    void releaseBuffer(float* buffer, size_t num = 1) {
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
        Alloc::deallocate((void*)buffer, sizeof(float) * num);
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

    void returnBuffer(float* buffer) {
        if(buffer == 0)
            return;
        std::unique_lock<std::mutex> lock(accessMutex);
        if(bufferSizes.find(buffer) == bufferSizes.end()) {
            printf("this buffer is not in our list!\n");
            return ;
        }
        size_t size = bufferSizes.at(buffer);
        if(availableBuffers.find(size) != availableBuffers.end())
            availableBuffers.at(size).push_back(buffer);
        else {
            std::deque<float*> availableOfSize;
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
    std::unordered_map<float*, size_t>               bufferSizes;
    std::unordered_map<size_t, std::deque<float*> >  availableBuffers;
    std::mutex                                     accessMutex;
};

template <>
class MemAllocator<float*>
{
public:
    static MemAllocator< typename trait<float>::typeName >& Instance() {
        return MemAllocator< typename trait<float>::typeName >::Instance();
    }

private:
    MemAllocator() {}
};


template<>
class MemAllocator<double> {
public:
    static MemAllocator<double>& Instance() {
        static MemAllocator<double> theOneAndOnly;
        return theOneAndOnly;
    }

    double* getBuffer(size_t num) {
        std::unique_lock<std::mutex> lock(accessMutex);

        if(availableBuffers.find(num) != availableBuffers.end()) {
            std::deque<double*>& availableBuffer = availableBuffers.at(num);
            if(availableBuffer.empty()) {
                void *buf = Alloc::allocate(num * sizeof(double));
                double* buffer = (double*)buf;

                bufferSizes.insert(std::make_pair(buffer, num));

                return buffer;
            }

            else {
                double *buffer = availableBuffer.back();
                availableBuffer.pop_back();

                return buffer;
            }
        }

        else {
            void* buf = Alloc::allocate(num * sizeof(double));
            double* buffer = (double*)buf;
            bufferSizes.insert(std::make_pair(buffer, num));
            return buffer;
        }

    }

    void releaseBuffer(double* buffer, size_t num = 1) {
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
        Alloc::deallocate((void*)buffer, sizeof(double) * num);
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

    void returnBuffer(double* buffer) {
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
            std::deque<double*> availableOfSize;
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
    std::unordered_map<double*, size_t>               bufferSizes;
    std::unordered_map<size_t, std::deque<double*> >  availableBuffers;
    std::mutex                                        accessMutex;
};

template <>
class MemAllocator<double*>
{
public:
    static MemAllocator< typename trait<double>::typeName >& Instance() {
        return MemAllocator< typename trait<double>::typeName >::Instance();
    }

private:
    MemAllocator() {}
};



template<>
class MemAllocator<char> {
public:
    static MemAllocator<char>& Instance() {
        static MemAllocator<char> theOneAndOnly;
        return theOneAndOnly;
    }

    char* getBuffer(size_t num) {
        std::unique_lock<std::mutex> lock(accessMutex);

        if(availableBuffers.find(num) != availableBuffers.end()) {
            std::deque<char*>& availableBuffer = availableBuffers.at(num);
            if(availableBuffer.empty()) {
                void *buf = Alloc::allocate(num * sizeof(char));
                char* buffer = (char*)buf;

                bufferSizes.insert(std::make_pair(buffer, num));

                return buffer;
            }

            else {
                char *buffer = availableBuffer.back();
                availableBuffer.pop_back();

                return buffer;
            }
        }

        else {
            void* buf = Alloc::allocate(num * sizeof(char));
            char* buffer = (char*)buf;
            bufferSizes.insert(std::make_pair(buffer, num));
            return buffer;
        }

    }

    void releaseBuffer(char* buffer, size_t num = 1) {
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
        Alloc::deallocate((void*)buffer, sizeof(char) * num);
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

    void returnBuffer(char* buffer) {
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
            std::deque<char*> availableOfSize;
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
    std::unordered_map<char*, size_t>               bufferSizes;
    std::unordered_map<size_t, std::deque<char*> >  availableBuffers;
    std::mutex                                      accessMutex;
};

template <>
class MemAllocator<char*>
{
public:
    static MemAllocator< typename trait<char>::typeName >& Instance() {
        return MemAllocator< typename trait<char>::typeName >::Instance();
    }

private:
    MemAllocator() {}
};


template<>
class MemAllocator<void> {
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
                void *buf = Alloc::allocate(bytes);
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
            void* buf = Alloc::allocate(bytes);
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
        Alloc::deallocate((void*)buffer, byte);
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

template <>
class MemAllocator<void*>
{
public:
    static MemAllocator<void>& Instance() {
        return MemAllocator<void>::Instance();
    }

private:
    MemAllocator() {}
};

typedef MemAllocator<void*> Allocator;

} // end of namespace pi

#endif // MEMALLOCATOR_H


