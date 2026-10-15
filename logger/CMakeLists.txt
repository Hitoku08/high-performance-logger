include_guard(GLOBAL)

include_directories(. third_party/cryptopp_cmake third_party/protobuf third_party/fmt/include)

add_subdirectory(third_party/cryptopp_cmake)
add_subdirectory(third_party/protobuf)
# 只需要静态库；zstd 的动态库资源文件在 MSVC 下找不到 zstd.h
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/zstd/build/cmake zstd)
add_subdirectory(third_party/zlib)

# 每完成一个模块，把该模块的源文件取消注释（模块编号见 AGENTS.md 的开发路线）。
# 只有头文件的模块（M1 等）不需要在这里登记。
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(UTIL_SRCS
        # utils/sys_util_linux.cc utils/sys_util.cc utils/file_util.cc   # M2
    )
    set(MMAP_SRCS
        # mmap/mmap_aux.cc mmap/mmap_linux.cc                            # M7
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(UTIL_SRCS
        # utils/sys_util_win.cc utils/sys_util.cc utils/file_util.cc     # M2
    )
    set(MMAP_SRCS
        # mmap/mmap_aux.cc mmap/mmap_win.cc                              # M7
    )
else()
    message(FATAL_ERROR "unsupported.")
endif()

set(CORE_SRCS
    # log_msg.cc log_handle.cc                                         # M3
    # log_factory.cc                                                   # M5
)
set(FORMATTER_SRCS
    # formatter/default_formatter.cc                                   # M4
    # formatter/effective_formatter.cc                                 # M10
)
set(SINK_SRCS
    # sinks/console_sink.cc                                            # M4
    # sinks/effective_sink.cc                                          # M11
)
set(CONTEXT_SRCS
    # context/thread_pool.cc context/executor.cc context/context.cc    # M6
)
set(COMPRESS_SRCS
    # compress/zstd_compress.cc compress/zlib_compress.cc              # M8
)
set(CRYPT_SRCS
    # crypt/crypt.cc crypt/aes_crypt.cc                                # M9
)
set(PROTO_SRCS
    # ${CMAKE_BINARY_DIR}/effective_msg.pb.cc                          # M10
)

set(SRCS
    ${CORE_SRCS}
    ${PROTO_SRCS}
    ${MMAP_SRCS}
    ${UTIL_SRCS}
    ${FORMATTER_SRCS}
    ${SINK_SRCS}
    ${COMPRESS_SRCS}
    ${CRYPT_SRCS}
    ${CONTEXT_SRCS}
)

message(STATUS "libs ${LIBS}")
message(STATUS "logger srcs ${SRCS}")

# 还没有任何 .cc 时，logger 先作为纯头文件库存在，保证骨架阶段也能构建和测试。
if(SRCS)
    add_library(logger STATIC ${SRCS})
    target_link_libraries(logger PUBLIC ${LIBS} cryptopp protobuf libzstd_static zlib)
else()
    add_library(logger INTERFACE)
    target_link_libraries(logger INTERFACE ${LIBS})
endif()
