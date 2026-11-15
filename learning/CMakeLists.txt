# cmake_minimum_required(VERSION 3.16)
#
# project(tet LANGUAGES CXX)
#
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED 17)
#
# find_package(OpenCV REQUIRED)
#
# add_executable(main 
#     main.cpp
# )
#
#
# target_link_libraries(main PRIVATE ${OpenCV_LIBS})
# target_include_directories(main PRIVATE 
#     ${OpenCV_INCLUDE_DIRS}
#     ${CMAKE_CURRENT_SOURCE_DIR}/../io/hikrobot/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../io
#     ${CMAKE_CURRENT_SOURCE_DIR}/..
# )
