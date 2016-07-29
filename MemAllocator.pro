QMAKE_CXXFLAGS += -std=c++11

OBJECTS_DIR = ./build


HEADERS += \
    MemAllocator.h

SOURCES += \
    Test_MemoryPool.cpp \
    MemAllocator.cpp

QMAKE_CXXFLAGS += -std=c++11
QMAKE_LFLAGS += -Wl,--no-as-needed

################################################################################
# OpenCV
################################################################################
OPENCV_DIR = /opt/opencv-2.4.9
INCLUDEPATH += $$OPENCV_DIR/include $$OPENCV_DIR/include/opencv
LIBS        +=  -L$$OPENCV_DIR/lib \
                -lopencv_calib3d -lopencv_contrib -lopencv_core \
                -lopencv_features2d -lopencv_flann -lopencv_gpu \
                -lopencv_highgui -lopencv_imgproc -lopencv_legacy \
                -lopencv_ml -lopencv_nonfree -lopencv_objdetect \
                -lopencv_photo -lopencv_stitching -lopencv_ts \
                -lopencv_video -lopencv_videostab
