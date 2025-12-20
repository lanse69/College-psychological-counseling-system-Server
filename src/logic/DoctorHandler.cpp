#include "DoctorHandler.h"

#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QPair>
#include <QSqlQuery> 
#include <QSqlError>

#include "dao/UserDao.h"
#include "dao/AppointmentDao.h"
#include "dao/ScheduleDao.h"
#include "network/ClientSocket.h"
#include "core/ProtocolDefs.h"
#include "core/AsyncExecutor.h"
#include "core/ServerApp.h"

struct OpResult {
    bool success;
    QString msg;
    QJsonValue data; // 可选的返回数据
};

void DoctorHandler::handleGetDoctorList(ClientSocket* sender, const QJsonObject& request)
{
    // 获取请求中的 CMD
    int reqCmd = request[JsonKeys::CMD].toInt();

    // 获取医生列表
    AsyncExecutor::run(sender,
        [](QSqlDatabase db) -> QJsonArray {
            return UserDao::getDoctorList(db);
        },
        // 主线程回调
        [sender, reqCmd](QJsonArray doctorList) {
            // 使用请求时的 CMD 进行回复
            sendSuccessResponse(sender, reqCmd, doctorList, "获取医生列表成功");
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

    // 定义结果结构体
    struct CompleteResult {
        bool success;
        QString msg;
        int studentId;
    };

    // 完成咨询
    AsyncExecutor::run(sender,
        [appointmentId, doctorId](QSqlDatabase db) -> CompleteResult {
            // 先查询学生ID用于推送
            int studentId = 0;
            QSqlQuery q(db);
            q.prepare("SELECT student_id FROM appointments WHERE id = ?");
            q.addBindValue(appointmentId);
            if (q.exec() && q.next()) studentId = q.value(0).toInt();

            // 执行完成逻辑
            AppointmentDao dao;
            QString errorMsg;
            bool success = dao.completeConsultation(db, appointmentId, doctorId, errorMsg);
            
            if (!success) return {false, errorMsg, 0};
            return {true, "咨询已完成", studentId};
        },
        [sender, request](CompleteResult result) {
            if (result.success) {
                // 回复医生
                sendSuccessResponse(sender, (int)CmdType::DOCTOR_COMPLETE_CONSULTATION, QJsonValue::Null, result.msg);

                // 推送通知给学生
                if (result.studentId > 0) {
                    ClientSocket* stuSock = ServerApp::instance().getClient(result.studentId);
                    if (stuSock) {
                        QJsonObject notify;
                        notify[JsonKeys::CMD] = (int)CmdType::PUSH_NOTIFICATION;
                        notify[JsonKeys::CODE] = (int)StatusCode::SUCCESS;
                        notify[JsonKeys::MSG] = "医生已完成咨询报告，您现在可以查看结果了。";
                        notify["action"] = "refresh_schedule"; // 触发学生端刷新
                        stuSock->sendJson(notify);
                    }
                }
            } else {
                sendErrorResponse(sender, (int)CmdType::DOCTOR_COMPLETE_CONSULTATION, (int)StatusCode::INTERNAL_ERROR, result.msg);
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
        [doctorId, date, mask](QSqlDatabase db) -> QPair<bool, QString> {
            // 查询该医生在该日期所有“占用中”的预约
            // 状态 0(待确认), 1(已确认), 4(待修改确认) 都视为占用
            QSqlQuery query(db);
            query.prepare(R"(
                SELECT time_slot FROM appointments 
                WHERE doctor_id = ? AND date = ? AND status IN (0, 1, 4)
            )");
            query.addBindValue(doctorId);
            query.addBindValue(date);
            
            if (!query.exec()) {
                return {false, "查询预约冲突失败"};
            }

            int activeApptsMask = 0;
            while (query.next()) {
                int slot = query.value(0).toInt();
                if (slot >= 0 && slot <= 6) {
                    activeApptsMask |= (1 << slot);
                }
            }

            // 校验冲突
            // activeApptsMask 的某位是 1 (有预约)，但 mask 的对应位是 0 (医生设为空闲)，则冲突
            for (int i = 0; i <= 6; ++i) {
                bool hasAppt = (activeApptsMask >> i) & 1;
                bool settingFree = !((mask >> i) & 1); // 0是空闲

                if (hasAppt && settingFree) {
                    return {false, QString("时段 %1 尚有未完成的预约，无法设为空闲").arg(GetTimeSlotText(i))};
                }
            }

            // 更新
            if (ScheduleDao::updateScheduleMask(db, doctorId, date, mask)) {
                return {true, "排班更新成功"};
            } else {
                return {false, "数据库更新失败"};
            }
        },
        [sender, request](QPair<bool, QString> result) {
            if (result.first) {
                sendSuccessResponse(sender, (int)CmdType::UPDATE_SCHEDULE, QJsonValue::Null, result.second);
            } else {
                sendErrorResponse(sender, (int)CmdType::UPDATE_SCHEDULE, (int)StatusCode::CONFLICT, result.second);
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
    int doctorId = sender->userId();
    QJsonObject data = request[JsonKeys::DATA].toObject();
    int apptId = data[JsonKeys::APPOINTMENT_ID].toInt();

    // 传递删除结果和学生ID
    struct DelResult {
        bool success;
        QString msg;
        int studentId;
    };

    AsyncExecutor::run(sender,
        [doctorId, apptId](QSqlDatabase db) -> DelResult {
            int studentId = 0;
            QSqlQuery q(db);
            q.prepare("SELECT student_id FROM appointments WHERE id = ? AND doctor_id = ?");
            q.addBindValue(apptId);
            q.addBindValue(doctorId);
            if (q.exec() && q.next()) {
                studentId = q.value(0).toInt();
            }

            AppointmentDao dao;
            QString errorMsg;
            bool success = dao.deleteCancelledAppointment(db, apptId, doctorId, errorMsg);
            
            if (!success) {
                return {false, errorMsg, 0};
            }
            return {true, "记录已删除", studentId};
        },
        
        // 主线程回调
        [sender, request](DelResult res) {
            if (res.success) {
                // 回复医生
                sendSuccessResponse(sender, request[JsonKeys::CMD].toInt(), QJsonValue::Null, res.msg);

                // 推送通知给学生
                if (res.studentId > 0) {
                    ClientSocket* stuSock = ServerApp::instance().getClient(res.studentId);
                    if (stuSock) {
                        QJsonObject notify;
                        notify[JsonKeys::CMD] = (int)CmdType::PUSH_NOTIFICATION;
                        notify[JsonKeys::CODE] = (int)StatusCode::SUCCESS;
                        notify[JsonKeys::MSG] = ""; 
                        notify["action"] = "refresh_schedule"; 
                        stuSock->sendJson(notify);
                    }
                }
            } else {
                sendErrorResponse(sender, request[JsonKeys::CMD].toInt(), (int)StatusCode::BAD_REQUEST, res.msg);
            }
        }
    );
}