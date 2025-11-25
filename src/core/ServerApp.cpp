#include "ServerApp.h"
#include <QMap>

ServerApp& ServerApp::instance() {
    static ServerApp _instance;
    return _instance;
}

// 注册在线用户 (登录成功后调用)
void ServerApp::registerUser(int userId, ClientSocket* socket) {
    QMutexLocker locker(&m_mutex);
    // 如果该用户之前有连接，可以踢掉旧连接或覆盖
    if (m_onlineUsers.contains(userId)) {
        // Log warning or disconnect old
    }
    m_onlineUsers.insert(userId, socket);
    // 同时也为了方便 socket 断开时反向查找，Socket 类里最好存了 userId
    socket->setUserId(userId); 
    qDebug() << "User logged in:" << userId;
}

// 用户下线
void ServerApp::unregisterUser(int userId) {
    QMutexLocker locker(&m_mutex);
    m_onlineUsers.remove(userId);
    qDebug() << "User logged out:" << userId;
}

// 获取用户 Socket 用于推送
ClientSocket* ServerApp::getClient(int userId) {
    QMutexLocker locker(&m_mutex);
    return m_onlineUsers.value(userId, nullptr);
}