#include "ServerApp.h"

#include <QMap>
#include <QDebug>

#include "ProtocolDefs.h" // 引入协议定义

ServerApp& ServerApp::instance() {
    static ServerApp _instance;
    return _instance;
}

// 注册在线用户 (登录成功后调用)
void ServerApp::registerUser(int userId, ClientSocket* socket) {
    QMutexLocker locker(&m_mutex); // 打开互斥锁
    // 如果用户之前有连接
    if (m_onlineUsers.contains(userId)) {
        ClientSocket* oldSocket = m_onlineUsers.value(userId);

        // 同一个 Socket 重复发包，不做处理
        if (oldSocket == socket) {
            return;
        }

        qInfo() << "用户 " << userId << " 从新位置登陆. 断开前连接.";

        // 通知旧客户端
        QJsonObject kickMsg;
        kickMsg[JsonKeys::CMD] = (int)CmdType::PUSH_NOTIFICATION;
        kickMsg[JsonKeys::CODE] = (int)StatusCode::CONFLICT; // Conflict
        kickMsg[JsonKeys::MSG] = "您的账号已在其他设备登录，本连接即将断开。";
        oldSocket->sendJson(kickMsg);

        oldSocket->disconnectFromHost();

        // 旧 Socket 的 UserId 重置为 -1
        oldSocket->setUserId(-1);

        // 强制断开旧连接
        QMetaObject::invokeMethod(oldSocket, "deleteLater", Qt::QueuedConnection);
    }
    // 插入新连接 (新用户则新增，顶号则覆盖)
    m_onlineUsers.insert(userId, socket);

    // 绑定 Socket 与 UserID (用于断线反查)
    socket->setUserId(userId);

    qDebug() << "用户上线:" << userId << " 当前在线人数:" << m_onlineUsers.size();
}

// 用户下线
void ServerApp::unregisterUser(int userId) {
    QMutexLocker locker(&m_mutex);
    m_onlineUsers.remove(userId);
    qDebug() << "用户下线:" << userId;
}

// 获取用户 Socket 用于推送
ClientSocket* ServerApp::getClient(int userId) {
    QMutexLocker locker(&m_mutex);
    return m_onlineUsers.value(userId, nullptr);
}
