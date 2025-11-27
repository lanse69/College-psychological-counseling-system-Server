#pragma once

#include <QObject>
#include <QMutex>

#include "network/ClientSocket.h"

class ServerApp : public QObject {
    Q_OBJECT
public:
    static ServerApp& instance();

    // 注册在线用户 (登录成功后调用)
    void registerUser(int userId, ClientSocket* socket);

    // 用户下线
    void unregisterUser(int userId);

    // 获取用户 Socket 用于推送
    ClientSocket* getClient(int userId);

private:
    ServerApp() = default;

    QMap<int, ClientSocket*> m_onlineUsers;
    QMutex m_mutex;
};
