#pragma once

#include <QtGlobal>
#include <QString>
#include <QDate>
#include <QDateTime>

// 定义包头长度(quint32)
static const qint64 PACKET_HEAD_SIZE = sizeof(quint32);
static const int MAX_PACKET_SIZE = 10 * 1024 * 1024; // 10MB

// [状态码定义 (StatusCode)]
enum StatusCode {
    SUCCESS = 200,
    NEGOTIATION_REQUIRED = 201,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    CONFLICT = 409,
    INTERNAL_ERROR = 500,
    NOT_IMPLEMENTED = 501
};

// 通信指令类型 (Command Type)
enum class CmdType {
    // 基础 & 认证 (1000 - 1009)
    LOGIN = 1000,
    LOGOUT,
    HEARTBEAT,
    UPDATE_PWD,
    GET_USER_INFO,
    UPDATE_USER_INFO,

    // 管理员 (1010 - 1019)
    ADMIN_ADD_USER = 1010,
    ADMIN_DEL_USER,
    ADMIN_GET_USER_LIST,
    GET_STATISTICS,

    // 医生/公共 (1020 - 1099)
    GET_DOCTOR_LIST = 1020,
    GET_DOCTOR_DETAIL,
    GET_DOCTOR_SCHEDULE,
    UPDATE_SCHEDULE,
    DOCTOR_GET_PATIENT_HISTORY = 2007,
    DOCTOR_GET_APPOINTMENTS = 1200,
    DOCTOR_CONFIRM_APPOINTMENT,
    DOCTOR_REJECT_APPOINTMENT,
    DOCTOR_COMPLETE_CONSULTATION,
    DOCTOR_GET_PATIENTS,
    DOCTOR_SUBMIT_REPORT,
    DOCTOR_GET_MY_SURVEY = 2101,
    DOCTOR_SAVE_SURVEY = 2102,
    DOCTOR_DELETE_BOOKING = 1206,

    // 学生/预约
    CREATE_BOOKING, 
    STUDENT_DELETE_BOOKING = 1105,
    CANCEL_BOOKING,
    MODIFY_BOOKING_DIRECT,
    MODIFY_BOOKING_REQ,
    MODIFY_BOOKING_REPLY,
    GET_MY_BOOKINGS,
    STUDENT_GET_DOCTOR_LIST = 1100,
    STUDENT_BOOK_APPOINTMENT,
    STUDENT_GET_MY_SCHEDULE,
    STUDENT_CANCEL_APPOINTMENT,
    STUDENT_SUBMIT_SURVEY,

    // 问卷 & 其他
    GET_SURVEY_LIST,
    GET_SURVEY_CONTENT,
    SUBMIT_SURVEY,
    
    // 推送
    PUSH_NOTIFICATION = 9000
};

static const int TIME_SLOT_COUNT = 7;
static const int MAX_TIME_SLOT_INDEX = TIME_SLOT_COUNT - 1;

// 定义不连续的时间段起始小时
static const int SLOT_START_HOURS[TIME_SLOT_COUNT] = {8, 9, 10, 14, 15, 16, 17};

// 获取某时段的起始小时
inline int GetSlotStartHour(int index) {
    if (index < 0 || index >= TIME_SLOT_COUNT) return -1;
    return SLOT_START_HOURS[index];
}

// 获取时间段的文本描述
inline QString GetTimeSlotText(int index) {
    int h = GetSlotStartHour(index);
    if (h == -1) return "未知时段";
    
    // 格式化为两位数
    return QString("%1:30 - %2:30")
           .arg(h, 2, 10, QChar('0'))
           .arg(h + 1, 2, 10, QChar('0'));
}

// 时间过期校验逻辑
// 返回 true 表示已过期，不可预约
inline bool IsTimeSlotExpired(const QString &dateStr, int slotIndex) {
    QDate date = QDate::fromString(dateStr, Qt::ISODate);
    if (!date.isValid()) date = QDate::fromString(dateStr, "yyyy/MM/dd");
    if (!date.isValid()) date = QDate::fromString(dateStr, "yyyy-M-d");

    if (!date.isValid()) return true; // 日期格式错误视为过期

    QDateTime now = QDateTime::currentDateTime();
    QDate today = now.date();

    if (date < today) return true;  // 过去日期
    if (date > today) return false; // 未来日期

    // 如果是今天，检查具体时间
    int startHour = GetSlotStartHour(slotIndex);
    if (startHour == -1) return true; // 无效时段

    // 规则：当前时间超过 "开始时间 + 30分钟" 即过期
    QTime cutoffTime(startHour, 30);
    return now.time() >= cutoffTime;
}

// 角色定义 (Role)
enum class UserRole { STUDENT = 1, DOCTOR = 2, ADMIN = 3 };

// 预约状态
enum class ApptStatus {
    PENDING = 0,
    CONFIRMED = 1,
    COMPLETED = 2,
    CANCELLED = 3,
    PENDING_CHANGE_CONFIRM = 4
};

// JSON Keys
namespace JsonKeys {
    const QString CMD = "cmd";
    const QString DATA = "data";
    const QString CODE = "code";
    const QString MSG = "msg";
    const QString USER_ID = "userId";
    const QString ROLE = "role";
    const QString USERNAME = "username";
    const QString PASSWORD = "password";
    const QString REAL_NAME = "realName";
    const QString INTRO = "intro";
    const QString SPEC = "spec";
    const QString TARGET_ID = "targetId";
    const QString APPOINTMENT_ID = "appointmentId";
    const QString DOCTOR_ID = "doctorId";
}