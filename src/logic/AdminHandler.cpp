#include "AdminHandler.h"

#include <QtConcurrent>
#include <QJsonArray>
#include <QPointer>
#include <QDebug>
#include <QCoreApplication>

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
        err[JsonKeys::CODE] = (int)StatusCode::FORBIDDEN;
        err[JsonKeys::MSG] = "无权操作";
        sender->sendJson(err);
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();
    QString username = data[JsonKeys::USERNAME].toString();
    QString passHash = data[JsonKeys::PASSWORD].toString();
    QString realName = data[JsonKeys::REAL_NAME].toString();
    int role = data[JsonKeys::ROLE].toInt();

    QJsonObject response;
    response[JsonKeys::CMD] = request[JsonKeys::CMD];

    // 检查参数
    if (username.isEmpty() || passHash.isEmpty()) {
        response[JsonKeys::CODE] = (int)StatusCode::BAD_REQUEST;
        response[JsonKeys::MSG] = "用户名或密码不能为空";
        sender->sendJson(response);
        return;
    }

    // 检查用户是否存在
    if (UserDao::isUsernameExist(username)) {
        response[JsonKeys::CODE] = (int)StatusCode::CONFLICT; // Conflict
        response[JsonKeys::MSG] = "用户名已存在";
        sender->sendJson(response);
        return;
    }

    // 执行插入
    int newId = UserDao::addUser(username, passHash, role, realName);

    if (newId != -1) {
        // 医生需要插入 doctor_info
        if (role == (int)UserRole::DOCTOR) {
            QString intro = data[JsonKeys::INTRO].toString();
            QString spec = data[JsonKeys::SPEC].toString();
            UserDao::addDoctorInfo(newId, intro, spec);
        }

        response[JsonKeys::CODE] = (int)StatusCode::SUCCESS;
        response[JsonKeys::MSG] = "添加用户成功";
    } else {
        response[JsonKeys::CODE] = (int)StatusCode::INTERNAL_ERROR;
        response[JsonKeys::MSG] = "数据库插入失败";
    }

    sender->sendJson(response);
}

void AdminHandler::handleDeleteUser(ClientSocket* sender, const QJsonObject& request) {
    if (!checkAdminPermission(sender)) {
        QJsonObject err;
        err[JsonKeys::CMD] = request[JsonKeys::CMD];
        err[JsonKeys::CODE] = (int)StatusCode::FORBIDDEN;
        err[JsonKeys::MSG] = "无权操作";
        sender->sendJson(err);
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int targetId = data[JsonKeys::TARGET_ID].toInt();

    QJsonObject response;
    response[JsonKeys::CMD] = request[JsonKeys::CMD];

    if (targetId <= 0) {
        response[JsonKeys::CODE] = (int)StatusCode::BAD_REQUEST;
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
        response[JsonKeys::CODE] = (int)StatusCode::SUCCESS;
        response[JsonKeys::MSG] = "用户删除成功";
    } else {
        response[JsonKeys::CODE] = (int)StatusCode::INTERNAL_ERROR;
        response[JsonKeys::MSG] = "删除失败，ID可能不存在";
    }

    sender->sendJson(response);
}

void AdminHandler::handleUpdateUserInfo(ClientSocket* sender, const QJsonObject& request) {
    if (!checkAdminPermission(sender)) {
        QJsonObject err;
        err[JsonKeys::CMD] = request[JsonKeys::CMD];
        err[JsonKeys::CODE] = (int)StatusCode::FORBIDDEN;
        err[JsonKeys::MSG] = "无权操作";
        sender->sendJson(err);
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int targetId = data[JsonKeys::TARGET_ID].toInt();
    QString realName = data[JsonKeys::REAL_NAME].toString();
    QString passHash = data[JsonKeys::PASSWORD].toString();

    // 医生特有字段
    QString intro = data[JsonKeys::INTRO].toString();
    QString spec = data[JsonKeys::SPEC].toString();

    QJsonObject response;
    response[JsonKeys::CMD] = request[JsonKeys::CMD];

    // 更新基础表 (users)
    bool success = UserDao::updateBasicInfo(targetId, realName, passHash);

    // 是医生则更新详情表
    int targetRole = UserDao::getUserRole(targetId);
    if (success && targetRole == (int)UserRole::DOCTOR) {
        success &= UserDao::updateDoctorInfo(targetId, intro, spec);
    }

    if (success) {
        response[JsonKeys::CODE] = (int)StatusCode::SUCCESS;
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
        response[JsonKeys::CODE] = (int)StatusCode::INTERNAL_ERROR;
        response[JsonKeys::MSG] = "更新失败，数据库错误或ID不存在";
    }

    sender->sendJson(response);
}

void AdminHandler::handleGetStatistics(ClientSocket* client, const QJsonObject& req) {
    // 使用 QPointer 弱引用 ClientSocket，防止悬空指针
    QPointer<ClientSocket> safeClient(client); 

    // 预先解析请求数据，避免在子线程中访问 QJsonObject 的隐式共享深拷贝问题
    QJsonObject data = req[JsonKeys::DATA].toObject();
    QString statType = data["type"].toString();
    int cmd = req[JsonKeys::CMD].toInt();

    // 启动子线程执行耗时数据库查询
    QFuture<void> ignoredFuture = QtConcurrent::run([safeClient, statType, cmd]() {
        // 早期检查：如果刚进子线程客户端就断了，直接退出节省资源
        if (!safeClient) return;

        // 获取独立数据库连接
        QString connName;
        QSqlDatabase db = DBManager::instance().openThreadConnection(connName);
        QJsonArray resultMap; 

        if (db.isOpen()) {
            QSqlQuery query(db);
            
            if (statType == "consult_trend") {
                // 统计最近12个月的咨询趋势
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
                } else {
                    qWarning() << "统计查询失败(consult_trend):" << query.lastError().text();
                }
            } else if (statType == "common_issues") {
                // 统计常见问题 Tag
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
                        resultMap.append(item);
                    }
                }
            } // TODO else
        }

        // 关闭独立连接
        DBManager::instance().closeThreadConnection(connName);

        // 准备回包数据
        QJsonObject response;
        response[JsonKeys::CMD] = cmd;
        response[JsonKeys::CODE] = (int)StatusCode::SUCCESS;
        response[JsonKeys::DATA] = resultMap;

        // 切换回主线程发送数据
        QMetaObject::invokeMethod(QCoreApplication::instance(), [safeClient, response]() {
            // 安全检查：发送前确认客户端还活着
            if (safeClient) {
                safeClient->sendJson(response);
                qDebug() << "统计报表发送成功，数据条数:" << response[JsonKeys::DATA].toArray().size();
            } else {
                qDebug() << "统计查询完成，但客户端已断开连接，数据丢弃。";
            }
        });
    });
}

void AdminHandler::handleGetUserList(ClientSocket* sender, const QJsonObject& request) {
    if (!checkAdminPermission(sender)) {
        QJsonObject err;
        err[JsonKeys::CMD] = request[JsonKeys::CMD];
        err[JsonKeys::CODE] = (int)StatusCode::FORBIDDEN;
        err[JsonKeys::MSG] = "无权操作";
        sender->sendJson(err);
        return;
    }

    QJsonArray userList = UserDao::getAllUsers(sender->userId());

    QJsonObject response;
    response[JsonKeys::CMD] = request[JsonKeys::CMD];
    response[JsonKeys::CODE] = (int)StatusCode::SUCCESS;
    response[JsonKeys::DATA] = userList;

    sender->sendJson(response);
}
