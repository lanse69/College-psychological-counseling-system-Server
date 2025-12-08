#include "DoctorHandler.h"

#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "dao/UserDao.h"
#include "dao/AppointmentDao.h"
#include "network/ClientSocket.h"
#include "core/ProtocolDefs.h"

void DoctorHandler::handleGetDoctorList(
    ClientSocket* sender, const QJsonObject& request)
{
    QJsonArray doctorList = UserDao::getDoctorList();

    sendSuccessResponse(sender, (int) CmdType::GET_DOCTOR_LIST, doctorList, "获取医生列表成功");
}

void DoctorHandler::handleGetDoctorDetail(ClientSocket* sender, const QJsonObject& request)
{
    QJsonObject data = request[JsonKeys::DATA].toObject();
    int doctorId = data[JsonKeys::DOCTOR_ID].toInt();

    if (doctorId <= 0) {
        sendErrorResponse(sender,
                          (int) CmdType::GET_DOCTOR_DETAIL,
                          (int) StatusCode::BAD_REQUEST,
                          "医生ID无效");
        return;
    }

    // 检查医生是否存在
    if (!UserDao::isDoctorExist(doctorId)) {
        sendErrorResponse(sender,
                          (int) CmdType::GET_DOCTOR_DETAIL,
                          (int) StatusCode::NOT_FOUND,
                          "医生不存在");
        return;
    }

    QJsonObject doctorDetail = UserDao::getDoctorDetail(doctorId);

    if (doctorDetail.contains("id")) {
        sendSuccessResponse(sender,
                            (int) CmdType::GET_DOCTOR_DETAIL,
                            doctorDetail,
                            "获取医生详情成功");
    } else {
        sendErrorResponse(sender,
                          (int) CmdType::GET_DOCTOR_DETAIL,
                          (int) StatusCode::INTERNAL_ERROR,
                          "获取医生详情失败");
    }
}

void DoctorHandler::handleStudentGetDoctorList(ClientSocket* sender, const QJsonObject& request)
{
    QJsonArray doctorList = UserDao::getDoctorList();

    sendSuccessResponse(sender, (int) CmdType::GET_DOCTOR_LIST, doctorList, "获取医生列表成功");
}

void DoctorHandler::handleGetAppointments(
    ClientSocket* sender, const QJsonObject& request)
{
    if (!sender) {
        return;
    }

    int doctorId = sender->userId();

    if (doctorId <= 0) {
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_GET_APPOINTMENTS,
                          (int) StatusCode::UNAUTHORIZED,
                          "用户未登录");
        return;
    }

    AppointmentDao dao;
    QString errorMsg;
    QJsonArray appointments = dao.getDoctorAppointments(doctorId, errorMsg);

    if (!errorMsg.isEmpty()) {
        qWarning() << "医生获取预约失败 ID:" << doctorId << " 错误:" << errorMsg;
        sendErrorResponse(sender, (int)CmdType::DOCTOR_GET_APPOINTMENTS, 
                          (int)StatusCode::INTERNAL_ERROR, "获取预约列表失败");
        return;
    }

    sendSuccessResponse(sender,
                        (int) CmdType::DOCTOR_GET_APPOINTMENTS,
                        appointments,
                        "获取预约列表成功");
}

void DoctorHandler::handleConfirmAppointment(ClientSocket* sender, const QJsonObject& request)
{
    if (!sender) {
        qCritical() << "错误: sender为空指针";
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_CONFIRM_APPOINTMENT,
                          (int) StatusCode::BAD_REQUEST,
                          "无效的客户端连接");
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int appointmentId = data[JsonKeys::APPOINTMENT_ID].toInt();

    if (appointmentId <= 0) {
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_CONFIRM_APPOINTMENT,
                          (int) StatusCode::BAD_REQUEST,
                          "预约ID无效");
        return;
    }

    int doctorId = sender->userId();

    if (doctorId <= 0) {
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_CONFIRM_APPOINTMENT,
                          (int) StatusCode::UNAUTHORIZED,
                          "用户未登录");
        return;
    }

    AppointmentDao dao;
    QString errorMsg;
    bool success = dao.confirmAppointment(appointmentId, doctorId, errorMsg);

    if (success) {
        qDebug() << "预约确认成功";
        sendSuccessResponse(sender,
                            (int) CmdType::DOCTOR_CONFIRM_APPOINTMENT,
                            QJsonValue::Null,
                            "预约确认成功");
    } else {
        qWarning() << "预约确认失败:" << errorMsg;
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_CONFIRM_APPOINTMENT,
                          (int) StatusCode::INTERNAL_ERROR,
                          errorMsg);
    }
}

void DoctorHandler::sendSuccessResponse(
    ClientSocket* sender, int cmd, const QJsonValue& data, const QString& msg)
{
    QJsonObject response;
    response[JsonKeys::CMD] = cmd;
    response[JsonKeys::CODE] = (int) StatusCode::SUCCESS;
    response[JsonKeys::MSG] = msg;
    response[JsonKeys::DATA] = data;

    if (sender) {
        sender->sendJson(response);
        qDebug() << "CMD:" << cmd << " 响应成功";
    }
}

void DoctorHandler::sendErrorResponse(ClientSocket* sender, int cmd, int code, const QString& msg)
{
    QJsonObject response;
    response[JsonKeys::CMD] = cmd;
    response[JsonKeys::CODE] = code;
    response[JsonKeys::MSG] = msg;

    if (sender) {
        sender->sendJson(response);
        qDebug() << "CMD:" << cmd << " 响应失败";
    }
}

void DoctorHandler::handleGetPatients(ClientSocket* sender, const QJsonObject& request)
{
    // 安全检查
    if (!sender) {
        qCritical() << "错误: sender为空指针";
        return;
    }

    int doctorId = sender->userId();
    if (doctorId <= 0) {
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_GET_PATIENTS,
                          (int) StatusCode::UNAUTHORIZED,
                          "用户未登录");
        return;
    }

    AppointmentDao dao;
    QString errorMsg;
    
    QJsonArray patients = dao.getDoctorPatients(doctorId, errorMsg);

    if (!errorMsg.isEmpty()) {
        qWarning() << "获取患者列表失败:" << errorMsg;
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_GET_PATIENTS,
                          (int) StatusCode::INTERNAL_ERROR,
                          errorMsg);
        return;
    }

    sendSuccessResponse(sender,
                        (int) CmdType::DOCTOR_GET_PATIENTS,
                        patients,
                        "获取患者列表成功");
}

void DoctorHandler::handleGetPatientHistory(ClientSocket* sender, const QJsonObject& request)
{
    if (!sender) return;

    int doctorId = sender->userId();

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int studentId = data["studentId"].toInt();

    if (studentId <= 0) {
        sendErrorResponse(sender, 
                          (int)CmdType::DOCTOR_GET_PATIENT_HISTORY, 
                          (int)StatusCode::BAD_REQUEST, 
                          "学生ID无效");
        return;
    }

    AppointmentDao dao;
    QString errorMsg;
    QJsonArray history = dao.getHistoryByDoctorAndStudent(doctorId, studentId, errorMsg);

    if (errorMsg.isEmpty()) {
        sendSuccessResponse(sender, 
                            (int)CmdType::DOCTOR_GET_PATIENT_HISTORY, 
                            history, 
                            "获取历史记录成功");
    } else {
        sendErrorResponse(sender, 
                          (int)CmdType::DOCTOR_GET_PATIENT_HISTORY, 
                          (int)StatusCode::INTERNAL_ERROR, 
                          errorMsg);
    }
}

void DoctorHandler::handleRejectAppointment(
    ClientSocket* sender, const QJsonObject& request)
{
    if (!sender) {
        qCritical() << "错误: sender为空指针";
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_REJECT_APPOINTMENT,
                          (int) StatusCode::BAD_REQUEST,
                          "无效的客户端连接");
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int appointmentId = data[JsonKeys::APPOINTMENT_ID].toInt();

    if (appointmentId <= 0) {
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_REJECT_APPOINTMENT,
                          (int) StatusCode::BAD_REQUEST,
                          "预约ID无效");
        return;
    }

    int doctorId = sender->userId();

    if (doctorId <= 0) {
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_REJECT_APPOINTMENT,
                          (int) StatusCode::UNAUTHORIZED,
                          "用户未登录");
        return;
    }

    AppointmentDao dao;
    QString errorMsg;
    bool success = dao.rejectAppointment(appointmentId, doctorId, errorMsg);

    if (success) {
        qDebug() << "预约拒绝成功";
        sendSuccessResponse(sender,
                            (int) CmdType::DOCTOR_REJECT_APPOINTMENT,
                            QJsonValue::Null,
                            "预约拒绝成功");
    } else {
        qWarning() << "预约拒绝失败:" << errorMsg;
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_REJECT_APPOINTMENT,
                          (int) StatusCode::INTERNAL_ERROR,
                          errorMsg);
    }
}

void DoctorHandler::handleSubmitReport(
    ClientSocket* sender, const QJsonObject& request)
{
    if (!sender) {
        qCritical() << "错误: sender为空指针";
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_SUBMIT_REPORT,
                          (int) StatusCode::BAD_REQUEST,
                          "无效的客户端连接");
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int appointmentId = data[JsonKeys::APPOINTMENT_ID].toInt();

    if (appointmentId <= 0) {
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_SUBMIT_REPORT,
                          (int) StatusCode::BAD_REQUEST,
                          "预约ID无效");
        return;
    }

    int doctorId = sender->userId();
    qDebug() << "医生提交报告 - 医生ID:" << doctorId << "预约ID:" << appointmentId;

    if (doctorId <= 0) {
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_SUBMIT_REPORT,
                          (int) StatusCode::UNAUTHORIZED,
                          "用户未登录");
        return;
    }

    // TODO: 这里应该保存报告内容到数据库
    // 现在暂时只是成功响应，需要存储报告内容
    qDebug() << "报告提交成功（暂时只记录日志）";

    sendSuccessResponse(sender,
                        (int) CmdType::DOCTOR_SUBMIT_REPORT,
                        QJsonValue::Null,
                        "报告提交成功");
}

void DoctorHandler::handleCompleteConsultation(
    ClientSocket* sender, const QJsonObject& request)
{
    if (!sender) {
        qCritical() << "错误: sender为空指针";
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_COMPLETE_CONSULTATION,
                          (int) StatusCode::BAD_REQUEST,
                          "无效的客户端连接");
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int appointmentId = data[JsonKeys::APPOINTMENT_ID].toInt();

    if (appointmentId <= 0) {
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_COMPLETE_CONSULTATION,
                          (int) StatusCode::BAD_REQUEST,
                          "预约ID无效");
        return;
    }

    int doctorId = sender->userId();

    if (doctorId <= 0) {
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_COMPLETE_CONSULTATION,
                          (int) StatusCode::UNAUTHORIZED,
                          "用户未登录");
        return;
    }

    AppointmentDao dao;
    QString errorMsg;
    bool success = dao.completeConsultation(appointmentId, doctorId, errorMsg);

    if (success) {
        qDebug() << "咨询完成状态更新成功";
        sendSuccessResponse(sender,
                            (int) CmdType::DOCTOR_COMPLETE_CONSULTATION,
                            QJsonValue::Null,
                            "咨询完成状态更新成功");
    } else {
        qWarning() << "咨询完成状态更新失败:" << errorMsg;
        sendErrorResponse(sender,
                          (int) CmdType::DOCTOR_COMPLETE_CONSULTATION,
                          (int) StatusCode::INTERNAL_ERROR,
                          errorMsg);
    }
}
