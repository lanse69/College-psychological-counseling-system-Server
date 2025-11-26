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

    qDebug() << "Looking for config at:" << configPath;

    if (!QFile::exists(configPath)) {
        qCritical() << "Failed: Config file not found!";
        qCritical() << "Current Project Root is:" << PROJECT_ROOT_PATH;
        return -1;
    }

    if (!ConfigManager::instance().loadConfig(configPath)) {
        qCritical() << "Failed: Unable to load configuration json.";
        return -1;
    }
    qDebug() << "Success: Configuration loaded.";

    // 连接数据库 & 创建库
    if (!DBManager::instance().connectToDatabase()) {
        qCritical() << "Database initialization failed. Exiting.";
        return -1;
    }

    // 自动建表
    if (!DBManager::instance().initTables()) {
        qCritical() << "Failed: Table creation failed. Exiting.";
        return -1;
    }
    qDebug() << "Success: Tables initialized.";

    // 启动网络监听
    TcpListener server;
    int port = ConfigManager::instance().net().listenPort;
    
    // TcpListener 支持 maxConnections 设置
    server.setMaxPendingConnections(ConfigManager::instance().net().maxConnections);

    if (!server.start(port)) {
        return -1;
    }

    qDebug() << "=== PsyServer is Running ===";
    return app.exec();
}
