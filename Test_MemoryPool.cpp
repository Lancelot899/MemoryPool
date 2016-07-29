#include <string.h>

#include <thread>
#include <memory>
#include <new>

#include <opencv2/opencv.hpp>

#include "MemAllocator.h"


using namespace pi;

struct Data {
    int id;
    cv::Mat pic;
    int reference;
    Data() {printf("Data()\n");}
    Data(int i, int j) {printf("i = %d, j = %d\n", i, j);}
    ~Data() {printf("~Data()\n");}
};

void func1() {
    printf("enter thread 1\n");
    Data *mat = (Data*)Allocator::Instance().getBuffer(2 * sizeof(Data));
    new(mat)Data[2];
    Data *mat1 = (Data*)Allocator::Instance().getBuffer(sizeof(Data));
    new(mat1)Data(12, 32);
    printf("hello 1!\n");
    for(int i = 0; i < 2; ++i) {
        char buffer[512];
        memset(buffer, 0, 512);
        sprintf(buffer, "./data/%d.jpg", i + 1);
        printf("get %s\n", buffer);
        mat[i].pic = cv::imread(buffer);
        printf("pic %d:width = %d, height = %d\n", i + 1, mat[i].pic.cols, mat[i].pic.rows);
    }

    Allocator::Instance().returnBuffer(mat);
    mat1->~Data();
    Allocator::Instance().releaseBuffer(mat1);
}

void func2() {
    printf("enter thread 2\n");
    Data *mat = MemAllocator<Data>::Instance().getBuffer(2);
    printf("hello 2!\n");
    for(int i = 0; i < 2; ++i) {
        char buffer[512];
        memset(buffer, 0, 512);
        sprintf(buffer, "./data/%d.jpg", i + 3);
        printf("get %s\n", buffer);
        mat[i].pic = cv::imread(buffer);

        printf("pic %d:width = %d, height = %d\n", i + 3, mat[i].pic.cols, mat[i].pic.rows);
    }
    Data *mat2 = MemAllocator<Data*>::Instance().getBuffer(2);
    for(int i = 0; i < 2; ++i) {
        char buffer[512];
        memset(buffer, 0, 512);
        sprintf(buffer, "./data/%d.jpg", i + 1);
        printf("get %s\n", buffer);
        mat2[i].pic = cv::imread(buffer);
        printf("pic %d:width = %d, height = %d\n", i + 1, mat[i].pic.cols, mat[i].pic.rows);
    }
    MemAllocator<Data>::Instance().returnBuffer(mat);
    MemAllocator<Data>::Instance().releaseBuffer(mat2, 2);
}


int main() {
    std::thread th1(func1);
    std::thread th2(func2);


    while(true);
    std::cin.get();

    return 0;
}
