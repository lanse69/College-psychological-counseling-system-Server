#include <QCoreApplication>
#include <QDir>

#include "core/ConfigManager.h"
#include "dao/DBManager.h"
#include "network/TcpListener.h"

#ifndef PROJECT_ROOT_PATH
#define PROJECT_ROOT_PATH "."
#endif

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // 读取配置
    QString configPath = QString(PROJECT_ROOT_PATH) + "/config/serverConfig.json";

    qDebug() << "在此处找寻配置文件:" << configPath;

    if (!QFile::exists(configPath)) {
        qCritical() << "错误: 未找到配置文件!";
        qCritical() << "当前运行路径:" << PROJECT_ROOT_PATH;
        return -1;
    }

    if (!ConfigManager::instance().loadConfig(configPath)) {
        qCritical() << "失败: 未能加载 json 配置.";
        return -1;
    }
    qDebug() << "成功: 配置已加载.";

    // 连接数据库 & 创建库
    if (!DBManager::instance().connectToDatabase()) {
        qCritical() << "致命错误: 无法连接数据库，程序即将退出。";
        return -1;
    }

    // 自动建表
    if (!DBManager::instance().initTables()) {
        qCritical() << "失败: 创建表失败. 离开.";
        return -1;
    }
    qDebug() << "成功: 已初始化表.";

    // 启动网络监听
    TcpListener server;
    int port = ConfigManager::instance().net().listenPort;
    
    // 设置 maxConnections
    server.setMaxPendingConnections(ConfigManager::instance().net().maxConnections);

    if (!server.start(port)) {
        qCritical() << "致命错误: 端口" << port << "占用或无法监听。";
        return -1;
    }

    qDebug() << "=== 高校心理咨询系统服务端 (PsyServer) 已启动 ===";
    qDebug() << "监听端口:" << port;

    return app.exec();
}
