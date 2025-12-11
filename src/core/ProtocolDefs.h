#pragma once

#include <QtGlobal>
#include <QString>

// 定义包头长度(quint32)
static const qint64 PACKET_HEAD_SIZE = sizeof(quint32);
static const int MAX_PACKET_SIZE = 10 * 1024 * 1024; // 最大允许包大小10MB

// [状态码定义 (StatusCode)]
enum StatusCode {
    SUCCESS = 200, // 操作成功

    // 业务流程相关
    NEGOTIATION_REQUIRED = 201, // 医生修改预约时，服务端收到请求，但需要学生确认，暂未修改入库

    // 客户端错误
    BAD_REQUEST = 400,  // 参数缺失或格式错误
    UNAUTHORIZED = 401, // 未登录或密码错误
    FORBIDDEN = 403,    // 权限不足
    NOT_FOUND = 404,    // 请求的资源(用户/预约)不存在
    CONFLICT = 409,     // 资源冲突 (如用户名已存在、该时间段已被预约、账号在别处登录)

    // 服务端错误
    INTERNAL_ERROR = 500, // 数据库错误或未知异常
    NOT_IMPLEMENTED = 501 // 接口尚未开发
};

// 通信指令类型 (Command Type)
enum class CmdType {
    // 基础 & 认证
    LOGIN = 1000, // 登录
    LOGOUT,       // 注销
    HEARTBEAT,    // 心跳包 (保持连接)
    UPDATE_PWD,   // 修改密码

    // 用户信息管理
    GET_USER_INFO,              // 获取个人信息
    UPDATE_USER_INFO,           // 修改个人信息
    ADMIN_ADD_USER,             // 管理员添加用户 (学生/医生)
    ADMIN_DEL_USER,             // 管理员删除用户
    ADMIN_GET_USER_LIST = 1010, // 管理员获取用户列表
    GET_DOCTOR_LIST = 1011,            // 获取医生列表 (含筛选)
    GET_DOCTOR_DETAIL = 1012,          // 获取医生详细信息
    DOCTOR_GET_PATIENT_HISTORY = 2007,

    // 排班管理 (Schedule)
    GET_DOCTOR_SCHEDULE = 1015, // 获取医生的排班表 (学生/医生通用)
    UPDATE_SCHEDULE = 1016,     // 医生/管理员 修改排班 (设置忙碌/空闲)

    // 预约核心 (Booking)
    CREATE_BOOKING, // 学生发起预约
    STUDENT_DELETE_BOOKING = 1105,
    DOCTOR_DELETE_BOOKING = 1206,
    CANCEL_BOOKING, // 取消预约 (学生/医生/管理员通用)

    // 预约修改
    MODIFY_BOOKING_DIRECT, // 直接修改 (学生发起 -> 服务端查表校验 -> 返回结果)
    MODIFY_BOOKING_REQ,    // 协商修改请求 (医生发起 -> 服务端转发给学生)
    MODIFY_BOOKING_REPLY,  // 协商修改响应 (学生同意/拒绝 -> 服务端更新)

    GET_MY_BOOKINGS, // 获取我的预约记录

    // 问卷 & 报告 (Survey & Report)
    DOCTOR_GET_MY_SURVEY = 2101, // 医生获取自己的问卷模版
    DOCTOR_SAVE_SURVEY = 2102,   // 医生保存/更新问卷模版
    GET_SURVEY_LIST,    // 获取可用问卷列表
    GET_SURVEY_CONTENT, // 获取具体问卷题目
    SUBMIT_SURVEY,      // 学生提交问卷答案
    UPLOAD_SURVEY,      // 医生上传/修改问卷题目
    WRITE_REPORT,       // 医生填写咨询报告
    GET_REPORT,         // 获取咨询报告

    // 统计报表 (Admin)
    GET_STATISTICS, // 获取统计数据

    // 服务端推送通知 (Server Push)
    PUSH_NOTIFICATION = 9000, // 服务端主动推消息 (如: 预约被取消、收到修改请求)

    // 学生端操作
    STUDENT_GET_DOCTOR_LIST = 1100, // 学生获取医生列表
    STUDENT_BOOK_APPOINTMENT,       // 学生预约
    STUDENT_GET_MY_SCHEDULE,        // 学生获取我的预约
    STUDENT_CANCEL_APPOINTMENT,     // 学生取消预约
    STUDENT_SUBMIT_SURVEY,          // 学生提交心理测评

    // 医生端操作
    DOCTOR_GET_APPOINTMENTS = 1200, // 医生获取预约列表
    DOCTOR_CONFIRM_APPOINTMENT,     // 医生确认预约
    DOCTOR_REJECT_APPOINTMENT,      // 医生拒绝预约
    DOCTOR_COMPLETE_CONSULTATION,   // 医生完成咨询
    DOCTOR_GET_PATIENTS,            // 医生获取患者列表
    DOCTOR_SUBMIT_REPORT,           // 医生提交咨询报告
};

// [排班时间段位掩码定义 (TimeSlot Bitmask)]
// 用于 schedules 表的 time_slot_flags 字段
// 0 = 空闲, 1 = 忙碌/不开放
// Bit 0 (Index 0): 08:30 - 09:30
// Bit 1 (Index 1): 09:30 - 10:30
// Bit 2 (Index 2): 10:30 - 11:30
// Bit 3 (Index 3): 14:30 - 15:30
// Bit 4 (Index 4): 15:30 - 16:30
// Bit 5 (Index 5): 16:30 - 17:30
// Bit 6 (Index 6): 17:30 - 18:30
static const int TIME_SLOT_START_HOUR = 8; // 仅作参考
static const int MAX_TIME_SLOT_INDEX = 6;  // 最大索引 (0~6 共7个)
static const int TIME_SLOT_COUNT = 7;      // 总时段数

// 获取时间段的文本描述（确保全端统一）
inline QString GetTimeSlotText(int index) {
    switch(index) {
        case 0: return "08:30 - 09:30";
        case 1: return "09:30 - 10:30";
        case 2: return "10:30 - 11:30";
        case 3: return "14:30 - 15:30";
        case 4: return "15:30 - 16:30";
        case 5: return "16:30 - 17:30";
        case 6: return "17:30 - 18:30";
        default: return "未知时段";
    }
}

// 角色定义 (Role)
enum class UserRole { STUDENT = 1, DOCTOR = 2, ADMIN = 3 };

// 预约状态 (Appointment Status)
enum class ApptStatus {
    PENDING = 0,               // 待生效
    CONFIRMED = 1,             // 已确认/即将开始
    COMPLETED = 2,             // 已完成
    CANCELLED = 3,             // 已取消
    PENDING_CHANGE_CONFIRM = 4 // 医生发起修改，待学生确认
};

// JSON 键名常量 (Key Constants)
namespace JsonKeys {
const QString CMD = "cmd";     // 指令类型 (int)
const QString DATA = "data";   // 数据主体 (object)
const QString CODE = "code";   // 状态码
const QString MSG = "msg";     // 错误信息
const QString TOKEN = "token"; // 鉴权 Token

// 常用字段
const QString USER_ID = "userId";
const QString ROLE = "role";
const QString USERNAME = "username";
const QString PASSWORD = "password";
const QString REAL_NAME = "realName";
const QString INTRO = "intro";        // 医生简介
const QString SPEC = "spec";          // 擅长领域 (Specialized Field)
const QString TARGET_ID = "targetId"; // 要删除的目标用户ID
const QString APPT_ID = "apptId";     // 预约ID
const QString DATE = "date";          // "2025-11-27"
const QString DOC_ID = "docId";
const QString STU_ID = "stuId";
const QString TIME_SLOT = "timeSlot"; // "14:00-15:00"

// 预约相关字段
const QString DOCTOR_ID = "doctorId";
const QString APPOINTMENT_ID = "appointmentId";

// 医生相关字段
const QString SPECIALIZED_FIELD = "specializedField";

// 预约状态相关
const QString STATUS = "status";
const QString STATUS_TEXT = "statusText";

// 咨询相关字段
const QString PATIENT_NAME = "patientName";
const QString REASON = "reason";
const QString REPORT = "report";
const QString RESULT_TAGS = "resultTags";

// 时间段相关
const QString TIME_SLOT_TEXT = "timeSlotText";

// 统计报表相关
const QString STAT_TYPE = "statType"; // common_issues, consult_trend
const QString STAT_LABEL = "label";   // 图表X轴
const QString STAT_VALUE = "value";   // 图表Y轴
}
