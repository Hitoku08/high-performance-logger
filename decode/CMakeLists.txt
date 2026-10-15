cmake_minimum_required(VERSION 3.20)
project(decoder)

# 解码器在 M12 才开发，源文件不存在时跳过。
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/decode.cc)
    add_executable(decoder decode.cc decode_formatter.cc)
    target_link_libraries(decoder logger)
endif()
