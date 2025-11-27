#include "AdminHandler.h"

#include <QtConcurrent>
#include <QJsonArray>
#include <QPointer>
#include <QCryptographicHash>
#include <QDebug>

#include "dao/UserDao.h"
#include "dao/DBManager.h"
#include "network/ClientSocket.h"
#include "core/ProtocolDefs.h"
#include "core/ServerApp.h"

bool AdminHandler::checkAdminPermission(ClientSocket* sender) {
    if (!sender) return false;
    int role = UserDao::getUserRole(sender->userId());
    return role == (int)UserRole::ADMIN;
}

void AdminHandler::handleAddUser(ClientSocket* sender, const QJsonObject& request) {
    if (!checkAdminPermission(sender)) {
        QJsonObject err;
        err[JsonKeys::CMD] = request[JsonKeys::CMD];
        err[JsonKeys::CODE] = 403;
        err[JsonKeys::MSG] = "无权操作";
        sender->sendJson(err);
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();
    QString username = data[JsonKeys::USERNAME].toString();
    QString rawPass  = data[JsonKeys::PASSWORD].toString();
    QString realName = data[JsonKeys::REAL_NAME].toString();
    int role = data[JsonKeys::ROLE].toInt();

    QJsonObject response;
    response[JsonKeys::CMD] = request[JsonKeys::CMD];

    // 检查参数
    if (username.isEmpty() || rawPass.isEmpty()) {
        response[JsonKeys::CODE] = 400;
        response[JsonKeys::MSG] = "用户名或密码不能为空";
        sender->sendJson(response);
        return;
    }

    // 检查用户是否存在
    if (UserDao::isUsernameExist(username)) {
        response[JsonKeys::CODE] = 409; // Conflict
        response[JsonKeys::MSG] = "用户名已存在";
        sender->sendJson(response);
        return;
    }

    // 计算 Hash
    QString passHash = QString(QCryptographicHash::hash(rawPass.toUtf8(), QCryptographicHash::Sha256).toHex());

    // 执行插入
    int newId = UserDao::addUser(username, passHash, role, realName);

    if (newId != -1) {
        // 医生需要插入 doctor_info
        if (role == (int)UserRole::DOCTOR) {
            QString intro = data[JsonKeys::INTRO].toString();
            QString spec = data[JsonKeys::SPEC].toString();
            UserDao::addDoctorInfo(newId, intro, spec);
        }

        response[JsonKeys::CODE] = 200;
        response[JsonKeys::MSG] = "添加用户成功";
    } else {
        response[JsonKeys::CODE] = 500;
        response[JsonKeys::MSG] = "数据库插入失败";
    }

    sender->sendJson(response);
}

void AdminHandler::handleDeleteUser(ClientSocket* sender, const QJsonObject& request) {
    if (!checkAdminPermission(sender)) {
        QJsonObject err;
        err[JsonKeys::CMD] = request[JsonKeys::CMD];
        err[JsonKeys::CODE] = 403;
        err[JsonKeys::MSG] = "无权操作";
        sender->sendJson(err);
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int targetId = data[JsonKeys::TARGET_ID].toInt();

    QJsonObject response;
    response[JsonKeys::CMD] = request[JsonKeys::CMD];

    if (targetId <= 0) {
        response[JsonKeys::CODE] = 400;
        response[JsonKeys::MSG] = "无效的用户ID";
        sender->sendJson(response);
        return;
    }

    // 该用户在线则强制踢下线
    ClientSocket* targetSocket = ServerApp::instance().getClient(targetId);
    if (targetSocket) {
        QJsonObject kickMsg;
        kickMsg[JsonKeys::CMD] = (int)CmdType::PUSH_NOTIFICATION;
        kickMsg[JsonKeys::MSG] = "管理员删除了您的账号，连接断开。";
        targetSocket->sendJson(kickMsg);
        // ServerApp::unregisterUser 会在 socket 断开时自动调用
        targetSocket->deleteLater();
    }

    // 删库
    if (UserDao::deleteUser(targetId)) {
        response[JsonKeys::CODE] = 200;
        response[JsonKeys::MSG] = "用户删除成功";
    } else {
        response[JsonKeys::CODE] = 500;
        response[JsonKeys::MSG] = "删除失败，ID可能不存在";
    }

    sender->sendJson(response);
}

void AdminHandler::handleUpdateUserInfo(ClientSocket* sender, const QJsonObject& request) {
    if (!checkAdminPermission(sender)) {
        QJsonObject err;
        err[JsonKeys::CMD] = request[JsonKeys::CMD];
        err[JsonKeys::CODE] = 403;
        err[JsonKeys::MSG] = "无权操作";
        sender->sendJson(err);
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int targetId = data[JsonKeys::TARGET_ID].toInt();
    QString realName = data[JsonKeys::REAL_NAME].toString();
    QString rawPass = data[JsonKeys::PASSWORD].toString();

    // 医生特有字段
    QString intro = data[JsonKeys::INTRO].toString();
    QString spec = data[JsonKeys::SPEC].toString();

    QJsonObject response;
    response[JsonKeys::CMD] = request[JsonKeys::CMD];

    // 密码 Hash
    QString passHash = "";
    if (!rawPass.isEmpty()) {
        passHash = QString(QCryptographicHash::hash(rawPass.toUtf8(), QCryptographicHash::Sha256).toHex());
    }

    // 更新基础表 (users)
    bool success = UserDao::updateBasicInfo(targetId, realName, passHash);

    // 是医生则更新详情表
    int targetRole = UserDao::getUserRole(targetId);
    if (success && targetRole == (int)UserRole::DOCTOR) {
        success &= UserDao::updateDoctorInfo(targetId, intro, spec);
    }

    if (success) {
        response[JsonKeys::CODE] = 200;
        response[JsonKeys::MSG] = "用户信息更新成功";

        // 通知目标用户
        ClientSocket* targetClient = ServerApp::instance().getClient(targetId);
        if (targetClient) {
            QJsonObject notify;
            notify[JsonKeys::CMD] = (int)CmdType::PUSH_NOTIFICATION;
            notify[JsonKeys::MSG] = "您的个人信息已被管理员修改，请刷新查看。";
            if (!passHash.isEmpty()) {
                notify[JsonKeys::MSG] = "您的密码已被管理员重置，请重新登录。";
                targetClient->deleteLater();
            }
            targetClient->sendJson(notify);
        }

    } else {
        response[JsonKeys::CODE] = 500;
        response[JsonKeys::MSG] = "更新失败，数据库错误或ID不存在";
    }

    sender->sendJson(response);
}

void AdminHandler::handleGetStatistics(ClientSocket* client, const QJsonObject& req) {
    // 获取 Socket 指针
    QPointer<ClientSocket> safeClient(client); 

    // 获取查询参数
    QJsonObject data = req[JsonKeys::DATA].toObject();
    QString statType = data["type"].toString();

    // 启动子线程
    QFuture<void> future = QtConcurrent::run(
        [safeClient, statType]() {
            // 获取独立数据库连接
            QString connName;
            QSqlDatabase db = DBManager::instance().openThreadConnection(connName);

            QJsonArray resultMap; // 存储查询结果

            QSqlQuery query(db);
            if (db.isOpen()) {
                // 根据类型进行查询
                if (statType == "consult_trend") {
                    // 统计每天/每月的咨询人数]
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
                            resultMap.append(item);
                        }
                    }
                }
            } else if (statType == "common_issues") {
                // 统计咨询记录中的 tag
                // TODO: 改为 string_to_array + unnest
                QString sql = R"(
                    SELECT result_tags, COUNT(*)
                    FROM consultation_records
                    GROUP BY result_tags
                    ORDER BY count DESC LIMIT 10
                )";
                if (query.exec(sql)) {
                    while(query.next()) {
                        QJsonObject item;
                        item["label"] = query.value(0).toString(); // tag
                        item["value"] = query.value(1).toInt();
                        resultMap.append(item);
                    }
                }
            } // else 其他类型查询

            // 关闭独立连接
            DBManager::instance().closeThreadConnection(connName);

            // 将结果发送回主线程
            if (safeClient) {
                // 构建回包
                QJsonObject response;
                response[JsonKeys::CMD] = (int)CmdType::GET_STATISTICS;
                response[JsonKeys::CODE] = 200;
                response[JsonKeys::DATA] = resultMap;

                // 回到创建 client 的线程执行 sendJson
                QMetaObject::invokeMethod(
                    safeClient, [safeClient, response]() {
                        safeClient->sendJson(response);
                    }
                );
            }
        }
    );
    // 防止 future 析构时阻塞
    (void)future;
}

void AdminHandler::handleGetUserList(ClientSocket* sender, const QJsonObject& request) {
    if (!checkAdminPermission(sender)) {
        QJsonObject err;
        err[JsonKeys::CMD] = request[JsonKeys::CMD];
        err[JsonKeys::CODE] = 403;
        err[JsonKeys::MSG] = "无权操作";
        sender->sendJson(err);
        return;
    }

    QJsonArray userList = UserDao::getAllUsers(sender->userId());

    QJsonObject response;
    response[JsonKeys::CMD] = request[JsonKeys::CMD];
    response[JsonKeys::CODE] = 200;
    response[JsonKeys::DATA] = userList;

    sender->sendJson(response);
}
