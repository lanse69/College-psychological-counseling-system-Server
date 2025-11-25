#include "RequestRouter.h"
#include "AuthHandler.h"
#include "BookingHandler.h"
#include "core/ProtocolDefs.h" 

void RequestRouter::dispatch(ClientSocket* sender, const QJsonObject& request) {
    // 1. 获取指令
    if (!request.contains(JsonKeys::CMD)) return;
    int cmd = request[JsonKeys::CMD].toInt();
    CmdType command = static_cast<CmdType>(cmd);

    // 2. 根据指令分发
    switch (command) {
        case CmdType::LOGIN:
            AuthHandler::handleLogin(sender, request);
            break;
        case CmdType::CREATE_BOOKING:
            BookingHandler::handleCreateBooking(sender, request);
            break;
        case CmdType::MODIFY_BOOKING_REQ: // 医生发起修改请求
            BookingHandler::handleModifyRequest(sender, request);
            break;
        
        // other case
        
        default:
            qWarning() << "Unknown command:" << cmd;
            break;
    }
}