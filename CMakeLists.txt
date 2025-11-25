cmake_minimum_required(VERSION 3.16)

project(PsyServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 服务端核心依赖：Core (基础), Network (TCP), Sql (MySQL)
find_package(Qt6 REQUIRED COMPONENTS Core Network Sql)

# 自动查找 src 下所有源文件
file(GLOB_RECURSE SERVER_SRCS 
    "src/*.h"
    "src/*.cpp"
)

add_executable(PsyServer ${SERVER_SRCS})

# 链接 Qt 库
target_link_libraries(PsyServer PRIVATE 
    Qt6::Core 
    Qt6::Network 
    Qt6::Sql
)

# 编译后将配置文件复制到输出目录
add_custom_command(TARGET PsyServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/config
    $<TARGET_FILE_DIR:PsyServer>/config
)
