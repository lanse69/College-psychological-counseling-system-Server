# 🖥️ PsyServer - 心理咨询系统服务端

## 📖 简介
**PsyServer** 是高校心理咨询系统的核心后端组件。它运行在服务器上，没有图形界面，专门负责处理所有的业务逻辑、数据持久化以及与客户端的通信。

## ✨ 核心功能
*   **TCP 连接池:** 使用 `QTcpServer` 管理多用户并发连接。
*   **JSON 协议路由:** 严格定义 JSON 格式，解析请求 (Login, Booking, Survey) 并分发至对应逻辑层。
*   **数据库管理:** 封装 `QtSql` 操作 PostgreSQL (用户表, 预约表, 医生排班表)。
*   **消息推送:** 实现实时通知机制（如医生取消预约时，即时或离线通知学生）。
*   **统计分析:** 为管理员提供聚合查询数据（如 `GROUP BY` 统计）。

## 📂 目录结构
```Text
PsyServer/
├── config/             # 配置文件
├── src/
│   ├── core/           # 全局应用状态 & Session 管理
│   ├── network/        # TCP 监听器 & 客户端 Socket 封装
│   ├── logic/          # 业务逻辑处理 (Auth, Booking, Admin)
│   ├── dao/            # 数据访问层 (直接执行 SQL)
│   └── main.cpp        # 程序入口
└── CMakeLists.txt      # 构建脚本
```

## ⚙️ 环境要求
- C++ 编译器 (支持 C++17)
- Qt 6 SDK (Core, Network, Sql 模块及 PostgreSQL 驱动)
- PostGreSQL Server 12+
- CMake (3.16+)

## Ubuntu 依赖安装命令
```bash
sudo apt update
sudo apt install build-essential cmake

# 安装 Qt6 基础库及 SQL 模块
sudo apt install qt6-base-dev libqt6sql6-psql

# 安装 PostgreSQL 数据库及开发库
sudo apt install postgresql libpq-dev
```
## 🔨 构建与运行
### 1. 数据库初始化 (PostgreSQL)
确保 PostGreSQL 服务已启动，在编译运行前，必须先在 PostgreSQL 中创建对应的用户和数据库。

1. 切换到 postgres 用户:
```Bash
sudo -u postgres psql
```
2. 执行 SQL 建库命令 (请确保密码与 serverConfig.json 中一致):
```SQL
-- 创建用户 (用户: PsyServer, 密码: PsyDB@Of@PostgreSQL)
CREATE USER "PsyServer" WITH PASSWORD 'PsyDB@Of@PostgreSQL';

-- 赋予这个用户建库权限
ALTER USER "PsyServer" CREATEDB;

-- 创建数据库并指定所有者
CREATE DATABASE "PsyDB" OWNER "PsyServer";

-- 赋予权限
GRANT ALL PRIVILEGES ON DATABASE "PsyDB" TO "PsyServer";

-- 退出
\q
```
### 2. 网络配置 (IP 与 防火墙)
为了让客户端（其他 PC 或手机）能连接到服务端，需要配置网络。

1.获取服务端 IP 地址:
```Bash
ip addr show
```
记录 inet 后面的 IP 地址，在客户端的LoginView.qml代码中需要输入此 IP。

2. 配置防火墙 (UFW):
允许外部设备访问 TCP 9999 端口。
```Bash
# 开放端口
sudo ufw allow 9999/tcp
# 重载规则
sudo ufw reload
# 检查状态
sudo ufw status
```
3. 项目配置
检查 config/serverConfig.json 文件，确保配置正确：
```JSON
{
    "database": {
        "host": "127.0.0.1",
        "port": 5432,
        "username": "PsyServer",
        "password": "PsyDB@Of@PostgreSQL",
        "db_name": "PsyDB"
    },
    "network": {
        "listen_port": 9999,
        "max_connections": 100
    }
}
```
4. 编译与安装
```Bash
# 1. 创建构建目录
mkdir build && cd build

# 2. 生成 Makefile
cmake ..

# 3. 编译
cmake --build .
```
5. 运行服务端
```Bash
./PsyServer
```
成功标志: 控制台输出 PsyServer listening on port 9999。
首次运行: 程序会自动在数据库中创建所需的数据表（users, appointments 等）并初始化默认管理员账号。

## ⚠️ 常见问题 (FAQ)
Q: 客户端显示“连接超时”或“主机不可达”？
- 检查 Ubuntu 防火墙是否已开放 9999 端口。
- 检查客户端和服务端是否在同一网段（尝试 ping 服务端 IP）。
- 若在校园网环境下 Ping 不通，可能存在 AP 隔离，建议使用手机热点组建局域网测试。

Q: 运行报错 QPSQL driver not loaded？
- 请检查是否安装了 libqt6sql6-psql 包。

Q: 运行报错 FATAL: password authentication failed？
- 请检查 PostgreSQL 中创建用户的密码是否与 config/serverConfig.json 中的密码完全一致。

## 📝 注意事项
本项目 不包含任何 UI 代码，仅为控制台程序。

服务端必须先于客户端启动。

代码严格遵守前后端分离架构，不包含客户端业务逻辑。
