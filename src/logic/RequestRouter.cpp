#include "RequestRouter.h"

#include <QDebug>

#include "AuthHandler.h"
#include "BookingHandler.h"
#include "AdminHandler.h"
#include "DoctorHandler.h"
#include "SurveyHandler.h"
#include "core/ProtocolDefs.h"
#include "network/ClientSocket.h"

RequestRouter& RequestRouter::instance()
{
    static RequestRouter instance;
    return instance;
}

void RequestRouter::dispatch(
    ClientSocket* sender, const QJsonObject& request)
{
    if (!request.contains(JsonKeys::CMD)) {
        qWarning() << "收到无效数据包：缺少 CMD 字段";
        return;
    }

    int cmdVal = request[JsonKeys::CMD].toInt();
    CmdType cmd = static_cast<CmdType>(cmdVal);

    if (!sender) {
        qCritical() << "错误: sender为空指针";
        return;
    }

    int uid = sender->userId();
    qDebug() << "路由分发 CMD:" << cmdVal << " 用户ID:" << (uid == -1 ? "Guest" : QString::number(uid));
    
    switch (cmd) {
        // 认证相关
        case CmdType::LOGIN:
            AuthHandler::handleLogin(sender, request);
            break;

        // 预约核心业务
        case CmdType::STUDENT_BOOK_APPOINTMENT:
            BookingHandler::handleCreateBooking(sender, request);
            break;
        case CmdType::STUDENT_CANCEL_APPOINTMENT:
            BookingHandler::handleCancelBooking(sender, request);
            break;
        case CmdType::STUDENT_GET_MY_SCHEDULE:
            BookingHandler::handleGetMyBookings(sender, request);
            break;
        case CmdType::DOCTOR_GET_APPOINTMENTS:
            DoctorHandler::handleGetAppointments(sender, request);
            break;
        case CmdType::DOCTOR_CONFIRM_APPOINTMENT:
            DoctorHandler::handleConfirmAppointment(sender, request);
            break;
        case CmdType::DOCTOR_REJECT_APPOINTMENT:
            DoctorHandler::handleRejectAppointment(sender, request);
            break;
        case CmdType::DOCTOR_SUBMIT_REPORT:
            DoctorHandler::handleSubmitReport(sender, request);
            break;
        case CmdType::DOCTOR_COMPLETE_CONSULTATION:
            DoctorHandler::handleCompleteConsultation(sender, request);
            break;
        case CmdType::DOCTOR_GET_PATIENTS:
            DoctorHandler::handleGetPatients(sender, request);
            break;

        // 预约修改
        case CmdType::MODIFY_BOOKING_DIRECT:
            // 学生直接修改
            BookingHandler::handleModifyBookingDirect(sender, request);
            break;
        case CmdType::MODIFY_BOOKING_REQ:
            // 医生发起修改请求
            BookingHandler::handleModifyRequest(sender, request);
            break;
        case CmdType::MODIFY_BOOKING_REPLY:
            // 学生回复医生的请求
            BookingHandler::handleModifyReply(sender, request);
            break;

        // 管理员业务
        case CmdType::ADMIN_ADD_USER:
            AdminHandler::handleAddUser(sender, request);
            break;
        case CmdType::ADMIN_DEL_USER:
            AdminHandler::handleDeleteUser(sender, request);
            break;
        case CmdType::GET_STATISTICS:
            AdminHandler::handleGetStatistics(sender, request);
            break;
        case CmdType::ADMIN_GET_USER_LIST:
            AdminHandler::handleGetUserList(sender, request);
            break;

        // 医生业务
        case CmdType::GET_DOCTOR_LIST:
            DoctorHandler::handleGetDoctorList(sender, request);
            break;
        case CmdType::GET_DOCTOR_DETAIL:
            DoctorHandler::handleGetDoctorDetail(sender, request);
            break;
        case CmdType::DOCTOR_GET_PATIENT_HISTORY:
            DoctorHandler::handleGetPatientHistory(sender, request);
            break;

        // 学生端医生
        case CmdType::STUDENT_GET_DOCTOR_LIST:
            DoctorHandler::handleStudentGetDoctorList(sender, request);
            break;
        case CmdType::STUDENT_SUBMIT_SURVEY:
            // SurveyHandler::handleSubmitSurvey(sender, request);
            qWarning() << "STUDENT_SUBMIT_SURVEY 待实现";
            break;

            // 医生排班与信息（待扩展）
            // TODO: 补充 DoctorHandler::handleUpdateSchedule 等

        case CmdType::HEARTBEAT:
            break;

            // 其他未处理的命令
        default:
            qWarning() << "未知或未处理的命令:" << cmdVal;
            break;
    }
}
