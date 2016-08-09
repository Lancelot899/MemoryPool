#MemAllocator

when you use this lib, you should use the c++11 standrand.
Please read Test_MemoryPool.cpp, you may find all demo for using it

#### note: 

 - A memory allocator has two layers allocators. basic allocator is class Alloc, which is almost like SGI allocator(https://www.sgi.com/tech/stl/alloc.html). The superstratum has maps used to manage the memory allocated by Alloc.
 * Each different type has only one object, but all the objects use the same basic allocator
