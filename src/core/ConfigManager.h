#pragma once

#include <QString>
#include <QJsonObject>

struct DBConfig {
    QString host;
    int port;
    QString username;
    QString password;
    QString dbName;
};

struct NetworkConfig {
    int listenPort;
    int maxConnections;
};

class ConfigManager {
public:
    static ConfigManager& instance();

    // 加载配置文件
    bool loadConfig(const QString &filePath);

    const DBConfig& db() const { return m_dbConfig; }
    const NetworkConfig& net() const { return m_netConfig; }

private:
    ConfigManager() = default;
    
    DBConfig m_dbConfig;
    NetworkConfig m_netConfig;
};
