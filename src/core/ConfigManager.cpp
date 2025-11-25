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
        qCritical() << "Failed to open config file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qCritical() << "Config JSON parse error:" << parseError.errorString();
        return false;
    }

    QJsonObject root = doc.object();

    // 解析 Database 节点
    if (root.contains("database")) {
        QJsonObject dbObj = root["database"].toObject();
        m_dbConfig.host = dbObj["host"].toString("127.0.0.1");
        m_dbConfig.port = dbObj["port"].toInt(3306);
        m_dbConfig.username = dbObj["username"].toString("root");
        m_dbConfig.password = dbObj["password"].toString("");
        m_dbConfig.dbName = dbObj["db_name"].toString("psy_db");
    } else {
        qCritical() << "Config missing 'database' node";
        return false;
    }

    // 解析 Network 节点
    if (root.contains("network")) {
        QJsonObject netObj = root["network"].toObject();
        m_netConfig.listenPort = netObj["listen_port"].toInt(9999);
        m_netConfig.maxConnections = netObj["max_connections"].toInt(100);
    } else {
        qWarning() << "Config missing 'network' node, using defaults.";
        m_netConfig.listenPort = 9999;
        m_netConfig.maxConnections = 100;
    }

    qDebug() << "Configuration loaded successfully.";
    return true;
}
