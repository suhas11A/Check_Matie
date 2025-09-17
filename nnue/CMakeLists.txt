cmake_minimum_required(VERSION 3.15)
project(my_engine)

set(CMAKE_CXX_STANDARD 17)
# Point this to where you unpacked libtorch
set(CMAKE_PREFIX_PATH "/home/suhas/libraries/libtorch")

# disable CMake’s default RPATH behavior so your executable can find libtorch .so files  
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)

find_package(Torch REQUIRED)

add_executable(my_engine uci.cpp)
target_link_libraries(my_engine PRIVATE "${TORCH_LIBRARIES}")
