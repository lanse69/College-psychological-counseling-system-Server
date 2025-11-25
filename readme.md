# 🖥️ PsyServer - 心理咨询系统服务端

## 📖 简介
**PsyServer** 是高校心理咨询系统的核心后端组件。它运行在服务器上，没有图形界面，专门负责处理所有的业务逻辑、数据持久化以及与客户端的通信。

## ✨ 核心功能
*   **TCP 连接池:** 使用 `QTcpServer` 管理多用户并发连接。
*   **JSON 协议路由:** 严格定义 JSON 格式，解析请求 (Login, Booking, Survey) 并分发至对应逻辑层。
*   **数据库管理:** 封装 `QtSql` 操作 MySQL (用户表, 预约表, 医生排班表)。
*   **消息推送:** 实现实时通知机制（如医生取消预约时，即时或离线通知学生）。
*   **统计分析:** 为管理员提供聚合查询数据（如 `GROUP BY` 统计）。

## 📂 目录结构
PsyServer/

├── config/             # 配置文件

├── src/

│   ├── core/           # 全局应用状态 & Session 管理

│   ├── network/        # TCP 监听器 & 客户端 Socket 封装

│   ├── logic/          # 业务逻辑处理 (Auth, Booking, Admin)

│   ├── dao/            # 数据访问层 (直接执行 SQL)

│   └── main.cpp        # 程序入口

└── CMakeLists.txt      # 构建脚本

## ⚙️ 环境要求
- C++ 编译器 (支持 C++17)
- Qt 6 SDK (Core, Network, Sql 模块)
- MySQL Server (8.0 或更高版本)
- CMake (3.16+)

## 🔨 构建与运行
### 1. 数据库配置
确保 MySQL 服务已启动。
### 2. 编译
mkdir build && cd build

cmake ..

cmake --build .
### 3. 运行
./PsyServer

## ⚠️ 注意事项
本项目 不包含任何 UI 代码。

代码中 不包含 客户端的逻辑，严格遵守前后端分离。
