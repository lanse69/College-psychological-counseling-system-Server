#include "BookingHandler.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonArray>
#include <QDebug>
#include <QDate>

#include "dao/DBManager.h"
#include "dao/AppointmentDao.h"
#include "dao/ScheduleDao.h"
#include "core/ProtocolDefs.h"
#include "core/ServerApp.h"
#include "network/ClientSocket.h"
#include "dao/UserDao.h"

void BookingHandler::handleCreateBooking(ClientSocket* sender, const QJsonObject& request)
{
    // 检查用户是否登录
    int userId = sender->userId();
    if (userId == -1) {
        sendErrorResponse(sender, request, StatusCode::UNAUTHORIZED, "用户未登录");
        return;
    }

    int role = UserDao::getUserRole(userId);
    if (role != (int)UserRole::STUDENT) {
        sendErrorResponse(sender, request, StatusCode::FORBIDDEN, "只有学生可以发起预约");
        return;
    }

    // 解析请求数据
    if (!request.contains(JsonKeys::DATA)) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "缺少数据字段");
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();

    // 验证必需参数
    if (!data.contains(JsonKeys::DOCTOR_ID) || !data.contains("date")
        || !data.contains("timeSlot")) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "缺少必需参数");
        return;
    }

    int doctorId = data[JsonKeys::DOCTOR_ID].toInt();
    QString dateStr = data["date"].toString();
    int timeSlot = data["timeSlot"].toInt();

    // 参数验证
    if (doctorId <= 0 || dateStr.isEmpty() || 
        timeSlot < 0 || timeSlot > MAX_TIME_SLOT_INDEX) {
        
        QString errInfo = QString("参数错误: 医生ID(%1), 日期(%2), 时段索引(%3)")
                          .arg(doctorId).arg(dateStr).arg(timeSlot);
        qWarning() << "非法预约请求:" << errInfo;
        
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "请求参数错误或时段无效");
        return;
    }
    
    // 尝试使用标准 ISO 格式 (YYYY-MM-DD) 解析
    QDate qDate = QDate::fromString(dateStr, Qt::ISODate);

    // 标准解析失败，尝试解析常见的替代格式
    if (!qDate.isValid()) {
        qDate = QDate::fromString(dateStr, "yyyy/MM/dd");
    }

    // 校验日期有效性
    if (!qDate.isValid()) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "日期格式无效，请使用 YYYY-MM-DD 格式");
        return;
    }

    // 校验是否是过去的时间
    if (qDate < QDate::currentDate()) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "不能预约过去的日期");
        return;
    }

    // 统一转换为标准字符串存入数据库
    QString formattedDate = qDate.toString(Qt::ISODate);

    ScheduleDao scheduleDao;
    if (!scheduleDao.isSlotAvailable(doctorId, QDate::fromString(formattedDate, Qt::ISODate), timeSlot)) {
         sendErrorResponse(sender, request, StatusCode::CONFLICT, "医生该时段已设置为忙碌/休息");
         return;
    }

    // 创建预约
    AppointmentDao dao;
    QString errorMsg;
    bool success = dao.createAppointment(userId, doctorId, formattedDate, timeSlot, errorMsg);

    if (success) {
        sendSuccessResponse(sender, request, "预约创建成功");
    } else {
        sendErrorResponse(sender, request, StatusCode::CONFLICT, errorMsg);
    }
}

// 学生修改预约：校验医生时间表
void BookingHandler::handleModifyBookingDirect(ClientSocket* sender, const QJsonObject& request)
{
    // 暂时返回未实现
    sendErrorResponse(sender, request, StatusCode::NOT_IMPLEMENTED, "功能尚未实现");
}

// 医生发起修改请求：需学生同意
void BookingHandler::handleModifyRequest(ClientSocket* sender, const QJsonObject& request)
{
    // 暂时返回未实现
    sendErrorResponse(sender, request, StatusCode::NOT_IMPLEMENTED, "功能尚未实现");
}

// 学生回复修改请求
void BookingHandler::handleModifyReply(ClientSocket* sender, const QJsonObject& request)
{
    // 暂时返回未实现
    sendErrorResponse(sender, request, StatusCode::NOT_IMPLEMENTED, "功能尚未实现");
}

void BookingHandler::handleCancelBooking(ClientSocket* sender, const QJsonObject& request)
{
    // 检查用户是否登录
    int userId = sender->userId();
    if (userId == -1) {
        sendErrorResponse(sender, request, StatusCode::UNAUTHORIZED, "用户未登录");
        return;
    }

    // 解析请求数据
    if (!request.contains(JsonKeys::DATA)) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "缺少数据字段");
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();

    // 验证必需参数
    if (!data.contains(JsonKeys::APPOINTMENT_ID)) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "缺少预约ID");
        return;
    }

    int appointmentId = data[JsonKeys::APPOINTMENT_ID].toInt();

    if (appointmentId <= 0) {
        sendErrorResponse(sender, request, StatusCode::BAD_REQUEST, "预约ID无效");
        return;
    }

    // 取消预约
    AppointmentDao dao;
    QString errorMsg;
    bool success = dao.cancelAppointment(appointmentId, errorMsg);

    if (success) {
        sendSuccessResponse(sender, request, "预约取消成功");
        qDebug() << "预约取消成功";
    } else {
        sendErrorResponse(sender, request, StatusCode::INTERNAL_ERROR, errorMsg);
        qDebug() << "预约取消失败:" << errorMsg;
    }
}

void BookingHandler::handleGetMyBookings(
    ClientSocket* sender, const QJsonObject& request)
{
    // 检查用户是否登录
    int userId = sender->userId();

    if (userId == -1) {
        qWarning() << "警告: 用户未登录";
        sendErrorResponse(sender, request, StatusCode::UNAUTHORIZED, "用户未登录");
        return;
    }

    // 获取预约列表
    AppointmentDao dao;
    QString errorMsg;
    QJsonArray appointments = dao.getStudentAppointments(userId, errorMsg);
    qDebug() << "学生预约数据库查询完成，错误信息:" << errorMsg;

    if (!errorMsg.isEmpty()) {
        qWarning() << "学生预约数据库查询失败:" << errorMsg;
        sendErrorResponse(sender, request, StatusCode::INTERNAL_ERROR, errorMsg);
        qDebug() << "获取预约列表失败:" << errorMsg;
        return;
    }

    // 发送成功响应和数据
    sendSuccessResponse(sender, request, "获取预约列表成功");
    QJsonObject response;
    response[JsonKeys::CMD] = request[JsonKeys::CMD];
    response[JsonKeys::CODE] = StatusCode::SUCCESS;
    response[JsonKeys::MSG] = "获取预约列表成功";
    response[JsonKeys::DATA] = appointments;
    sender->sendJson(response);
}

// 发送成功响应
void BookingHandler::sendSuccessResponse(
    ClientSocket* sender, const QJsonObject& request, const QString& message)
{
    QJsonObject response;
    response[JsonKeys::CMD] = request[JsonKeys::CMD];
    response[JsonKeys::CODE] = StatusCode::SUCCESS;
    response[JsonKeys::MSG] = message;

    sender->sendJson(response);
}

// 发送错误响应
void BookingHandler::sendErrorResponse(
    ClientSocket* sender, const QJsonObject& request, StatusCode code, const QString& message)
{
    QJsonObject response;
    response[JsonKeys::CMD] = request[JsonKeys::CMD];
    response[JsonKeys::CODE] = code;
    response[JsonKeys::MSG] = message;

    sender->sendJson(response);
}
