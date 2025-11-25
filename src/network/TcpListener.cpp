#include "TcpListener.h"
#include "logic/RequestRouter.h" // 引入路由
#include "core/ServerApp.h"      // 引入全局状态
#include <QDebug>

TcpListener::TcpListener(QObject *parent) : QTcpServer(parent) {}

bool TcpListener::start(int port) {
    if (!this->listen(QHostAddress::Any, port)) {
        qCritical() << "Server could not start on port" << port;
        return false;
    }
    qDebug() << "PsyServer listening on port" << port;
    return true;
}

void TcpListener::incomingConnection(qintptr socketDescriptor) {
    ClientSocket *client = new ClientSocket(socketDescriptor, this);
    
    connect(client, &ClientSocket::jsonReceived, this, &TcpListener::onClientJsonReceived);
    connect(client, &ClientSocket::disconnected, this, &TcpListener::onClientDisconnected);
    
    m_clients.insert(socketDescriptor, client);
    
    qDebug() << "New Connection stored in Map. Descriptor:" << socketDescriptor 
             << "Total Clients:" << m_clients.size();
}

void TcpListener::onClientJsonReceived(ClientSocket* sender, const QJsonObject& json) {
    qDebug() << "Received from client" << sender << ":" << json;
    // TODO
    // 将请求转交给路由处理
    // RequestRouter 也可以是单例，或者静态方法类
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
        qDebug() << "Client removed from Map. Descriptor:" << keyToRemove 
                 << "Remaining:" << m_clients.size();
    }

    if (sender->userId() != -1) {
        ServerApp::instance().unregisterUser(sender->userId());
    }
}
