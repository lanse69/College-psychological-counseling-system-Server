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
    sendErrorResponse(sender, request, StatusCode::NOT_IMPLEMENTED, "功能尚未实现");
}

void BookingHandler::handleModifyRequest(ClientSocket* sender, const QJsonObject& request)
{
    sendErrorResponse(sender, request, StatusCode::NOT_IMPLEMENTED, "功能尚未实现");
}

void BookingHandler::handleModifyReply(ClientSocket* sender, const QJsonObject& request)
{
    sendErrorResponse(sender, request, StatusCode::NOT_IMPLEMENTED, "功能尚未实现");
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