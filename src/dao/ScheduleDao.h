#pragma once

#include <QDate>
#include <QList>
#include <QMap>

class ScheduleDao {
public:
    /**
     * @brief 确保某医生某天的排班记录存在
     * 若不存在则插入一条默认空闲(flags=0)的记录
     */
    static bool initSchedule(int doctorId, QDate date);

    /**
     * @brief 获取单日排班掩码
     * @return int 掩码 (0=空闲, 1=忙/不开放)
     */
    static int getScheduleFlag(int doctorId, QDate date);

    /**
     * @brief 获取某医生指定日期范围的排班
     * 用于前端日历渲染
     * @return Map<DateString, Flags>
     */
    static QMap<QString, int> getScheduleRange(int doctorId, QDate startDate, QDate endDate);

    /**
     * @brief 检查某个时间段是否空闲
     * @param slotIndex 0-8 (对应 8:00 到 17:00)
     */
    static bool isSlotAvailable(int doctorId, QDate date, int slotIndex);

    /**
     * @brief 占用时间段 (用于预约成功)
     * 使用位运算 OR 操作
     */
    static bool occupySlot(int doctorId, QDate date, int slotIndex);

    /**
     * @brief 释放时间段 (用于取消预约)
     * 使用位运算 AND NOT 操作
     */
    static bool releaseSlot(int doctorId, QDate date, int slotIndex);

    /**
     * @brief 医生手动修改排班掩码
     * (覆盖整个掩码)
     */
    static bool updateScheduleMask(int doctorId, QDate date, int newMask);
};
