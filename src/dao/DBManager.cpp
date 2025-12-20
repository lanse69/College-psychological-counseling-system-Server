#include "DBManager.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDebug>
#include <QMap>

#include "core/ConfigManager.h"

DBManager &DBManager::instance()
{
    static DBManager instance;
    return instance;
}

DBManager::DBManager() {}

DBManager::~DBManager()
{
    if (m_mainDb.isOpen()) {
        m_mainDb.close();
    }
}

QSqlDatabase DBManager::getMainDatabase() const
{
    // 如果连接未打开，尝试重新打开
    if (!m_mainDb.isOpen()) {
        qDebug() << "主数据库连接已断开，尝试重连...";
        const_cast<DBManager*>(this)->m_mainDb.open();
    }
    return m_mainDb;
}

bool DBManager::connectToDatabase()
{
    // 获取配置
    const DBConfig &config = ConfigManager::instance().db();

    m_dbName = config.dbName;

    // 检查 PostgreSQL 驱动
    if (!QSqlDatabase::isDriverAvailable("QPSQL")) {
        qCritical() << "驱动错误: 未检测到 QPSQL 驱动!";
        qCritical()
            << "请确保已安装 PostgreSQL 客户端库 (libpq) 并将其路径添加到环境变量 PATH 中。";
        return false;
    }

    // 连接到默认的 'postgres' 数据库以创建新库
    {
        const QString tempConnName = "TempInitConnection";
        if (QSqlDatabase::contains(tempConnName)) {
            QSqlDatabase::removeDatabase(tempConnName);
        }

        QSqlDatabase tempDb = QSqlDatabase::addDatabase("QPSQL", tempConnName);
        tempDb.setHostName(config.host);
        tempDb.setPort(config.port);
        tempDb.setUserName(config.username);
        tempDb.setPassword(config.password);
        tempDb.setDatabaseName("postgres");

        if (tempDb.open()) {
            {
                QSqlQuery query(tempDb);
                query.prepare("SELECT 1 FROM pg_database WHERE datname = ?");
                query.addBindValue(config.dbName);
                if (!(query.exec() && query.next())) {
                    // 数据库不存在，创建它
                    qDebug() << "创建数据库:" << config.dbName;
                    if (!query.exec(QString("CREATE DATABASE \"%1\"").arg(config.dbName))) {
                        qCritical() << "创建数据库失败:" << query.lastError().text();
                    }
                }
            } // 销毁query对象, 方便后面安全关闭连接
            tempDb.close();
        } else {
            qCritical() << "连接 Postgres 服务(db=postgres)失败:" << tempDb.lastError().text();
            return false;
        }
    }
    QSqlDatabase::removeDatabase("TempInitConnection");

    // 连接主连接
    const QString mainConnName = "PsyServerMainConnection";
    if (QSqlDatabase::contains(mainConnName)) {
        m_mainDb = QSqlDatabase::database(mainConnName);
    } else {
        m_mainDb = QSqlDatabase::addDatabase("QPSQL", mainConnName);
    }

    m_mainDb.setHostName(config.host);
    m_mainDb.setPort(config.port);
    m_mainDb.setUserName(config.username);
    m_mainDb.setPassword(config.password);
    m_mainDb.setDatabaseName(config.dbName);

    if (!m_mainDb.open()) {
        qCritical() << "数据库主连接连接失败:" << m_mainDb.lastError().text();
        return false;
    }
    qDebug() << "已连接 PostgreSQL 数据库:" << config.dbName;
    return true;
}

QSqlDatabase DBManager::openThreadConnection(QString &connectionName)
{
    // 使用 UUID 确保连接名在整个程序生命周期内唯一，防止线程间冲突
    connectionName = QString("ThreadConn_%1").arg(QUuid::createUuid().toString());

    // 添加数据库
    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", connectionName);
    const DBConfig &config = ConfigManager::instance().db();

    db.setHostName(config.host);
    db.setPort(config.port);
    db.setUserName(config.username);
    db.setPassword(config.password);
    db.setDatabaseName(config.dbName);

    if (!db.open()) {
        qCritical() << "线程连接数据库失败 [" << connectionName << "]:" << db.lastError().text();
    }
    return db;
}

void DBManager::closeThreadConnection(const QString &connectionName)
{
    // 获取数据库对象
    {
        QSqlDatabase db = QSqlDatabase::database(connectionName);
        if (db.isOpen()) {
            db.close();
        }
    } 
    // 必须让 db 对象超出作用域销毁后，才能调用 removeDatabase
    QSqlDatabase::removeDatabase(connectionName);
}

bool DBManager::initTables()
{
    QMap<QString, QString> tables;

    // 用户表 (users)
    // role: 1=Student, 2=Doctor, 3=Admin
    tables["users"] = R"(
        CREATE TABLE IF NOT EXISTS users (
            id SERIAL PRIMARY KEY,
            username VARCHAR(50) NOT NULL UNIQUE,
            password VARCHAR(255) NOT NULL,
            salt VARCHAR(64) DEFAULT '', 
            role INT NOT NULL,
            real_name VARCHAR(50),
            gender VARCHAR(10),
            age INT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )";

    // 医生信息表 (doctor_info)
    // 扩展 users 表，存储医生特有信息
    tables["doctor_info"] = R"(
        CREATE TABLE IF NOT EXISTS doctor_info (
            user_id INT PRIMARY KEY,
            intro TEXT,
            specialized_field VARCHAR(255),
            avatar_path VARCHAR(255),
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        );
    )";

    // 医生排班表 (schedules)
    /*
     * 时间段位掩码定义 (Time Slot Bitmask Definitions)
     * 用于 schedules 表的 time_slot_flags 字段
     *
     * 数据类型: 32位整数 (INT / quint32)
     * 规则: 0 = 空闲, 1 = 忙碌 (已有预约 或 医生设为休息)
     *
     * Bit 0 (1 << 0): 08:00 - 09:00
     * Bit 1 (1 << 1): 09:00 - 10:00
     * Bit 2 (1 << 2): 10:00 - 11:00
     * Bit 3 (1 << 3): 11:00 - 12:00
     * Bit 4 (1 << 4): 13:00 - 14:00
     * Bit 5 (1 << 5): 14:00 - 15:00
     * Bit 6 (1 << 6): 15:00 - 16:00
     * Bit 7 (1 << 7): 16:00 - 17:00
     * Bit 8 (1 << 8): 17:00 - 18:00
     */
    tables["schedules"] = R"(
        CREATE TABLE IF NOT EXISTS schedules (
            id SERIAL PRIMARY KEY,
            doctor_id INT NOT NULL,
            date DATE NOT NULL,
            time_slot_flags INT DEFAULT 0,
            status INT DEFAULT 1, 
            FOREIGN KEY (doctor_id) REFERENCES users(id) ON DELETE CASCADE,
            UNIQUE (doctor_id, date)
        );
    )";

    // 预约记录表 (appointments)
    // status: 0=Pending, 1=Confirmed, 2=Completed, 3=Cancelled
    tables["appointments"] = R"(
        CREATE TABLE IF NOT EXISTS appointments (
            id SERIAL PRIMARY KEY,
            student_id INT NOT NULL,
            doctor_id INT NOT NULL,
            date DATE NOT NULL,
            time_slot INT NOT NULL,
            status INT DEFAULT 0,
            create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            modify_request_json JSONB,
            FOREIGN KEY (student_id) REFERENCES users(id) ON DELETE CASCADE,
            FOREIGN KEY (doctor_id) REFERENCES users(id) ON DELETE CASCADE
        );
    )";

    // 问卷模板表 (surveys)
    // content_json: 存储题目数组
    tables["surveys"] = R"(
        CREATE TABLE IF NOT EXISTS surveys (
            id SERIAL PRIMARY KEY,
            doctor_id INT NOT NULL,
            title VARCHAR(100) NOT NULL,
            content_json JSONB NOT NULL,
            create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (doctor_id) REFERENCES users(id) ON DELETE CASCADE
        );
    )";

    // 问卷回答表 (survey_answers)
    // answers_json: 存储学生提交的答案
    tables["survey_answers"] = R"(
        CREATE TABLE IF NOT EXISTS survey_answers (
            id SERIAL PRIMARY KEY,
            appt_id INT NOT NULL,
            student_id INT NOT NULL,
            answers_json JSONB NOT NULL,
            submit_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (appt_id) REFERENCES appointments(id) ON DELETE CASCADE,
            FOREIGN KEY (student_id) REFERENCES users(id) ON DELETE CASCADE
        );
    )";

    // 咨询记录/报告表 (consultation_records)
    tables["consultation_records"] = R"(
        CREATE TABLE IF NOT EXISTS consultation_records (
            id SERIAL PRIMARY KEY,
            appt_id INT NOT NULL,
            doctor_id INT NOT NULL,
            report_content TEXT,
            result_tags VARCHAR(255),
            create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (appt_id) REFERENCES appointments(id) ON DELETE CASCADE,
            FOREIGN KEY (doctor_id) REFERENCES users(id) ON DELETE CASCADE
        );
    )";

    bool allSuccess = true;
    QSqlQuery query(m_mainDb);

    for (auto it = tables.begin(); it != tables.end(); ++it) {
        if (!query.exec(it.value())) {
            qCritical() << "创建表失败 [" << it.key() << "]:" << query.lastError().text();
            allSuccess = false;
        }
    }

    // 创建 GIN 索引
    if (allSuccess) {        
        // 为问卷内容的 JSONB 创建 GIN 索引
        if (!query.exec("CREATE INDEX IF NOT EXISTS idx_surveys_content ON surveys USING GIN (content_json)")) {
            qWarning() << "警告: 创建 surveys GIN 索引失败:" << query.lastError().text();
        }

        // 为学生回答的 JSONB 创建 GIN 索引
        if (!query.exec("CREATE INDEX IF NOT EXISTS idx_survey_answers ON survey_answers USING GIN (answers_json)")) {
            qWarning() << "警告: 创建 survey_answers GIN 索引失败:" << query.lastError().text();
        }
    }

    // 为 users 表的 role 字段创建索引
    QString createIndexSql = "CREATE INDEX IF NOT EXISTS idx_users_role ON users(role)";
    
    if (!query.exec(createIndexSql)) {
        qWarning() << "警告: 创建 role 索引失败 (不影响主流程):" << query.lastError().text();
    }

    // 初始化默认数据
    if (allSuccess) {
        seedDefaultAdmin();
    }

    return allSuccess;
}

void DBManager::seedDefaultAdmin()
{
    QSqlQuery query(m_mainDb);
    
    // 检查是否已经存在管理员
    query.exec("SELECT count(*) FROM users WHERE role = 3");
    if (query.next() && query.value(0).toInt() > 0) {
        return;
    }

    qDebug() << "植入默认管理员账户...";

    QString rawPassword = "123456";
    QString clientHash = QString(QCryptographicHash::hash(rawPassword.toUtf8(), QCryptographicHash::Sha256).toHex());

    const int saltLength = 16;
    QByteArray saltData;
    saltData.resize(saltLength);
    // 使用 Qt 全局随机生成器填充数据
    QRandomGenerator::global()->fillRange(
        reinterpret_cast<quint32*>(saltData.data()), 
        saltLength / sizeof(quint32)
    );
    QString salt = QString(saltData.toHex());

    QString finalHash = QString(QCryptographicHash::hash((clientHash + salt).toUtf8(), QCryptographicHash::Sha256).toHex());

    query.prepare(
        "INSERT INTO users (username, password, salt, role, real_name) VALUES (:u, :p, :s, :r, :n)");
    
    query.bindValue(":u", "admin");
    query.bindValue(":p", finalHash);
    query.bindValue(":s", salt);
    query.bindValue(":r", 3);
    query.bindValue(":n", "System Admin");

    if (!query.exec()) {
        qWarning() << "植入管理员失败:" << query.lastError().text();
    } else {
        qDebug() << "默认管理员创建成功. 用户名: admin, 密码: 123456";
    }
}
