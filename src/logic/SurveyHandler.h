#pragma once

#include <QObject>
#include <QJsonObject>

class ClientSocket;

class SurveyHandler : public QObject
{
    Q_OBJECT
public:
    // 获取问卷内容 (CMD: GET_SURVEY_CONTENT)
    static void handleGetSurveyContent(ClientSocket* sender, const QJsonObject& request);

    // 学生提交问卷 (CMD: STUDENT_SUBMIT_SURVEY)
    static void handleStudentSubmitSurvey(ClientSocket* sender, const QJsonObject& request);

    // 医生获取自己的问卷
    static void handleDoctorGetSurvey(ClientSocket* sender, const QJsonObject& request);

    // 医生保存问卷
    static void handleDoctorSaveSurvey(ClientSocket* sender, const QJsonObject& request);

private:
    SurveyHandler() = default;
};