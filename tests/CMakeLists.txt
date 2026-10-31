# Works standalone (cmake -S tests -B build/tests) or via add_subdirectory from the top level.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.22)
    project(PhuAudioLibTests LANGUAGES CXX)
    enable_testing()
endif()

add_executable(test_audio_buffers test_audio_buffers.cpp)
target_include_directories(test_audio_buffers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(test_audio_buffers PRIVATE cxx_std_20)

add_test(NAME audio_buffer_tests COMMAND test_audio_buffers)
