#pragma once

#include <QObject>
#include <QJsonObject>

class ClientSocket;

class AuthHandler : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 处理登录请求
     * @param sender 发送请求的客户端 Socket
     * @param request 完整的 JSON 请求数据
     */
    static void handleLogin(ClientSocket* sender, const QJsonObject& request);

private:
    AuthHandler() = default;
};
