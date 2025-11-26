#include "BookingHandler.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonArray>
#include <QDebug>

#include "dao/DBManager.h"
#include "core/ProtocolDefs.h"
#include "core/ServerApp.h"

void BookingHandler::handleCreateBooking(ClientSocket* sender, const QJsonObject& request) {
    
}

// 学生修改预约：校验医生时间表
void BookingHandler::handleModifyBookingDirect(ClientSocket* sender, const QJsonObject& request) {
    
}

// 医生发起修改请求：需学生同意
void BookingHandler::handleModifyRequest(ClientSocket* sender, const QJsonObject& request) {
    
}

// 学生回复修改请求
void BookingHandler::handleModifyReply(ClientSocket* sender, const QJsonObject& request) {

}

void BookingHandler::handleCancelBooking(ClientSocket* sender, const QJsonObject& request) {

}

void BookingHandler::handleGetMyBookings(ClientSocket* sender, const QJsonObject& request) {

}