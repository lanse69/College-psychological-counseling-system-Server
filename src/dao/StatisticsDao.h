#pragma once

#include <QObject>
#include <QJsonArray>
#include <QSqlDatabase>

class StatisticsDao : public QObject
{
    Q_OBJECT
public:
    explicit StatisticsDao(QObject *parent = nullptr);

    /**
     * @brief 获取近12个月的咨询数量趋势
     * SQL: 按月份分组 COUNT(*)
     */
    QJsonArray getConsultTrend(QSqlDatabase db, QString &errorMsg);

    /**
     * @brief 获取热门咨询问题 (Tag)
     * SQL: 拆分 tags 字符串并统计频率
     */
    QJsonArray getCommonIssues(QSqlDatabase db, QString &errorMsg);

    // 学生性别分布
    QJsonArray getStudentGenderStats(QSqlDatabase db, QString &errorMsg);

    // 热门医生 Top 10
    QJsonArray getTopDoctors(QSqlDatabase db, QString &errorMsg);

    // 预约时段分布
    QJsonArray getPeakTimeSlots(QSqlDatabase db, QString &errorMsg);
};