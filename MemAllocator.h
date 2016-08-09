/**
 * @file  MemAllocator.h
 * @brief this file define a memory allocator and a memory manager
 *         when you use it, please use c++11 standard.
 *         A memory allocator has two layers allocators. basic allocator is
 *         class Alloc, which is almost like SGI allocator(https://www.sgi.com/tech/stl/alloc.html).
 *         the superstratum has maps used to manage the memory allocated by Alloc
 *         Each different type has only one object, but all the objects use the 
 *         same basic allocator
 *
 * @author lancelot
 * @Email  3128243880@qq.com
 * @date   20160727
 * @version 1.2
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


#include "MemAllocator.inl"
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

__GEN_MEMALLOC_(int)
__GEN_MEMALLOC_(float)
__GEN_MEMALLOC_(char)
__GEN_MEMALLOC_(double)
__GEN_MEMALLOC_(unsigned int)
__GEN_MEMALLOC_(unsigned char)
__GEN_MEMALLOC_(short)
__GEN_MEMALLOC_(unsigned short)
__GEN_MEMALLOC_(long)
__GEN_MEMALLOC_(unsigned long)

__GEN_PT_MEMALLOC_(int)
__GEN_PT_MEMALLOC_(float)
__GEN_PT_MEMALLOC_(char)
__GEN_PT_MEMALLOC_(double)
__GEN_PT_MEMALLOC_(unsigned int)
__GEN_PT_MEMALLOC_(unsigned char)
__GEN_PT_MEMALLOC_(short)
__GEN_PT_MEMALLOC_(unsigned short)
__GEN_PT_MEMALLOC_(long)
__GEN_PT_MEMALLOC_(unsigned long)



typedef MemAllocator<void*> Allocator;

} // end of namespace pi

#endif // MEMALLOCATOR_H


