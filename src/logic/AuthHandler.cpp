#include "AuthHandler.h"

#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>

#include "dao/UserDao.h"
#include "core/ServerApp.h"
#include "core/ProtocolDefs.h"
#include "core/AsyncExecutor.h"

void AuthHandler::handleLogin(ClientSocket* sender, const QJsonObject& request) {
    QJsonObject data = request[JsonKeys::DATA].toObject();
    QString username = data[JsonKeys::USERNAME].toString();
    QString passwordHash = data[JsonKeys::PASSWORD].toString();

    AsyncExecutor::run(sender, 
        // Task
        [username, passwordHash](QSqlDatabase db) -> UserInfo {
            return UserDao::validateUser(db, username, passwordHash);
        },
        
        // Callback
        [sender](UserInfo user) {
            QJsonObject response;
            response[JsonKeys::CMD] = (int)CmdType::LOGIN;

            if (user.isValid()) {
                sender->setUserId(user.id);
                ServerApp::instance().registerUser(user.id, sender);

                response[JsonKeys::CODE] = (int)StatusCode::SUCCESS;
                response[JsonKeys::MSG] = "登录成功";
                QJsonObject respData;
                respData[JsonKeys::USER_ID] = user.id;
                respData[JsonKeys::USERNAME] = user.username;
                respData[JsonKeys::ROLE] = user.role;
                respData["realName"] = user.realName;
                response[JsonKeys::DATA] = respData;
            } else {
                response[JsonKeys::CODE] = (int)StatusCode::UNAUTHORIZED;
                response[JsonKeys::MSG] = "无效用户名或密码";
                qWarning() << "登录失败";
            }
            sender->sendJson(response);
        }
    );
}

void AuthHandler::handleGetUserInfo(ClientSocket* sender, const QJsonObject& request) {
    int userId = sender->userId();
    if (userId == -1) return; // 未登录
    AsyncExecutor::run(sender,
        [userId](QSqlDatabase db) -> QJsonObject {
            int role = UserDao::getUserRole(db, userId);
            if (role == (int)UserRole::DOCTOR) {
                return UserDao::getDoctorDetail(db, userId); 
            } else {
                QSqlQuery q(db);
                q.prepare("SELECT id, username, real_name, role FROM users WHERE id = ?");
                q.addBindValue(userId);
                if (q.exec() && q.next()) {
                    QJsonObject obj;
                    obj["id"] = q.value("id").toInt();
                    obj["username"] = q.value("username").toInt(); // wait, string
                    obj["username"] = q.value("username").toString();
                    obj["realName"] = q.value("real_name").toString();
                    obj["role"] = q.value("role").toInt();
                    return obj;
                }
                return QJsonObject();
            }
        },
        [sender, request](QJsonObject user) {
            QJsonObject response;
            response[JsonKeys::CMD] = request[JsonKeys::CMD];
            response[JsonKeys::CODE] = user.isEmpty() ? 404 : 200;
            response[JsonKeys::MSG] = user.isEmpty() ? "获取失败" : "获取成功";
            response[JsonKeys::DATA] = user;
            sender->sendJson(response);
        }
    );
}