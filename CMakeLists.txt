cmake_minimum_required(VERSION 3.22)
project(nukes)

set(CMAKE_CXX_STANDARD 20)

set(NUKES_SOURCES include
     include/nukes/details
     include/nukes/memory
     include/nukes
)

include(FetchContent)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG main
    SOURCE_DIR ${GTEST_PATH})
FetchContent_MakeAvailable(googletest)

add_library(${PROJECT_NAME} INTERFACE ${NUKES_SOURCES})
target_include_directories(${PROJECT_NAME} INTERFACE ${NUKES_SOURCES})
target_link_directories(${PROJECT_NAME} INTERFACE ${NUKES_SOURCES})

add_executable(${PROJECT_NAME}_test main.cpp tests.cpp fixture.h)
target_link_libraries(${PROJECT_NAME}_test PRIVATE ${PROJECT_NAME} atomic gtest)
#target_compile_options(${PROJECT_NAME}_test PRIVATE -fsanitize=address)
#target_link_options(${PROJECT_NAME}_test PRIVATE -fsanitize=address)
