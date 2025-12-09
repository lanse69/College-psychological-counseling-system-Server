#pragma once

#include <QtConcurrent>
#include <QFuture>
#include <QPointer>
#include <QCoreApplication>
#include <functional>

#include "network/ClientSocket.h"
#include "dao/DBManager.h"

class AsyncExecutor {
public:
    /**
     * @brief 执行异步数据库任务
     * @tparam Func 任务函数类型 (在子线程执行)
     * @tparam Callback 回调函数类型 (在主线程执行)
     * 
     * @param client 发起请求的客户端 (用于回调检查连接是否存活)
     * @param task 任务逻辑: [&](QSqlDatabase db) -> ResultType { ... }
     * @param callback 结果处理: [&](ResultType result) { ... }
     */
    template <typename Func, typename Callback>
    static void run(ClientSocket* client, Func task, Callback callback) {
        // 使用弱引用保护 ClientSocket
        QPointer<ClientSocket> safeClient(client);

        // 在线程池中运行
        QFuture<void> ignored = QtConcurrent::run([safeClient, task, callback]() {
            // 获取独立数据库连接
            QString connName;

            using ResultType = decltype(task(std::declval<QSqlDatabase>()));
            ResultType result;

            {
                QSqlDatabase db = DBManager::instance().openThreadConnection(connName);

                // 执行具体的 DAO 业务逻辑
                // 使用 auto 推导返回值类型
                result = task(db);
            }

            // 关闭数据库连接
            DBManager::instance().closeThreadConnection(connName);

            // 切换回主线程,发送响应
            QMetaObject::invokeMethod(QCoreApplication::instance(), [safeClient, callback, result]() {
                // 如果客户端还在线，则执行回调发送数据
                if (safeClient) {
                    callback(result);
                } else {
                    qDebug() << "异步任务完成，但客户端已断开，丢弃结果。";
                }
            });
        });
        Q_UNUSED(ignored);
    }
};