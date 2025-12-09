#include "TcpListener.h"

#include <QDebug>
#include <QDateTime>

#include "logic/RequestRouter.h" // 引入路由
#include "core/ServerApp.h"      // 引入全局状态

TcpListener::TcpListener(QObject *parent) : QTcpServer(parent) {
    m_checkTimer = new QTimer(this);
    m_checkTimer->setInterval(10000); 
    connect(m_checkTimer, &QTimer::timeout, this, &TcpListener::onCheckHeartbeat);
}

bool TcpListener::start(int port) {
    if (!this->listen(QHostAddress::Any, port)) {
        qCritical() << "错误: 服务端无法监听端口 " << port;
        return false;
    }
    qDebug() << "服务端开始监听端口 " << port;

    m_checkTimer->start();

    return true;
}

void TcpListener::incomingConnection(qintptr socketDescriptor) {
    ClientSocket *client = new ClientSocket(socketDescriptor, this);
    
    connect(client, &ClientSocket::jsonReceived, this, &TcpListener::onClientJsonReceived);
    connect(client, &ClientSocket::disconnected, this, &TcpListener::onClientDisconnected);

    m_clients.insert(socketDescriptor, client);

    qDebug() << "新客户端接入. 描述符:" << socketDescriptor
             << " 当前连接数:" << m_clients.size();
}

void TcpListener::onClientJsonReceived(ClientSocket* sender, const QJsonObject& json) {
    // 安全检查
    if (!sender) {
        qWarning() << "TCP 监听错误: 接收到未知来源信号.";
        return;
    }
    
    RequestRouter::instance().dispatch(sender, json);
}

void TcpListener::onClientDisconnected(ClientSocket* sender) {
    qintptr keyToRemove = -1;
    bool found = false;

    // 遍历 Map 寻找 value 等于 sender 的项
    QMapIterator<qintptr, ClientSocket*> i(m_clients);
    while (i.hasNext()) {
        i.next();
        if (i.value() == sender) {
            keyToRemove = i.key();
            found = true;
            break;
        }
    }

    if (found) {
        m_clients.remove(keyToRemove);
        qDebug() << "客户端从 Map 中移除. 描述符:" << keyToRemove
                 << " 剩余:" << m_clients.size();
    }

    if (sender->userId() != -1) {
        ServerApp::instance().unregisterUser(sender->userId());
    }
}

// 心跳检测逻辑
void TcpListener::onCheckHeartbeat() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    // 超时阈值：60秒 (允许丢失1个心跳包)
    const qint64 timeoutThreshold = 60000; 
    
    QList<ClientSocket*> timeoutClients;

    for (ClientSocket* client : m_clients) {
        if (client && (now - client->lastActiveTime() > timeoutThreshold)) {
            timeoutClients.append(client);
        }
    }

    // 执行断开操作
    for (ClientSocket* client : timeoutClients) {
        qInfo() << "检测到僵尸连接 (User:" << client->userId() << ")，强制断开。";
        client->disconnectFromHost(); 
    }
}