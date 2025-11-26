#include "ClientSocket.h"
#include "core/ProtocolDefs.h" // 引用协议定义
#include <QDebug>

int ClientSocket::userId() const { 
    return m_userId; 
}

void ClientSocket::setUserId(int id) { 
    m_userId = id; 
}

ClientSocket::ClientSocket(qintptr socketDescriptor, QObject *parent) 
    : QObject(parent) 
{
    m_socket = new QTcpSocket(this);
    if (!m_socket->setSocketDescriptor(socketDescriptor)) {
        qWarning() << "Socket error:" << m_socket->errorString();
        return;
    }

    connect(m_socket, &QTcpSocket::readyRead, this, &ClientSocket::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientSocket::onDisconnected);
    
    qDebug() << "New Client Connected:" << socketDescriptor;
}

ClientSocket::~ClientSocket() {
    if (m_socket && m_socket->isOpen()) {
        m_socket->close();
    }
}

void ClientSocket::sendJson(const QJsonObject &json) {
    if (!m_socket || !m_socket->isOpen()) return;

    QJsonDocument doc(json);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    
    // 写入长度 (quint32)
    out << (quint32)jsonData.size();
    // 写入数据
    packet.append(jsonData);

    m_socket->write(packet);
    m_socket->flush();
}

void ClientSocket::onReadyRead() {
    m_buffer.append(m_socket->readAll());

    while (true) {
        // 检查是否有完整的包头
        if (m_buffer.size() < PACKET_HEAD_SIZE) {
            return; // 数据不够，等待下一次 readyRead
        }

        // 读取包体长度 (使用 QDataStream 处理大小端)
        QDataStream stream(m_buffer);
        stream.setVersion(QDataStream::Qt_6_0);
        quint32 packetSize = 0;
        stream >> packetSize;

        // 检查是否有完整的包体
        if (m_buffer.size() < PACKET_HEAD_SIZE + packetSize) {
            return; // 数据不够，等待
        }

        // 提取包体数据
        QByteArray data = m_buffer.mid(PACKET_HEAD_SIZE, packetSize);
        
        // 从缓冲区移除已处理的数据
        m_buffer.remove(0, PACKET_HEAD_SIZE + packetSize);

        // 解析 JSON
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            emit jsonReceived(this, doc.object());
        } else {
            qWarning() << "JSON Parse Error:" << parseError.errorString();
        }
    }
}

void ClientSocket::onDisconnected() {
    qDebug() << "Client Disconnected";
    emit disconnected(this);
    deleteLater(); // 自我销毁
}
