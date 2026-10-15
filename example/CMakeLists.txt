cmake_minimum_required(VERSION 3.16)
project(logger_example)

# 示例随模块逐步加入，只构建已经写好的示例文件。
foreach(name zstd_example crypt_example macro_logger_example logger_example)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${name}.cc)
        add_executable(${name} ${name}.cc)
        target_link_libraries(${name} logger)
    endif()
endforeach()
