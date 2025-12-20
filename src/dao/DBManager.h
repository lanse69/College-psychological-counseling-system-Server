#pragma once

#include <QString>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <QMutex>
#include <QUuid>

class DBManager {
public:
    static DBManager& instance();

    // 初始化主线程数据库连接
    bool connectToDatabase();
    bool initTables();

    // 获取主线程的连接
    QSqlDatabase getMainDatabase() const;

    /**
     * @brief 为当前线程创建一个新的、独立的数据库连接
     * @param connectionName [输出参数] 返回生成的唯一连接名，用于后续清理
     * @return 打开的数据库对象
     */
    QSqlDatabase openThreadConnection(QString &connectionName);

    /**
     * @brief 关闭并移除线程的临时连接
     * @param connectionName openThreadConnection 返回的名字
     */
    void closeThreadConnection(const QString &connectionName);

private:
    DBManager();
    ~DBManager();
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;

    void seedDefaultAdmin();

    QSqlDatabase m_mainDb; // 主线程连接对象
    QString m_dbName;
};
