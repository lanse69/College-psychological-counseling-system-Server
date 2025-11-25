#pragma once

#include <QString>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <QMutex>

class DBManager {
public:
    // 单例获取
    static DBManager& instance();

    // 初始化数据库连接
    bool connectToDatabase();

    // 创建所有表
    bool initTables();

    // 获取数据库对象
    QSqlDatabase getDatabase() const;

private:
    DBManager();
    ~DBManager();
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;

    bool createTable(const QString &tableName, const QString &sql);
    void seedDefaultAdmin(); // 预置管理员

    QSqlDatabase m_db;
    QString m_dbName;
};
