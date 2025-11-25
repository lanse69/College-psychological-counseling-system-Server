#pragma once

#include <QtGlobal>
#include <QString>

// 定义包头长度为 4 字节 (quint32)
static const qint64 PACKET_HEAD_SIZE = sizeof(quint32);

static const int MAX_PACKET_SIZE = 10 * 1024 * 1024; // 最大允许包大小10MB

// 1. 通信指令类型 (Command Type)
enum class CmdType {
    // 基础 & 认证
    LOGIN = 1000,           // 登录
    LOGOUT,                 // 注销
    HEARTBEAT,              // 心跳包 (保持连接)
    UPDATE_PWD,             // 修改密码

    // 用户信息管理
    GET_USER_INFO,          // 获取个人信息
    UPDATE_USER_INFO,       // 修改个人信息
    ADMIN_ADD_USER,         // 管理员添加用户 (学生/医生)
    ADMIN_DEL_USER,         // 管理员删除用户
    GET_DOCTOR_LIST,        // 获取医生列表 (含筛选)
    GET_DOCTOR_DETAIL,      // 获取医生详细信息

    // 排班管理 (Schedule)
    GET_DOCTOR_SCHEDULE,    // 获取某医生的排班表 (学生/医生通用)
    UPDATE_SCHEDULE,        // 医生/管理员 修改排班 (设置忙碌/空闲)

    // 预约核心 (Booking)
    CREATE_BOOKING,         // 学生发起预约
    CANCEL_BOOKING,         // 取消预约 (学生/医生/管理员通用，逻辑后端区分)
    
    // 预约修改 (特殊逻辑)
    MODIFY_BOOKING_DIRECT,  // 直接修改 (学生发起 -> 服务端查表校验 -> 返回结果)
    MODIFY_BOOKING_REQ,     // 协商修改请求 (医生发起 -> 服务端转发给学生)
    MODIFY_BOOKING_REPLY,   // 协商修改响应 (学生同意/拒绝 -> 服务端更新)

    GET_MY_BOOKINGS,        // 获取我的预约记录

    // 问卷 & 报告 (Survey & Report)
    GET_SURVEY_LIST,        // 获取可用问卷列表
    GET_SURVEY_CONTENT,     // 获取具体问卷题目
    SUBMIT_SURVEY,          // 学生提交问卷答案
    UPLOAD_SURVEY,          // 医生上传/修改问卷题目
    WRITE_REPORT,           // 医生填写咨询报告
    GET_REPORT,             // 获取咨询报告

    // 统计报表 (Admin)
    GET_STATISTICS,         // 获取统计数据 (用于绘图)

    // 服务端推送通知 (Server Push)
    PUSH_NOTIFICATION = 9000 // 服务端主动推消息 (如: 预约被取消、收到修改请求)
};

// 2. 角色定义 (Role)
enum class UserRole {
    STUDENT = 1,
    DOCTOR = 2,
    ADMIN = 3
};

// 3. 预约状态 (Appointment Status)
enum class ApptStatus {
    PENDING = 0,            // 待生效 (如需审核)
    CONFIRMED = 1,          // 已确认/即将开始
    COMPLETED = 2,          // 已完成
    CANCELLED = 3,          // 已取消
    PENDING_CHANGE_CONFIRM  // 待学生确认变更 (医生发起了修改请求)
};

// 4. JSON 键名常量 (Key Constants)
namespace JsonKeys {
    const QString CMD       = "cmd";        // 指令类型 (int)
    const QString DATA      = "data";       // 数据主体 (object)
    const QString CODE      = "code";       // 状态码 (200成功)
    const QString MSG       = "msg";        // 错误信息
    const QString TOKEN     = "token";      // 鉴权 Token
    
    // 常用字段
    const QString USER_ID   = "userId";
    const QString ROLE      = "role";
    const QString USERNAME  = "username";
    const QString PASSWORD  = "password";
    const QString APPT_ID   = "apptId";     // 预约ID
    const QString DOC_ID    = "docId";
    const QString STU_ID    = "stuId";
    const QString DATE      = "date";       // "2023-10-27"
    const QString TIME_SLOT = "timeSlot";   // "14:00-15:00"
    const QString STAT_TYPE = "statType";   // 报表类型
}
