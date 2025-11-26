#include "AuthHandler.h"

#include <QCryptographicHash>
#include <QDebug>

#include "dao/UserDao.h"
#include "core/ServerApp.h"
#include "core/ProtocolDefs.h"

void AuthHandler::handleLogin(ClientSocket* sender, const QJsonObject& request) {
    QJsonObject data = request[JsonKeys::DATA].toObject();
    QString username = data[JsonKeys::USERNAME].toString();
    QString password = data[JsonKeys::PASSWORD].toString();

    // 计算密码 Hash (SHA256)
    QString hashedPassword = QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());

    // 查库验证
    UserInfo user = UserDao::validateUser(username, hashedPassword);

    QJsonObject response;
    response[JsonKeys::CMD] = (int)CmdType::LOGIN;

    if (user.isValid()) {
        // 登录成功
        qDebug() << "User login success:" << username << "Role:" << user.role;

        // 绑定 Socket 与 UserID
        sender->setUserId(user.id);
        // 注册到全局在线列表
        ServerApp::instance().registerUser(user.id, sender);

        response[JsonKeys::CODE] = 200;
        response[JsonKeys::MSG] = "Login Success";
        
        QJsonObject respData;
        respData[JsonKeys::USER_ID] = user.id;
        respData[JsonKeys::USERNAME] = user.username;
        respData[JsonKeys::ROLE] = user.role;
        respData["realName"] = user.realName;
        
        response[JsonKeys::DATA] = respData;
    } else {
        // 登录失败
        qWarning() << "User login failed:" << username;
        response[JsonKeys::CODE] = 401;
        response[JsonKeys::MSG] = "Invalid username or password";
    }

    // 发送回包
    sender->sendJson(response);
}