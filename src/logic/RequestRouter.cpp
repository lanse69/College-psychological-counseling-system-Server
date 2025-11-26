#include "RequestRouter.h"
#include "AuthHandler.h"
#include "BookingHandler.h"
#include "AdminHandler.h"
#include "core/ProtocolDefs.h" 

RequestRouter& RequestRouter::instance() {
    static RequestRouter instance;
    return instance;
}

void RequestRouter::dispatch(ClientSocket* sender, const QJsonObject& request) {
    if (!request.contains(JsonKeys::CMD)) return;

    int cmdVal = request[JsonKeys::CMD].toInt();
    CmdType cmd = static_cast<CmdType>(cmdVal);

    switch (cmd) {
        case CmdType::LOGIN:
            AuthHandler::handleLogin(sender, request);
            break;
        case CmdType::CREATE_BOOKING:
            BookingHandler::handleCreateBooking(sender, request);
            break;
        case CmdType::MODIFY_BOOKING_REQ: // 医生发起修改请求
            BookingHandler::handleModifyRequest(sender, request);
            break;
        case CmdType::ADMIN_ADD_USER:
            AdminHandler::handleAddUser(sender, request);
            break;
        case CmdType::ADMIN_DEL_USER:
            AdminHandler::handleDeleteUser(sender, request);
            break;
        case CmdType::GET_STATISTICS:
            // 这里调用支持多线程的统计接口
            AdminHandler::handleGetStatistics(sender, request);
            break;

        // other case
        
        default:
            // 处理未知指令
            qWarning() << "Unknown command:" << cmd;
            break;
    }
}