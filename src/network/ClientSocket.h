#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QDateTime>

class ClientSocket : public QObject {
    Q_OBJECT
public:
    explicit ClientSocket(qintptr socketDescriptor, QObject *parent = nullptr);
    ~ClientSocket();

    int userId() const;
    
    // 发送 JSON 数据给客户端
    void sendJson(const QJsonObject &json);

    // 主动断开连接
    void disconnectFromHost();

    // 获取最后活跃时间
    qint64 lastActiveTime() const;
    // 更新活跃时间
    void updateActiveTime();

signals:
    // 解析出完整的 JSON 包后发出信号，由 RequestRouter 处理
    void jsonReceived(ClientSocket* sender, const QJsonObject &json);
    void disconnected(ClientSocket* sender);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    void setUserId(int id);

    friend class AuthHandler;
    friend class ServerApp;

    QTcpSocket *m_socket;
    QByteArray m_buffer; // 接收缓冲区
    int m_userId;   // 关联的用户ID
    qint64 m_lastActiveTime;
};
