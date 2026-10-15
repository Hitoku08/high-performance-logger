if(MSVC)
    # googletest 自带 /utf-8，与根目录的 /source-charset:utf-8 冲突，测试目录统一改用 /utf-8
    set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "")
    add_compile_options(/utf-8)
endif()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/logger/third_party/googletest googletest)

include(GoogleTest)

# 每个 *_test.cc 编译成一个独立的测试程序，新增测试文件后重新运行构建脚本即可被收录。
file(GLOB TEST_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cc)

foreach(src ${TEST_SRCS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE logger GTest::gtest_main)
    gtest_discover_tests(${name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DISCOVERY_TIMEOUT 60
    )
endforeach()
