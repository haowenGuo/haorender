cmake_minimum_required(VERSION 3.10)
project(myrender)
find_package(OpenCV REQUIRED)
set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(myrender main.cpp tgaimage.cpp Drawer.cpp shader.cpp ppp.cpp render.cpp model.cpp)
target_link_libraries(myrender ${OpenCV_LIBRARIES})
