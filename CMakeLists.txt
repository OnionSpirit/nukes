cmake_minimum_required(VERSION 3.22)
project(nukes)

set(CMAKE_CXX_STANDARD 20)

set(NUKES_SOURCES include
     include/nukes/bounded
     include/nukes/unbounded
     include/nukes/buffer
     include/nukes/details
)

add_library(${PROJECT_NAME} INTERFACE ${NUKES_SOURCES})
target_include_directories(${PROJECT_NAME} INTERFACE ${NUKES_SOURCES})
target_link_directories(${PROJECT_NAME} INTERFACE ${NUKES_SOURCES})

add_executable(${PROJECT_NAME}_test main.cpp atomic_stack.h atomic_stack_base.h)
target_link_libraries(${PROJECT_NAME}_test PRIVATE ${PROJECT_NAME} atomic)
