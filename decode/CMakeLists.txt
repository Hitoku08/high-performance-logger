cmake_minimum_required(VERSION 3.20)
project(decoder)

# 解码器在 M12 才开发，源文件不存在时跳过。
# 解码逻辑编译成静态库 decoder_core，decoder 程序和 tests/decoder_test.cc 都链接它。
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/decode.cc)
    add_library(decoder_core STATIC decoder.cc decode_formatter.cc)
    target_link_libraries(decoder_core PUBLIC logger)

    add_executable(decoder decode.cc)
    target_link_libraries(decoder PRIVATE decoder_core)
endif()
