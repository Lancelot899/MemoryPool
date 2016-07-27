QMAKE_CXXFLAGS += -std=c++11

OBJECTS_DIR = ./build


HEADERS += \
    MemAllocator.h

SOURCES += \
    text.cpp \
    MemAllocator.cpp

QMAKE_CXXFLAGS += -std=c++11
QMAKE_LFLAGS += -Wl,--no-as-needed
INCLUDEPATH +=/opt/opencv-2.4.9/include

LIBS    += -lpthread
LIBS += -L/opt/opencv-2.4.9/lib -lopencv_calib3d \
 -lopencv_contrib -lopencv_core -lopencv_features2d -lopencv_flann \
 -lopencv_highgui -lopencv_imgproc -lopencv_legacy \
 -lopencv_nonfree -lopencv_objdetect \
 -lopencv_photo  -lopencv_ts -lopencv_video -lopencv_videostab


