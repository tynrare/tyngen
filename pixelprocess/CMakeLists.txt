# File version: 1.2.0 | Updated: 2026-10-05 | Purpose: Build the independent pixelprocess stages and integration checks.
# agent: gpt-6.1-sol | 2026-10-05 | Build stage tools and pipeline check | v 1.2.0 | 1d3b9c
cmake_minimum_required(VERSION 3.16)
project(pixelprocess VERSION 1.2.0 LANGUAGES CXX)
include(CTest)

find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(pixelprocess_common common.cpp)
target_include_directories(pixelprocess_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pixelprocess_common PUBLIC ${OpenCV_LIBS})
target_compile_options(pixelprocess_common PRIVATE -Wall -Wextra -Wpedantic)

foreach(stage background ownership extract lattice reconstruct cleanup assemble)
  add_executable(pixelprocess-${stage} stage_${stage}.cpp)
  target_link_libraries(pixelprocess-${stage} PRIVATE pixelprocess_common)
endforeach()
add_executable(pixelprocess-run runner.cpp)
target_link_libraries(pixelprocess-run PRIVATE pixelprocess_common)

if(BUILD_TESTING)
  add_executable(pixelprocess-pipeline-test tests/pipeline_test.cpp)
  target_link_libraries(pixelprocess-pipeline-test PRIVATE pixelprocess_common)
  add_test(NAME pixelprocess-pipeline COMMAND pixelprocess-pipeline-test $<TARGET_FILE:pixelprocess-run>)
endif()

install(TARGETS pixelprocess-run pixelprocess-background pixelprocess-ownership
        pixelprocess-extract pixelprocess-lattice pixelprocess-reconstruct
        pixelprocess-cleanup pixelprocess-assemble RUNTIME DESTINATION bin)
# agent: gpt-6.1-sol | 2026-10-05 | Build stage tools and pipeline check | v 1.2.0 | 1d3b9c
