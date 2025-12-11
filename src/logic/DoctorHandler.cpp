#include "DoctorHandler.h"

#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QPair>

#include "dao/UserDao.h"
#include "dao/AppointmentDao.h"
#include "dao/ScheduleDao.h"
#include "network/ClientSocket.h"
#include "core/ProtocolDefs.h"
#include "core/AsyncExecutor.h"

struct OpResult {
    bool success;
    QString msg;
    QJsonValue data; // 可选的返回数据
};

void DoctorHandler::handleGetDoctorList(ClientSocket* sender, const QJsonObject& request)
{
    // 获取医生列表
    AsyncExecutor::run(sender,
        [](QSqlDatabase db) -> QJsonArray {
            return UserDao::getDoctorList(db);
        },
        // 主线程回调
        [sender](QJsonArray doctorList) {
            sendSuccessResponse(sender, (int)CmdType::GET_DOCTOR_LIST, doctorList, "获取医生列表成功");
        }
    );
}

void DoctorHandler::handleGetDoctorDetail(ClientSocket* sender, const QJsonObject& request)
{
    QJsonObject data = request[JsonKeys::DATA].toObject();
    int doctorId = data[JsonKeys::DOCTOR_ID].toInt();

    if (doctorId <= 0) {
        sendErrorResponse(sender, (int)CmdType::GET_DOCTOR_DETAIL, (int)StatusCode::BAD_REQUEST, "医生ID无效");
        return;
    }

    // 查询详情
    AsyncExecutor::run(sender,
        [doctorId](QSqlDatabase db) -> QJsonObject {
            // 检查医生是否存在
            if (!UserDao::isDoctorExist(db, doctorId)) {
                return QJsonObject(); // 返回空对象表示不存在
            }
            return UserDao::getDoctorDetail(db, doctorId);
        },
        [sender](QJsonObject detail) {
            if (!detail.isEmpty() && detail.contains("id")) {
                sendSuccessResponse(sender, (int)CmdType::GET_DOCTOR_DETAIL, detail, "获取医生详情成功");
            } else {
                // 如果返回空，说明不存在或查询失败
                sendErrorResponse(sender, (int)CmdType::GET_DOCTOR_DETAIL, (int)StatusCode::NOT_FOUND, "医生不存在或查询失败");
            }
        }
    );
}

void DoctorHandler::handleStudentGetDoctorList(ClientSocket* sender, const QJsonObject& request)
{
    // 获取医生列表
    AsyncExecutor::run(sender,
        [](QSqlDatabase db) -> QJsonArray {
            return UserDao::getDoctorList(db);
        },
        // 主线程回调
        [sender](QJsonArray doctorList) {
            sendSuccessResponse(sender, (int)CmdType::STUDENT_GET_DOCTOR_LIST, doctorList, "获取医生列表成功");
        }
    );
}

void DoctorHandler::handleGetAppointments(ClientSocket* sender, const QJsonObject& request)
{
    int doctorId = sender->userId();
    if (doctorId <= 0) {
        sendErrorResponse(sender, (int)CmdType::DOCTOR_GET_APPOINTMENTS, (int)StatusCode::UNAUTHORIZED, "用户未登录");
        return;
    }

    // 获取预约列表
    AsyncExecutor::run(sender,
        [doctorId](QSqlDatabase db) -> OpResult {
            AppointmentDao dao;
            QString errorMsg;
            QJsonArray list = dao.getDoctorAppointments(db, doctorId, errorMsg);
            
            if (!errorMsg.isEmpty()) {
                return {false, errorMsg, QJsonValue::Null};
            }
            return {true, "获取成功", list};
        },
        [sender](OpResult result) {
            if (result.success) {
                sendSuccessResponse(sender, (int)CmdType::DOCTOR_GET_APPOINTMENTS, result.data, result.msg);
            } else {
                sendErrorResponse(sender, (int)CmdType::DOCTOR_GET_APPOINTMENTS, (int)StatusCode::INTERNAL_ERROR, result.msg);
            }
        }
    );
}

void DoctorHandler::handleGetPatients(ClientSocket* sender, const QJsonObject& request)
{
    if (!sender) return;

    int doctorId = sender->userId();
    if (doctorId <= 0) {
        sendErrorResponse(sender, (int)CmdType::DOCTOR_GET_PATIENTS, (int)StatusCode::UNAUTHORIZED, "用户未登录");
        return;
    }

    // 获取预约学生列表
    AsyncExecutor::run(sender,
        [doctorId](QSqlDatabase db) -> OpResult {
            AppointmentDao dao;
            QString errorMsg;
            QJsonArray patients = dao.getDoctorPatients(db, doctorId, errorMsg);

            if (!errorMsg.isEmpty()) {
                return {false, errorMsg, QJsonValue::Null};
            }
            return {true, "获取成功", patients};
        },
        [sender](OpResult result) {
            if (result.success) {
                sendSuccessResponse(sender, (int)CmdType::DOCTOR_GET_PATIENTS, result.data, result.msg);
            } else {
                sendErrorResponse(sender, (int)CmdType::DOCTOR_GET_PATIENTS, (int)StatusCode::INTERNAL_ERROR, result.msg);
            }
        }
    );
}

void DoctorHandler::handleGetPatientHistory(ClientSocket* sender, const QJsonObject& request)
{
    if (!sender) return;
    int doctorId = sender->userId();

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int studentId = data["studentId"].toInt();

    if (studentId <= 0) {
        sendErrorResponse(sender, (int)CmdType::DOCTOR_GET_PATIENT_HISTORY, (int)StatusCode::BAD_REQUEST, "学生ID无效");
        return;
    }

    // 获取历史记录
    AsyncExecutor::run(sender,
        [doctorId, studentId](QSqlDatabase db) -> OpResult {
            AppointmentDao dao;
            QString errorMsg;
            QJsonArray history = dao.getHistoryByDoctorAndStudent(db, doctorId, studentId, errorMsg);

            if (!errorMsg.isEmpty()) {
                return {false, errorMsg, QJsonValue::Null};
            }
            return {true, "获取成功", history};
        },
        [sender](OpResult result) {
            if (result.success) {
                sendSuccessResponse(sender, (int)CmdType::DOCTOR_GET_PATIENT_HISTORY, result.data, result.msg);
            } else {
                sendErrorResponse(sender, (int)CmdType::DOCTOR_GET_PATIENT_HISTORY, (int)StatusCode::INTERNAL_ERROR, result.msg);
            }
        }
    );
}

void DoctorHandler::handleConfirmAppointment(ClientSocket* sender, const QJsonObject& request)
{
    if (!sender) return;
    
    QJsonObject data = request[JsonKeys::DATA].toObject();
    int appointmentId = data[JsonKeys::APPOINTMENT_ID].toInt();
    int doctorId = sender->userId();

    if (appointmentId <= 0 || doctorId <= 0) {
        sendErrorResponse(sender, (int)CmdType::DOCTOR_CONFIRM_APPOINTMENT, (int)StatusCode::BAD_REQUEST, "参数无效或未登录");
        return;
    }

    // 确认预约
    AsyncExecutor::run(sender,
        [appointmentId, doctorId](QSqlDatabase db) -> QPair<bool, QString> {
            AppointmentDao dao;
            QString errorMsg;
            bool success = dao.confirmAppointment(db, appointmentId, doctorId, errorMsg);
            return {success, success ? "预约确认成功" : errorMsg};
        },
        [sender](QPair<bool, QString> result) {
            if (result.first) {
                sendSuccessResponse(sender, (int)CmdType::DOCTOR_CONFIRM_APPOINTMENT, QJsonValue::Null, result.second);
            } else {
                sendErrorResponse(sender, (int)CmdType::DOCTOR_CONFIRM_APPOINTMENT, (int)StatusCode::INTERNAL_ERROR, result.second);
            }
        }
    );
}

void DoctorHandler::handleRejectAppointment(ClientSocket* sender, const QJsonObject& request)
{
    if (!sender) return;

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int appointmentId = data[JsonKeys::APPOINTMENT_ID].toInt();
    int doctorId = sender->userId();

    if (appointmentId <= 0 || doctorId <= 0) {
        sendErrorResponse(sender, (int)CmdType::DOCTOR_REJECT_APPOINTMENT, (int)StatusCode::BAD_REQUEST, "参数无效");
        return;
    }

    // 拒绝预约
    AsyncExecutor::run(sender,
        [appointmentId, doctorId](QSqlDatabase db) -> QPair<bool, QString> {
            AppointmentDao dao;
            QString errorMsg;
            bool success = dao.rejectAppointment(db, appointmentId, doctorId, errorMsg);
            return {success, success ? "预约拒绝成功" : errorMsg};
        },
        [sender](QPair<bool, QString> result) {
            if (result.first) {
                sendSuccessResponse(sender, (int)CmdType::DOCTOR_REJECT_APPOINTMENT, QJsonValue::Null, result.second);
            } else {
                sendErrorResponse(sender, (int)CmdType::DOCTOR_REJECT_APPOINTMENT, (int)StatusCode::INTERNAL_ERROR, result.second);
            }
        }
    );
}

void DoctorHandler::handleCompleteConsultation(ClientSocket* sender, const QJsonObject& request)
{
    if (!sender) return;

    QJsonObject data = request[JsonKeys::DATA].toObject();
    int appointmentId = data[JsonKeys::APPOINTMENT_ID].toInt();
    int doctorId = sender->userId();

    if (appointmentId <= 0 || doctorId <= 0) {
        sendErrorResponse(sender, (int)CmdType::DOCTOR_COMPLETE_CONSULTATION, (int)StatusCode::BAD_REQUEST, "参数无效");
        return;
    }

    // 完成咨询
    AsyncExecutor::run(sender,
        [appointmentId, doctorId](QSqlDatabase db) -> QPair<bool, QString> {
            AppointmentDao dao;
            QString errorMsg;
            bool success = dao.completeConsultation(db, appointmentId, doctorId, errorMsg);
            return {success, success ? "咨询已完成" : errorMsg};
        },
        [sender](QPair<bool, QString> result) {
            if (result.first) {
                sendSuccessResponse(sender, (int)CmdType::DOCTOR_COMPLETE_CONSULTATION, QJsonValue::Null, result.second);
            } else {
                sendErrorResponse(sender, (int)CmdType::DOCTOR_COMPLETE_CONSULTATION, (int)StatusCode::INTERNAL_ERROR, result.second);
            }
        }
    );
}

void DoctorHandler::handleSubmitReport(ClientSocket* sender, const QJsonObject& request)
{
    if (!sender) return;
    int doctorId = sender->userId();
    if (doctorId <= 0) {
        sendErrorResponse(sender, (int)CmdType::DOCTOR_SUBMIT_REPORT, (int)StatusCode::UNAUTHORIZED, "用户未登录");
        return;
    }

    QJsonObject data = request[JsonKeys::DATA].toObject();
    // 解析参数
    int appointmentId = data["appointmentId"].toInt();
    QString problem = data["problemDescription"].toString();
    QString process = data["consultationProcess"].toString();
    QString analysis = data["analysis"].toString();
    QString tags = data["tags"].toString();
    
    QString fullReport = QString("【主要问题】\n%1\n\n【咨询过程】\n%2\n\n【评估与建议】\n%3")
                         .arg(problem).arg(process).arg(analysis);

    if (appointmentId <= 0) {
        sendErrorResponse(sender, (int)CmdType::DOCTOR_SUBMIT_REPORT, (int)StatusCode::BAD_REQUEST, "参数错误");
        return;
    }

    // 提交报告
    AsyncExecutor::run(sender,
        [appointmentId, doctorId, fullReport, tags](QSqlDatabase db) -> OpResult {
            AppointmentDao dao;
            QString errorMsg;
            bool success = dao.saveReport(db, appointmentId, doctorId, fullReport, tags, errorMsg);
            
            return {success, success ? "报告提交成功" : errorMsg, QJsonValue::Null};
        },
        [sender](OpResult result) {
            if (result.success) {
                sendSuccessResponse(sender, (int)CmdType::DOCTOR_SUBMIT_REPORT, result.data, result.msg);
            } else {
                sendErrorResponse(sender, (int)CmdType::DOCTOR_SUBMIT_REPORT, (int)StatusCode::INTERNAL_ERROR, result.msg);
            }
        }
    );
}

void DoctorHandler::handleGetSchedule(ClientSocket* sender, const QJsonObject& request)
{
    int userId = sender->userId();
    QJsonObject data = request[JsonKeys::DATA].toObject();
    
    int targetDoctorId = data.contains("doctorId") ? data["doctorId"].toInt() : userId;
    
    int year = data["year"].toInt();
    int month = data["month"].toInt();

    if (targetDoctorId <= 0 || year == 0 || month == 0) {
        sendErrorResponse(sender, (int)CmdType::GET_DOCTOR_SCHEDULE, (int)StatusCode::BAD_REQUEST, "参数错误");
        return;
    }

    AsyncExecutor::run(sender,
        [targetDoctorId, year, month](QSqlDatabase db) -> QJsonObject {
            QDate startDate(year, month, 1);
            QDate endDate = startDate.addMonths(1).addDays(-1);
            
            // 获取该月每一天的排班掩码
            QMap<QString, int> scheduleMap = ScheduleDao::getScheduleRange(db, targetDoctorId, startDate, endDate);
            
            // 转换为 JsonObject 返回
            QJsonObject result;
            for(auto it = scheduleMap.begin(); it != scheduleMap.end(); ++it) {
                result[it.key()] = it.value();
            }
            return result;
        },
        [sender](QJsonObject result) {
            sendSuccessResponse(sender, (int)CmdType::GET_DOCTOR_SCHEDULE, result, "获取排班成功");
        }
    );
}

void DoctorHandler::handleUpdateSchedule(ClientSocket* sender, const QJsonObject& request)
{
    int doctorId = sender->userId(); // 只能改自己的
    QJsonObject data = request[JsonKeys::DATA].toObject();
    QString dateStr = data["date"].toString();
    int mask = data["mask"].toInt();

    QDate date = QDate::fromString(dateStr, "yyyy-MM-dd");
    if (!date.isValid()) {
        sendErrorResponse(sender, (int)CmdType::UPDATE_SCHEDULE, (int)StatusCode::BAD_REQUEST, "日期格式错误");
        return;
    }

    AsyncExecutor::run(sender,
        [doctorId, date, mask](QSqlDatabase db) -> bool {
            // TODO: 检查是否为医生

            return ScheduleDao::updateScheduleMask(db, doctorId, date, mask);
        },
        [sender](bool success) {
            if (success) {
                sendSuccessResponse(sender, (int)CmdType::UPDATE_SCHEDULE, QJsonValue::Null, "排班更新成功");
            } else {
                sendErrorResponse(sender, (int)CmdType::UPDATE_SCHEDULE, (int)StatusCode::INTERNAL_ERROR, "排班更新失败");
            }
        }
    );
}

void DoctorHandler::sendSuccessResponse(ClientSocket* sender, int cmd, const QJsonValue& data, const QString& msg)
{
    QJsonObject response;
    response[JsonKeys::CMD] = cmd;
    response[JsonKeys::CODE] = (int) StatusCode::SUCCESS;
    response[JsonKeys::MSG] = msg;
    response[JsonKeys::DATA] = data;

    if (sender) {
        sender->sendJson(response);
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
    }
}

void DoctorHandler::handleDeleteBooking(ClientSocket* sender, const QJsonObject& request)
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
            if (result.first) {
                sendSuccessResponse(sender, request[JsonKeys::CMD].toInt(), QJsonValue::Null, result.second);
            } else {
                sendErrorResponse(sender, request[JsonKeys::CMD].toInt(), (int)StatusCode::BAD_REQUEST, result.second);
            }
        }
    );
}