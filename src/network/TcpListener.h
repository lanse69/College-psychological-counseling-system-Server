#pragma once

#include <QTcpServer>
#include <QMap>
#include "ClientSocket.h"

class TcpListener : public QTcpServer {
    Q_OBJECT
public:
    explicit TcpListener(QObject *parent = nullptr);
    bool start(int port);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientJsonReceived(ClientSocket* sender, const QJsonObject& json);
    void onClientDisconnected(ClientSocket* sender);

private:
    // Key: socketDescriptor (唯一标识), Value: ClientSocket对象指针
    QMap<qintptr, ClientSocket*> m_clients; 
};
