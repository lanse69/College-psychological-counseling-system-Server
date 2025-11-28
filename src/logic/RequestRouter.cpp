#include "RequestRouter.h"

#include <QDebug>

#include "AuthHandler.h"
#include "BookingHandler.h"
#include "AdminHandler.h"
#include "DoctorHandler.h"
#include "SurveyHandler.h"
#include "core/ProtocolDefs.h"
#include "network/ClientSocket.h"

RequestRouter& RequestRouter::instance() {
    static RequestRouter instance;
    return instance;
}

void RequestRouter::dispatch(ClientSocket* sender, const QJsonObject& request) {
    if (!request.contains(JsonKeys::CMD)) return;

    int cmdVal = request[JsonKeys::CMD].toInt();
    CmdType cmd = static_cast<CmdType>(cmdVal);

    int uid = sender->userId();
    QString userStr = (uid == -1) ? "未登录(Guest)" : QString::number(uid);
    qDebug() << "分派 CMD:" << cmdVal << " 来源用户:" << userStr;

    switch (cmd) {
        // 认证相关
        case CmdType::LOGIN:
            AuthHandler::handleLogin(sender, request);
            break;
            
        // 预约核心业务
        case CmdType::CREATE_BOOKING:
            BookingHandler::handleCreateBooking(sender, request);
            break;
        case CmdType::CANCEL_BOOKING:
            BookingHandler::handleCancelBooking(sender, request);
            break;
        case CmdType::GET_MY_BOOKINGS:
            BookingHandler::handleGetMyBookings(sender, request);
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

        // 医生排班与信息
        // TODO
        // 补充 DoctorHandler::handleUpdateSchedule 等

        // else

        default:
            qWarning() << "未知或未处理的命令:" << cmdVal;
            break;
    }
}
