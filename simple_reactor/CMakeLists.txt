cmake_minimum_required(VERSION 3.10)
project(SimpleReactor VERSION 1.0.0 LANGUAGES CXX)

# C++标准设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(MSVC)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 源文件列表
set(SOURCES
    src/Buffer.cpp
    src/ThreadPool.cpp
    src/Epoll.cpp
    src/Channel.cpp
    src/EventLoop.cpp
    src/Acceptor.cpp
    src/TcpConnection.cpp
    src/EventLoopThread.cpp
    src/EventLoopThreadPool.cpp
    src/TcpServer.cpp
)

# 创建静态库
add_library(simple_reactor_static STATIC ${SOURCES})
target_link_libraries(simple_reactor_static PUBLIC pthread)
set_target_properties(simple_reactor_static PROPERTIES OUTPUT_NAME simple_reactor)

# 创建动态库
add_library(simple_reactor_shared SHARED ${SOURCES})
target_link_libraries(simple_reactor_shared PUBLIC pthread)
set_target_properties(simple_reactor_shared PROPERTIES OUTPUT_NAME simple_reactor)

# 主从Reactor服务器示例
add_executable(main_reactor_server examples/main_reactor_server.cpp)
target_link_libraries(main_reactor_server PRIVATE simple_reactor_static)

# 回显服务器示例（单线程版本，用于对比）
add_executable(echo_server examples/echo_server.cpp)
target_link_libraries(echo_server PRIVATE simple_reactor_static)

# 压力测试客户端
add_executable(benchmark_client examples/benchmark_client.cpp)
target_link_libraries(benchmark_client PRIVATE pthread)

# 安装规则
include(GNUInstallDirs)

install(TARGETS simple_reactor_static simple_reactor_shared
    EXPORT SimpleReactorTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ 
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/simple_reactor
)

# 打印配置信息
message(STATUS "")
message(STATUS "========== Simple Reactor Configuration ==========")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "==================================================")
message(STATUS "")
