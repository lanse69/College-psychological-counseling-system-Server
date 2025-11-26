cmake_minimum_required(VERSION 3.16)

project(PsyServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 依赖
find_package(Qt6 REQUIRED COMPONENTS Core Network Sql)

# 查找 src 下所有源文件
file(GLOB_RECURSE SERVER_SRCS 
    "src/*.h"
    "src/*.cpp"
)

add_executable(PsyServer ${SERVER_SRCS})

target_compile_definitions(PsyServer PRIVATE 
    PROJECT_ROOT_PATH="${CMAKE_CURRENT_SOURCE_DIR}"
)

# 链接 Qt 库
target_link_libraries(PsyServer PRIVATE 
    Qt6::Core 
    Qt6::Network 
    Qt6::Sql
)

target_include_directories(PsyServer PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# 编译后将配置文件复制到输出目录
add_custom_command(TARGET PsyServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/config
    $<TARGET_FILE_DIR:PsyServer>/config
)
