#pragma once

#include <QObject>
#include <QJsonObject>
#include <QMap>

class ClientSocket;

class RequestRouter : public QObject
{
    Q_OBJECT
public:
    // 单例访问
    static RequestRouter& instance();

    /**
     * @brief 核心分发函数
     * @param sender 发送请求的客户端 Socket 对象（用于回包）
     * @param request 客户端发送的完整 JSON 数据
     */
    void dispatch(ClientSocket* sender, const QJsonObject& request);

private:
    RequestRouter() = default;
    ~RequestRouter() = default;
    RequestRouter(const RequestRouter&) = delete;
    RequestRouter& operator=(const RequestRouter&) = delete;
};
