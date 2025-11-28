#include "ScheduleDao.h"
#include "DBManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

bool ScheduleDao::initSchedule(int doctorId, QDate date) {
    QSqlDatabase db = DBManager::instance().getMainDatabase();
    QSqlQuery query(db);

    // 记录已存在则什么都不做
    // 如果不存在则插入 0
    query.prepare(R"(
        INSERT INTO schedules (doctor_id, date, time_slot_flags)
        VALUES (:did, :date, 0)
        ON CONFLICT (doctor_id, date) DO NOTHING
    )");
    query.bindValue(":did", doctorId);
    query.bindValue(":date", date);

    if (!query.exec()) {
        qCritical() << "错误: 初始化排班失败 " << query.lastError().text();
        return false;
    }
    return true;
}

int ScheduleDao::getScheduleFlag(int doctorId, QDate date) {
    QSqlQuery query(DBManager::instance().getMainDatabase());
    query.prepare("SELECT time_slot_flags FROM schedules WHERE doctor_id = :did AND date = :date");
    query.bindValue(":did", doctorId);
    query.bindValue(":date", date);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    // 无记录则视为默认空闲（0）
    return 0;
}

QMap<QString, int> ScheduleDao::getScheduleRange(int doctorId, QDate startDate, QDate endDate) {
    QMap<QString, int> resultMap;
    QSqlQuery query(DBManager::instance().getMainDatabase());

    query.prepare(R"(
        SELECT to_char(date, 'YYYY-MM-DD'), time_slot_flags
        FROM schedules
        WHERE doctor_id = :did AND date >= :start AND date <= :end
    )");
    query.bindValue(":did", doctorId);
    query.bindValue(":start", startDate);
    query.bindValue(":end", endDate);

    if (query.exec()) {
        while (query.next()) {
            resultMap.insert(query.value(0).toString(), query.value(1).toInt());
        }
    } else {
        qWarning() << "警告: 获取排班范围失败 " << query.lastError().text();
    }
    return resultMap;
}

bool ScheduleDao::isSlotAvailable(int doctorId, QDate date, int slotIndex) {
    if (slotIndex < 0 || slotIndex > 31) return false;

    int flags = getScheduleFlag(doctorId, date);
    // 检查对应位是否为 0
    // (flags >> slotIndex) & 1
    bool isBusy = (flags & (1 << slotIndex));
    return !isBusy;
}

bool ScheduleDao::occupySlot(int doctorId, QDate date, int slotIndex) {
    // 确保记录存在
    if (!initSchedule(doctorId, date)) return false;

    QSqlQuery query(DBManager::instance().getMainDatabase());
    // 1 << slotIndex 计算位掩码
    // | (OR) 操作符将该位置 1
    query.prepare(R"(
        UPDATE schedules
        SET time_slot_flags = time_slot_flags | (1 << :slot)
        WHERE doctor_id = :did AND date = :date
    )");

    query.bindValue(":slot", slotIndex);
    query.bindValue(":did", doctorId);
    query.bindValue(":date", date);

    if (!query.exec()) {
        qCritical() << "错误: 占用时间段失败 " << query.lastError().text();
        return false;
    }
    return true;
}

bool ScheduleDao::releaseSlot(int doctorId, QDate date, int slotIndex) {
    QSqlQuery query(DBManager::instance().getMainDatabase());
    // 利用位运算 AND NOT (& ~) 将对应位置为 0
    query.prepare(R"(
        UPDATE schedules
        SET time_slot_flags = time_slot_flags & ~(1 << :slot)
        WHERE doctor_id = :did AND date = :date
    )");

    query.bindValue(":slot", slotIndex);
    query.bindValue(":did", doctorId);
    query.bindValue(":date", date);

    if (!query.exec()) {
        qCritical() << "错误: 释放时间段失败 " << query.lastError().text();
        return false;
    }
    return true;
}

bool ScheduleDao::updateScheduleMask(int doctorId, QDate date, int newMask) {
    if (!initSchedule(doctorId, date)) return false;

    QSqlQuery query(DBManager::instance().getMainDatabase());
    query.prepare("UPDATE schedules SET time_slot_flags = :mask WHERE doctor_id = :did AND date = :date");
    query.bindValue(":mask", newMask);
    query.bindValue(":did", doctorId);
    query.bindValue(":date", date);

    if (!query.exec()) {
        qCritical() << "错误: 更新排班掩码失败 " << query.lastError().text();
        return false;
    }
    return true;
}
