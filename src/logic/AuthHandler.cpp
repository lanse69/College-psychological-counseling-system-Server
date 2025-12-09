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
    // 获取当前 Socket 绑定的用户 ID
    int userId = sender->userId();
    
    // 校验登录状态
    if (userId <= 0) {
        QJsonObject response;
        response[JsonKeys::CMD] = request[JsonKeys::CMD];
        response[JsonKeys::CODE] = (int)StatusCode::UNAUTHORIZED;
        response[JsonKeys::MSG] = "用户未登录";
        sender->sendJson(response);
        return;
    }

    // 执行数据库查询
    AsyncExecutor::run(sender,
        [userId](QSqlDatabase db) -> QJsonObject {
            QJsonObject userData;
            
            if (!db.isOpen()) return userData; // 返回空对象表示失败

            QSqlQuery query(db);
            // 使用 LEFT JOIN 关联查询，即使用户不是医生也能查出基本信息
            QString sql = R"(
                SELECT 
                    u.id, u.username, u.real_name, u.role, 
                    d.intro, d.specialized_field 
                FROM users u 
                LEFT JOIN doctor_info d ON u.id = d.user_id 
                WHERE u.id = :id
            )";

            query.prepare(sql);
            query.bindValue(":id", userId);

            if (query.exec() && query.next()) {
                userData[JsonKeys::USER_ID] = query.value("id").toInt();
                userData[JsonKeys::USERNAME] = query.value("username").toString();
                userData[JsonKeys::REAL_NAME] = query.value("real_name").toString();
                userData[JsonKeys::ROLE] = query.value("role").toInt();
                
                // 只有医生才会有这些字段，学生查出来是空字符串，不影响前端显示
                userData[JsonKeys::INTRO] = query.value("intro").toString();
                userData[JsonKeys::SPEC] = query.value("specialized_field").toString();
            } else {
                qWarning() << "获取用户信息失败 ID:" << userId << query.lastError().text();
            }
            return userData;
        },
        // 主线程回调发送响应
        [sender, request](QJsonObject userData) {
            QJsonObject response;
            response[JsonKeys::CMD] = request[JsonKeys::CMD];

            if (!userData.isEmpty()) {
                response[JsonKeys::CODE] = (int)StatusCode::SUCCESS;
                response[JsonKeys::MSG] = "获取用户信息成功";
                response[JsonKeys::DATA] = userData;
            } else {
                response[JsonKeys::CODE] = (int)StatusCode::NOT_FOUND;
                response[JsonKeys::MSG] = "用户不存在或查询失败";
            }
            
            sender->sendJson(response);
        }
    );
}