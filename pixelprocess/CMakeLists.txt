# File version: 1.7.0 | Updated: 2026-10-06 | Purpose: Build the independent pixelprocess stages and integration checks.
# agent: gpt-6.1-sol | 2026-10-06 | Build cleanup preview executable | v 1.6.0 | f3ca03
# agent: gpt-6 | 2026-10-05 | Build focused fringe cleanup checks | v 1.5.0 | bfd08a
cmake_minimum_required(VERSION 3.16)
project(pixelprocess VERSION 1.6.0 LANGUAGES CXX)
include(CTest)

find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(pixelprocess_common common.cpp)
target_include_directories(pixelprocess_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pixelprocess_common PUBLIC ${OpenCV_LIBS})
target_compile_options(pixelprocess_common PRIVATE -Wall -Wextra -Wpedantic)

# agent: gpt-6.1-sol | 2026-10-06 | Build cleanup preview executable | v 1.6.0 | f3ca03
# agent: gpt-6 | 2026-10-06 | Build diagnosis stage | v 1.7.0 | 000000
foreach(stage stage_background stage_ownership stage_diagnosis stage_extract stage_lattice stage_reconstruct stage_cleanup_preview stage_cleanup stage_assemble)
  add_executable(${stage} ${stage}.cpp)
  target_link_libraries(${stage} PRIVATE pixelprocess_common)
endforeach()
# agent: gpt-6.1-sol | 2026-10-06 | Build cleanup preview executable | v 1.6.0 | f3ca03
add_executable(runner runner.cpp)
target_link_libraries(runner PRIVATE pixelprocess_common)

if(BUILD_TESTING)
  add_executable(fringe_test tests/fringe_test.cpp)
  target_link_libraries(fringe_test PRIVATE pixelprocess_common)
  set_target_properties(fringe_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  add_test(NAME pixelprocess-fringe COMMAND fringe_test)
  add_executable(pipeline_test tests/pipeline_test.cpp)
  target_link_libraries(pipeline_test PRIVATE pixelprocess_common)
  set_target_properties(pipeline_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  add_test(NAME pixelprocess-pipeline COMMAND pipeline_test $<TARGET_FILE:runner>)
endif()

# agent: gpt-6.1-sol | 2026-10-06 | Build cleanup preview executable | v 1.6.0 | f3ca03
install(TARGETS runner stage_background stage_ownership stage_diagnosis stage_extract stage_lattice
        stage_reconstruct stage_cleanup_preview stage_cleanup stage_assemble RUNTIME DESTINATION bin)
# agent: gpt-6.1-sol | 2026-10-06 | Build cleanup preview executable | v 1.6.0 | f3ca03
# agent: gpt-6 | 2026-10-05 | Build focused fringe cleanup checks | v 1.5.0 | bfd08a
