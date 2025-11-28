#include "ConfigManager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadConfig(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "打开配置文件失败:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qCritical() << "JSON配置解析错误:" << parseError.errorString();
        return false;
    }

    QJsonObject root = doc.object();

    // 解析 Database 节点
    if (root.contains("database")) {
        QJsonObject dbObj = root["database"].toObject();
        m_dbConfig.host = dbObj["host"].toString("127.0.0.1");
        m_dbConfig.port = dbObj["port"].toInt(5432);
        m_dbConfig.username = dbObj["username"].toString("PsyServer");
        m_dbConfig.password = dbObj["password"].toString("PsyDB@Of@PostgreSQL");
        m_dbConfig.dbName = dbObj["db_name"].toString("PsyDB");
    } else {
        qCritical() << "缺少数据库节点配置";
        return false;
    }

    // 解析 Network 节点
    if (root.contains("network")) {
        QJsonObject netObj = root["network"].toObject();
        m_netConfig.listenPort = netObj["listen_port"].toInt(9999);
        m_netConfig.maxConnections = netObj["max_connections"].toInt(100);
    } else {
        qWarning() << "缺少网络节点配置, 使用默认.";
        m_netConfig.listenPort = 9999;
        m_netConfig.maxConnections = 100;
    }

    qDebug() << "配置加载成功.";
    return true;
}

const DBConfig& ConfigManager::db() const {
    return m_dbConfig;
}

const NetworkConfig& ConfigManager::net() const {
    return m_netConfig;
}
