cmake_minimum_required(VERSION 3.22)
project(AtomicStack)

set(CMAKE_CXX_STANDARD 20)

file(GLOB SOURCES
     ${PROJECT_SOURCE_DIR}/*.h
     ${PROJECT_SOURCE_DIR}/*.cpp
)
message("Sources : ${SOURCES}")
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE atomic)
