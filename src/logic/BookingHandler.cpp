#include "BookingHandler.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonArray>
#include <QDebug>
#include <QDate>

#include "core/AsyncExecutor.h"
#include "dao/DBManager.h"
#include "dao/AppointmentDao.h"
#include "dao/UserDao.h"
#include "core/ProtocolDefs.h"
#include "core/ServerApp.h"
#include "network/ClientSocket.h"

void BookingHandler::handleCreateBooking(ClientSocket* sender, const QJsonObject& request)
{
    // 基础校验
    int userId = sender->userId();
    if (userId == -1) {
        sendErrorResponse(sender, request, StatusCode::UNAUTHORIZED, "用户未登录");
        return;
    }

    if (!request.contains(JsonKeys::DATA)) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "缺少数据字段");
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();

    // 校验参数存在性
    if (!data.contains(JsonKeys::DOCTOR_ID) || !data.contains("date") || !data.contains("timeSlot")) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "缺少必需参数");
        return;
    }

    int doctorId = data[JsonKeys::DOCTOR_ID].toInt();
    QString dateStr = data["date"].toString();
    int timeSlot = data["timeSlot"].toInt();

    // 校验参数逻辑合法性
    if (doctorId <= 0 || dateStr.isEmpty() || timeSlot < 0 || timeSlot > MAX_TIME_SLOT_INDEX) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "参数格式错误或时间段无效");
        return;
    }

    // 校验日期格式
    QDate qDate = QDate::fromString(dateStr, Qt::ISODate);
    if (!qDate.isValid()) qDate = QDate::fromString(dateStr, "yyyy/MM/dd");
    if (!qDate.isValid()) qDate = QDate::fromString(dateStr, "yyyy-M-d");

    if (!qDate.isValid()) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "日期格式无效，请使用 YYYY-MM-DD 格式");
        return;
    }

    if (qDate < QDate::currentDate()) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "不能预约过去的日期");
        return;
    }

    // 转换为标准字符串
    QString formattedDate = qDate.toString(Qt::ISODate);

    AsyncExecutor::run(sender,
        [userId, doctorId, formattedDate, timeSlot](QSqlDatabase db) -> QPair<bool, QString> {
            
            // 检查角色
            int role = UserDao::getUserRole(db, userId);
            if (role != (int)UserRole::STUDENT) {
                return {false, "只有学生可以发起预约"};
            }

            AppointmentDao dao;
            QString errorMsg;
            // 执行预约
            bool success = dao.createAppointment(db, userId, doctorId, formattedDate, timeSlot, errorMsg);
            
            if (success) {
                // TODO：推送给医生
            }
            
            return {success, success ? "预约成功！请按时咨询" : errorMsg};
        },
        
        // 主线程回调
        [sender, request, doctorId](QPair<bool, QString> result) {
            if (result.first) {
                sendSuccessResponse(sender, request, result.second);
                
                // 推送通知给医生
                ClientSocket* docSocket = ServerApp::instance().getClient(doctorId);
                if (docSocket) {
                    QJsonObject notify;
                    notify[JsonKeys::CMD] = (int)CmdType::PUSH_NOTIFICATION;
                    notify[JsonKeys::CODE] = (int)StatusCode::SUCCESS;
                    notify[JsonKeys::MSG] = "您有一个新的预约";
                    notify["action"] = "refresh_appointments"; 
                    docSocket->sendJson(notify);
                }
            } else {
                sendErrorResponse(sender, request, StatusCode::CONFLICT, result.second);
            }
        }
    );
}

void BookingHandler::handleDeleteBooking(ClientSocket* sender, const QJsonObject& request)
{
    int userId = sender->userId();
    QJsonObject data = request[JsonKeys::DATA].toObject();
    int apptId = data[JsonKeys::APPOINTMENT_ID].toInt();

    AsyncExecutor::run(sender,
        [userId, apptId](QSqlDatabase db) -> QPair<bool, QString> {
            AppointmentDao dao;
            QString errorMsg;
            bool success = dao.deleteCancelledAppointment(db, apptId, userId, errorMsg);
            return {success, success ? "记录已删除" : errorMsg};
        },
        [sender, request](QPair<bool, QString> result) {
            if (result.first) sendSuccessResponse(sender, request, result.second);
            else sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, result.second);
        }
    );
}

void BookingHandler::handleCancelBooking(ClientSocket* sender, const QJsonObject& request)
{
    // 基础校验
    int userId = sender->userId();
    if (userId == -1) {
        sendErrorResponse(sender, request, StatusCode::UNAUTHORIZED, "用户未登录");
        return;
    }

    if (!request.contains(JsonKeys::DATA)) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "缺少数据字段");
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();
    if (!data.contains(JsonKeys::APPOINTMENT_ID)) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "缺少预约ID");
        return;
    }

    int appointmentId = data[JsonKeys::APPOINTMENT_ID].toInt();
    if (appointmentId <= 0) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "预约ID无效");
        return;
    }

    AsyncExecutor::run(sender,
        [appointmentId](QSqlDatabase db) -> QPair<bool, QString> {
            AppointmentDao dao;
            QString errorMsg;
            bool success = dao.cancelAppointment(db, appointmentId, errorMsg);
            return {success, success ? "预约取消成功" : errorMsg};
        },

        [sender, request](QPair<bool, QString> result) {
            if (result.first) {
                sendSuccessResponse(sender, request, result.second);
            } else {
                sendErrorResponse(sender, request, StatusCode::INTERNAL_ERROR, result.second);
            }
        }
    );
}

void BookingHandler::handleGetMyBookings(ClientSocket* sender, const QJsonObject& request)
{
    // 基础校验
    int userId = sender->userId();
    if (userId == -1) {
        sendErrorResponse(sender, request, StatusCode::UNAUTHORIZED, "用户未登录");
        return;
    }

    AsyncExecutor::run(sender,
        [userId](QSqlDatabase db) -> QJsonArray {
            AppointmentDao dao;
            QString errorMsg;
            QJsonArray list = dao.getStudentAppointments(db, userId, errorMsg);
            
            if (!errorMsg.isEmpty()) {
                qWarning() << "查询预约失败:" << errorMsg;
            }
            return list;
        },

        [sender, request](QJsonArray appointments) {
            QJsonObject response;
            response[JsonKeys::CMD] = request[JsonKeys::CMD];
            response[JsonKeys::CODE] = StatusCode::SUCCESS;
            response[JsonKeys::MSG] = "获取预约列表成功";
            response[JsonKeys::DATA] = appointments;
            sender->sendJson(response);
        }
    );
}

void BookingHandler::handleModifyBookingDirect(ClientSocket* sender, const QJsonObject& request)
{
    // 基础校验
    int userId = sender->userId();
    if (userId == -1) {
        sendErrorResponse(sender, request, StatusCode::UNAUTHORIZED, "用户未登录");
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int apptId = data[JsonKeys::APPOINTMENT_ID].toInt();
    QString newDate = data["date"].toString();
    int newSlot = data["timeSlot"].toInt();

    // 参数检查
    if (apptId <= 0 || newDate.isEmpty() || newSlot < 0) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "参数错误");
        return;
    }

    AsyncExecutor::run(sender,
        [userId, apptId, newDate, newSlot](QSqlDatabase db) -> QPair<bool, QString> {
            // 校验角色
            if (UserDao::getUserRole(db, userId) != (int)UserRole::STUDENT) {
                return {false, "只有学生可以修改预约"};
            }

            AppointmentDao dao;
            QString errorMsg;
            bool success = dao.modifyAppointmentDirect(db, userId, apptId, newDate, newSlot, errorMsg);
            
            QString notificationMsg;
            int doctorId = 0;
            if (success) {
                 QSqlQuery q(db);
                 q.prepare("SELECT doctor_id FROM appointments WHERE id = ?");
                 q.addBindValue(apptId);
                 if(q.exec() && q.next()) doctorId = q.value(0).toInt();
                 
                 notificationMsg = QString("学生修改了预约 (ID: %1) 到 %2 %3, 请确认。").arg(apptId).arg(newDate).arg(GetTimeSlotText(newSlot));
            }

            // 将 doctorId 编码进 msg 字符串前缀
            if (success && doctorId > 0) {
                return {true, QString::number(doctorId) + ":" + notificationMsg};
            }

            return {success, errorMsg};
        },
        
        // 主线程回调
        [sender, request](QPair<bool, QString> result) {
            if (result.first) {
                // 解析返回字符串 "doctorId:Message"
                int sepIdx = result.second.indexOf(':');
                int doctorId = result.second.left(sepIdx).toInt();
                QString realMsg = result.second.mid(sepIdx + 1);

                sendSuccessResponse(sender, request, "预约时间修改成功，请等待医生确认");

                // 推送给医生
                ClientSocket* docSocket = ServerApp::instance().getClient(doctorId);
                if (docSocket) {
                    QJsonObject notify;
                    notify[JsonKeys::CMD] = (int)CmdType::PUSH_NOTIFICATION;
                    notify[JsonKeys::CODE] = (int)StatusCode::SUCCESS;
                    notify[JsonKeys::MSG] = realMsg;
                    notify["action"] = "refresh_appointments"; // 通知医生客户端刷新列表
                    docSocket->sendJson(notify);
                }
            } else {
                sendErrorResponse(sender, request, StatusCode::CONFLICT, result.second);
            }
        }
    );
}

void BookingHandler::handleModifyRequest(ClientSocket* sender, const QJsonObject& request)
{
    int doctorId = sender->userId();
    QJsonObject data = request[JsonKeys::DATA].toObject();
    int apptId = data[JsonKeys::APPOINTMENT_ID].toInt();
    QString newDate = data["date"].toString();
    int newSlot = data["timeSlot"].toInt();

    if (apptId <= 0 || newDate.isEmpty()) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "参数错误");
        return;
    }

    AsyncExecutor::run(sender,
        [doctorId, apptId, newDate, newSlot](QSqlDatabase db) -> QPair<bool, QString> {
            if (UserDao::getUserRole(db, doctorId) != (int)UserRole::DOCTOR) {
                return {false, "只有医生可以发起协商修改"};
            }

            AppointmentDao dao;
            QString errorMsg;
            bool ok = dao.createModifyRequest(db, apptId, doctorId, newDate, newSlot, errorMsg);
            
            // 获取学生ID用于推送
            int studentId = 0;
            if (ok) {
                QSqlQuery q(db);
                q.prepare("SELECT student_id FROM appointments WHERE id = ?");
                q.addBindValue(apptId);
                if (q.exec() && q.next()) studentId = q.value(0).toInt();
                return {true, QString::number(studentId) + ":已发送修改请求给学生"};
            }
            return {false, errorMsg};
        },
        [sender, request](QPair<bool, QString> res) {
            if (res.first) {
                int sep = res.second.indexOf(':');
                int stuId = res.second.left(sep).toInt();
                
                sendSuccessResponse(sender, request, "请求已发送");
                
                // 推送给学生
                ClientSocket* stuSock = ServerApp::instance().getClient(stuId);
                if (stuSock) {
                    QJsonObject notify;
                    notify[JsonKeys::CMD] = (int)CmdType::PUSH_NOTIFICATION;
                    notify[JsonKeys::CODE] = (int)StatusCode::NEGOTIATION_REQUIRED; // 特殊状态码
                    notify[JsonKeys::MSG] = "医生请求修改预约时间，请在“我的预约”中查看";
                    notify["action"] = "refresh_schedule";
                    stuSock->sendJson(notify);
                }
            } else {
                sendErrorResponse(sender, request, StatusCode::CONFLICT, res.second);
            }
        }
    );
}

void BookingHandler::handleModifyReply(ClientSocket* sender, const QJsonObject& request)
{
    int studentId = sender->userId();
    QJsonObject data = request[JsonKeys::DATA].toObject();
    int apptId = data[JsonKeys::APPOINTMENT_ID].toInt();
    bool accept = data["accept"].toBool();

    AsyncExecutor::run(sender,
        [studentId, apptId, accept](QSqlDatabase db) -> QPair<bool, QString> {
            AppointmentDao dao;
            QString errorMsg;
            bool ok = dao.resolveModifyRequest(db, apptId, studentId, accept, errorMsg);
            
            // 获取医生ID用于推送
            int doctorId = 0;
            if (ok) {
                QSqlQuery q(db);
                q.prepare("SELECT doctor_id FROM appointments WHERE id = ?");
                q.addBindValue(apptId);
                if (q.exec() && q.next()) doctorId = q.value(0).toInt();
                
                QString msg = accept ? "学生已同意修改时间" : "学生拒绝了修改请求，保持原时间";
                return {true, QString::number(doctorId) + ":" + msg};
            }
            return {false, errorMsg};
        },
        [sender, request](QPair<bool, QString> res) {
            if (res.first) {
                int sep = res.second.indexOf(':');
                int docId = res.second.left(sep).toInt();
                QString msg = res.second.mid(sep+1);

                sendSuccessResponse(sender, request, "操作成功");
                
                // 推送给医生
                ClientSocket* docSock = ServerApp::instance().getClient(docId);
                if (docSock) {
                    QJsonObject notify;
                    notify[JsonKeys::CMD] = (int)CmdType::PUSH_NOTIFICATION;
                    notify[JsonKeys::CODE] = (int)StatusCode::SUCCESS;
                    notify[JsonKeys::MSG] = msg;
                    notify["action"] = "refresh_appointments";
                    docSock->sendJson(notify);
                }
            } else {
                sendErrorResponse(sender, request, StatusCode::INTERNAL_ERROR, res.second);
            }
        }
    );
}

void BookingHandler::sendSuccessResponse(
    ClientSocket* sender, const QJsonObject& request, const QString& message)
{
    QJsonObject response;
    response[JsonKeys::CMD] = request[JsonKeys::CMD];
    response[JsonKeys::CODE] = StatusCode::SUCCESS;
    response[JsonKeys::MSG] = message;

    sender->sendJson(response);
}

void BookingHandler::sendErrorResponse(
    ClientSocket* sender, const QJsonObject& request, StatusCode code, const QString& message)
{
    QJsonObject response;
    response[JsonKeys::CMD] = request[JsonKeys::CMD];
    response[JsonKeys::CODE] = code;
    response[JsonKeys::MSG] = message;

    sender->sendJson(response);
}