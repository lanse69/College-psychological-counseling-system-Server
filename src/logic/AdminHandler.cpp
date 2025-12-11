#include "AdminHandler.h"

#include <QJsonArray>
#include <QDebug>
#include <QDateTime>

#include "dao/UserDao.h"
#include "dao/DBManager.h"
#include "network/ClientSocket.h"
#include "core/ProtocolDefs.h"
#include "core/ServerApp.h"
#include "core/AsyncExecutor.h"

// 发送通用响应
static void sendResponse(ClientSocket* sender, int cmd, int code, const QString& msg, const QJsonValue& data = QJsonValue()) {
    if (!sender) return;
    QJsonObject response;
    response[JsonKeys::CMD] = cmd;
    response[JsonKeys::CODE] = code;
    response[JsonKeys::MSG] = msg;
    if (!data.isNull()) {
        response[JsonKeys::DATA] = data;
    }
    sender->sendJson(response);
}

// 检查是否为管理员
static bool isUserAdmin(QSqlDatabase db, int userId) {
    return UserDao::getUserRole(db, userId) == (int)UserRole::ADMIN;
}

void AdminHandler::handleAddUser(ClientSocket* sender, const QJsonObject& request) {
    int operatorId = sender->userId();
    int cmd = request[JsonKeys::CMD].toInt();
    QJsonObject data = request[JsonKeys::DATA].toObject();

    // 提取参数
    QString username = data[JsonKeys::USERNAME].toString();
    QString passHash = data[JsonKeys::PASSWORD].toString();
    QString realName = data[JsonKeys::REAL_NAME].toString();
    int role = data[JsonKeys::ROLE].toInt();
    QString intro = data[JsonKeys::INTRO].toString();
    QString spec = data[JsonKeys::SPEC].toString();

    AsyncExecutor::run(sender,
        [operatorId, username, passHash, realName, role, intro, spec](QSqlDatabase db) -> QPair<int, QString> {
            
            // 权限校验
            if (!isUserAdmin(db, operatorId)) {
                return {StatusCode::FORBIDDEN, "无权操作"};
            }

            // 参数校验
            if (username.isEmpty() || passHash.isEmpty()) {
                return {StatusCode::BAD_REQUEST, "用户名或密码不能为空"};
            }

            // 业务逻辑
            if (UserDao::isUsernameExist(db, username)) {
                return {StatusCode::CONFLICT, "用户名已存在"};
            }

            int newId = UserDao::addUser(db, username, passHash, role, realName);
            if (newId != -1) {
                if (role == (int)UserRole::DOCTOR) {
                    UserDao::addDoctorInfo(db, newId, intro, spec);
                }
                return {StatusCode::SUCCESS, "添加用户成功"};
            }
            return {StatusCode::INTERNAL_ERROR, "数据库插入失败"};
        },
        [sender, cmd](QPair<int, QString> result) {
            // 主线程回调
            sendResponse(sender, cmd, result.first, result.second);
        }
    );
}

void AdminHandler::handleDeleteUser(ClientSocket* sender, const QJsonObject& request) {
    int operatorId = sender->userId();
    int cmd = request[JsonKeys::CMD].toInt();
    
    QJsonObject data = request[JsonKeys::DATA].toObject();
    int targetId = data[JsonKeys::TARGET_ID].toInt();

    AsyncExecutor::run(sender,
        [operatorId, targetId](QSqlDatabase db) -> QPair<int, QString> {
            // 权限校验
            if (!isUserAdmin(db, operatorId)) return {StatusCode::FORBIDDEN, "无权操作"};
            
            // 校验参数
            if (targetId <= 0) return {StatusCode::BAD_REQUEST, "无效的用户ID"};
            if (targetId == operatorId) return {StatusCode::BAD_REQUEST, "不能删除自己"};

            // 执行删除
            if (UserDao::deleteUser(db, targetId)) {
                return {StatusCode::SUCCESS, "用户删除成功"};
            }
            return {StatusCode::INTERNAL_ERROR, "删除失败，ID可能不存在"};
        },
        [sender, cmd, targetId](QPair<int, QString> result) {
            // 如果删除成功，需要处理被删除用户的在线连接
            if (result.first == StatusCode::SUCCESS) {
                ClientSocket* targetSocket = ServerApp::instance().getClient(targetId);
                if (targetSocket) {
                    QJsonObject kickMsg;
                    kickMsg[JsonKeys::CMD] = (int)CmdType::PUSH_NOTIFICATION;
                    kickMsg[JsonKeys::CODE] = (int)StatusCode::CONFLICT;
                    kickMsg[JsonKeys::MSG] = "管理员删除了您的账号，连接断开。";
                    targetSocket->sendJson(kickMsg);
                    // 稍后断开，确保消息发出
                    QTimer::singleShot(100, targetSocket, &ClientSocket::disconnectFromHost);
                }
            }
            sendResponse(sender, cmd, result.first, result.second);
        }
    );
}

void AdminHandler::handleUpdateUserInfo(ClientSocket* sender, const QJsonObject& request) {
    int operatorId = sender->userId();
    int cmd = request[JsonKeys::CMD].toInt();

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int targetId = data[JsonKeys::TARGET_ID].toInt();
    QString realName = data[JsonKeys::REAL_NAME].toString();
    QString passHash = data[JsonKeys::PASSWORD].toString();
    QString intro = data[JsonKeys::INTRO].toString();
    QString spec = data[JsonKeys::SPEC].toString();

    struct UpdateResult {
        int code;
        QString msg;
        bool passChanged;
    };

    AsyncExecutor::run(sender,
        [=](QSqlDatabase db) -> UpdateResult {
            bool isAdmin = isUserAdmin(db, operatorId);
            if (!isAdmin && operatorId != targetId) {
                return {StatusCode::FORBIDDEN, "无权操作", false};
            }

            bool success = UserDao::updateBasicInfo(db, targetId, realName, passHash);
            
            // 医生需要更新详细信息
            int targetRole = UserDao::getUserRole(db, targetId);
            if (success && targetRole == (int)UserRole::DOCTOR) {
                success &= UserDao::updateDoctorInfo(db, targetId, intro, spec);
            }

            if (success) {
                return {StatusCode::SUCCESS, "用户信息更新成功", !passHash.isEmpty()};
            }
            return {StatusCode::INTERNAL_ERROR, "更新失败", false};
        },
        [sender, cmd, targetId](UpdateResult res) {
            // 推送通知给目标用户
            if (res.code == StatusCode::SUCCESS) {
                ClientSocket* targetClient = ServerApp::instance().getClient(targetId);
                if (targetClient) {
                    QJsonObject notify;
                    notify[JsonKeys::CMD] = (int)CmdType::PUSH_NOTIFICATION;
                    
                    if (res.passChanged) {
                        notify[JsonKeys::MSG] = "您的密码已被管理员重置，请重新登录。";
                        notify[JsonKeys::CODE] = (int)StatusCode::CONFLICT; // 强制下线码
                        targetClient->sendJson(notify);
                        QTimer::singleShot(100, targetClient, &ClientSocket::disconnectFromHost);
                    } else {
                        notify[JsonKeys::MSG] = "您的个人信息已被管理员修改，请刷新查看。";
                        targetClient->sendJson(notify);
                    }
                }
            }
            sendResponse(sender, cmd, res.code, res.msg);
        }
    );
}

void AdminHandler::handleGetStatistics(ClientSocket* sender, const QJsonObject& request) {
    int operatorId = sender->userId();
    int cmd = request[JsonKeys::CMD].toInt();
    QJsonObject data = request[JsonKeys::DATA].toObject();
    QString statType = data["type"].toString();

    AsyncExecutor::run(sender,
        [operatorId, statType](QSqlDatabase db) -> QPair<int, QJsonValue> {
            if (!isUserAdmin(db, operatorId)) return {StatusCode::FORBIDDEN, QJsonValue()};

            QJsonArray resultArray;
            QSqlQuery query(db);

            if (statType == "consult_trend") {
                // 统计最近12个月
                QString sql = R"(
                    SELECT to_char(date, 'YYYY-MM') as month, COUNT(*)
                    FROM appointments
                    WHERE status = 2
                    GROUP BY month
                    ORDER BY month DESC LIMIT 12
                )";
                if (query.exec(sql)) {
                    while(query.next()) {
                        QJsonObject item;
                        item["label"] = query.value(0).toString();
                        item["value"] = query.value(1).toInt();
                        resultArray.append(item);
                    }
                }
            } else if (statType == "common_issues") {
                // 统计 Tag
                QString sql = R"(
                    SELECT result_tags, COUNT(*)
                    FROM consultation_records
                    WHERE result_tags IS NOT NULL AND result_tags != ''
                    GROUP BY result_tags
                    ORDER BY count DESC LIMIT 10
                )";
                if (query.exec(sql)) {
                    while(query.next()) {
                        QJsonObject item;
                        item["label"] = query.value(0).toString();
                        item["value"] = query.value(1).toInt();
                        resultArray.append(item);
                    }
                }
            }
            // 还有其他类型可扩展...

            return {StatusCode::SUCCESS, resultArray};
        },
        [sender, cmd](QPair<int, QJsonValue> result) {
            sendResponse(sender, cmd, result.first, 
                         result.first == StatusCode::SUCCESS ? "获取成功" : "获取失败", 
                         result.second);
        }
    );
}

void AdminHandler::handleGetUserList(ClientSocket* sender, const QJsonObject& request) {
    int operatorId = sender->userId();
    int cmd = request[JsonKeys::CMD].toInt();

    AsyncExecutor::run(sender,
        [operatorId](QSqlDatabase db) -> QPair<int, QJsonValue> {
            if (!isUserAdmin(db, operatorId)) {
                return {StatusCode::FORBIDDEN, QJsonValue()};
            }
            QJsonArray list = UserDao::getAllUsers(db, operatorId);
            return {StatusCode::SUCCESS, list};
        },
        [sender, cmd](QPair<int, QJsonValue> result) {
            sendResponse(sender, cmd, result.first, "获取用户列表成功", result.second);
        }
    );
}