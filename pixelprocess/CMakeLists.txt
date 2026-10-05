# File version: 1.4.0 | Updated: 2026-10-05 | Purpose: Build the independent pixelprocess stages and integration checks.
# agent: gpt-6.1-sol | 2026-10-05 | Place pipeline test beside its source | v 1.4.0 | bc9f40
cmake_minimum_required(VERSION 3.16)
project(pixelprocess VERSION 1.4.0 LANGUAGES CXX)
include(CTest)

find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(pixelprocess_common common.cpp)
target_include_directories(pixelprocess_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pixelprocess_common PUBLIC ${OpenCV_LIBS})
target_compile_options(pixelprocess_common PRIVATE -Wall -Wextra -Wpedantic)

foreach(stage stage_background stage_ownership stage_extract stage_lattice stage_reconstruct stage_cleanup stage_assemble)
  add_executable(${stage} ${stage}.cpp)
  target_link_libraries(${stage} PRIVATE pixelprocess_common)
endforeach()
add_executable(runner runner.cpp)
target_link_libraries(runner PRIVATE pixelprocess_common)

if(BUILD_TESTING)
  add_executable(pipeline_test tests/pipeline_test.cpp)
  target_link_libraries(pipeline_test PRIVATE pixelprocess_common)
  set_target_properties(pipeline_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  add_test(NAME pixelprocess-pipeline COMMAND pipeline_test $<TARGET_FILE:runner>)
endif()

install(TARGETS runner stage_background stage_ownership stage_extract stage_lattice
        stage_reconstruct stage_cleanup stage_assemble RUNTIME DESTINATION bin)
# agent: gpt-6.1-sol | 2026-10-05 | Place pipeline test beside its source | v 1.4.0 | bc9f40
