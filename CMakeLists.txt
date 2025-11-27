cmake_minimum_required(VERSION 3.28)

project(PsyServer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Sql Concurrent)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appPsyServer
	src/main.cpp
        src/core/ProtocolDefs.h
        src/core/ConfigManager.h src/core/ConfigManager.cpp
        src/core/ServerApp.h src/core/ServerApp.cpp
        # src/dao/AppointmentDao.h src/dao/AppointmentDao.cpp
        src/dao/DBManager.h src/dao/DBManager.cpp
        # src/dao/QuestionnaireDao.h src/dao/QuestionnaireDao.cpp
        # src/dao/ScheduleDao.h src/dao/ScheduleDao.cpp
        # src/dao/StatisticsDao.h src/dao/StatisticsDao.cpp
        src/dao/UserDao.h src/dao/UserDao.cpp
        src/logic/AdminHandler.h src/logic/AdminHandler.cpp
        src/logic/AuthHandler.h src/logic/AuthHandler.cpp
        src/logic/BookingHandler.h src/logic/BookingHandler.cpp
        # src/logic/DoctorHandler.h src/logic/DoctorHandler.cpp
        src/logic/RequestRouter.h src/logic/RequestRouter.cpp
        # src/logic/SurveyHandler.h src/logic/SurveyHandler.cpp
        src/network/ClientSocket.h src/network/ClientSocket.cpp
        src/network/TcpListener.h src/network/TcpListener.cpp
)

target_compile_definitions(appPsyServer PRIVATE 
    PROJECT_ROOT_PATH="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_include_directories(appPsyServer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(appPsyServer PRIVATE 
    Qt6::Core 
    Qt6::Network 
    Qt6::Sql
    Qt6::Concurrent
)

# 编译后将配置文件复制到输出目录
add_custom_command(TARGET appPsyServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/config
    $<TARGET_FILE_DIR:appPsyServer>/config
)
