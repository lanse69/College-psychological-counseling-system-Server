#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlDatabase>

class QuestionnaireDao : public QObject
{
    Q_OBJECT
public:
    explicit QuestionnaireDao(QObject *parent = nullptr);

    /**
     * @brief 根据预约ID获取对应的问卷内容
     * 逻辑：通过预约ID找到医生ID -> 查找该医生的最新问卷
     */
    QJsonObject getSurveyByAppointment(QSqlDatabase db, int appointmentId, QString &errorMsg);

    /**
     * @brief 保存学生提交的答案
     */
    bool saveAnswers(QSqlDatabase db, int appointmentId, int studentId, const QJsonArray &answers, QString &errorMsg);

    /**
     * @brief 获取某位医生的最新问卷模版
     */
    QJsonObject getDoctorSurvey(QSqlDatabase db, int doctorId, QString &errorMsg);

    /**
     * @brief 保存/更新医生的问卷
     * 策略：直接插入一条新记录 (version control by time)，读取时取最新的
     */
    bool saveSurveyTemplate(QSqlDatabase db, int doctorId, const QString &title, const QJsonArray &questions, QString &errorMsg);

private:
    // 如果没有问卷，创建一个默认的
    void ensureDefaultSurvey(QSqlDatabase db, int doctorId);
};