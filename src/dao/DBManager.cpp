#include "DBManager.h"

#include <QCryptographicHash>

#include "core/ConfigManager.h"

DBManager& DBManager::instance() {
    static DBManager instance;
    return instance;
}

DBManager::DBManager() {}

DBManager::~DBManager() {
    if (m_mainDb.isOpen()) {
        m_mainDb.close();
    }
}

QSqlDatabase DBManager::getMainDatabase() const {
    return m_mainDb;
}

bool DBManager::connectToDatabase() {
    // 获取配置
    const DBConfig &config = ConfigManager::instance().db();
    
    m_dbName = config.dbName;
    
    // 检查 PostgreSQL 驱动
    if (!QSqlDatabase::isDriverAvailable("QPSQL")) {
        qCritical() << "Error: QPSQL driver not loaded! Please install libpq and Qt PSQL plugin.";
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
                    qDebug() << "Creating database:" << config.dbName;
                    if (!query.exec(QString("CREATE DATABASE \"%1\"").arg(config.dbName))) {
                        qCritical() << "Failed to create database:" << query.lastError().text();
                    }
                }
            } // 销毁query对象, 方便后面安全关闭连接
            tempDb.close();
        } else {
            qCritical() << "Failed to connect to Postgres server (db=postgres):" << tempDb.lastError().text();
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
        qCritical() << "Main DB Connection Failed:" << m_mainDb.lastError().text();
        return false;
    }
    qDebug() << "Connected to PostgreSQL database:" << config.dbName;
    return true;
}

QSqlDatabase DBManager::openThreadConnection(QString &connectionName) {
    // 生成唯一的连接名
    connectionName = QString("ThreadConn_%1").arg(QUuid::createUuid().toString());
    
    // 添加数据库
    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", connectionName);
    
    // 读取配置
    const DBConfig &config = ConfigManager::instance().db();
    
    db.setHostName(config.host);
    db.setPort(config.port);
    db.setUserName(config.username);
    db.setPassword(config.password);
    db.setDatabaseName(config.dbName);
    
    if (!db.open()) {
        qWarning() << "Thread DB Open Failed:" << db.lastError().text();
    }
    
    return db;
}

void DBManager::closeThreadConnection(const QString &connectionName) {
    // 必须先让 QSqlDatabase 对象超出作用域或不再被持有，才能 removeDatabase
    {
        QSqlDatabase db = QSqlDatabase::database(connectionName);
        if (db.isOpen()) {
            db.close();
        }
    } // db 在此处析构
    
    // 移除连接定义
    QSqlDatabase::removeDatabase(connectionName);
}

bool DBManager::initTables() {
    bool success = true;

    // 用户表 (users)
    // role: 1=Student, 2=Doctor, 3=Admin
    success &= createTable("users", R"(
        CREATE TABLE IF NOT EXISTS users (
            id SERIAL PRIMARY KEY,
            username VARCHAR(50) NOT NULL UNIQUE,
            password VARCHAR(255) NOT NULL,
            role INT NOT NULL,
            real_name VARCHAR(50),
            gender VARCHAR(10),
            age INT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )");

    // 医生信息表 (doctor_info)
    // 扩展 users 表，存储医生特有信息
    success &= createTable("doctor_info", R"(
        CREATE TABLE IF NOT EXISTS doctor_info (
            user_id INT PRIMARY KEY,
            intro TEXT,
            specialized_field VARCHAR(255),
            avatar_path VARCHAR(255),
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        );
    )");

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
    success &= createTable("schedules", R"(
        CREATE TABLE IF NOT EXISTS schedules (
            id SERIAL PRIMARY KEY,
            doctor_id INT NOT NULL,
            date DATE NOT NULL,
            time_slot_flags INT DEFAULT 0,
            status INT DEFAULT 1, 
            FOREIGN KEY (doctor_id) REFERENCES users(id) ON DELETE CASCADE,
            UNIQUE (doctor_id, date)
        );
    )");

    // 预约记录表 (appointments)
    // status: 0=Pending, 1=Confirmed, 2=Completed, 3=Cancelled
    success &= createTable("appointments", R"(
        CREATE TABLE IF NOT EXISTS appointments (
            id SERIAL PRIMARY KEY,
            student_id INT NOT NULL,
            doctor_id INT NOT NULL,
            date DATE NOT NULL,
            time_slot INT NOT NULL,
            status INT DEFAULT 0,
            create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            modify_request_json JSON,
            FOREIGN KEY (student_id) REFERENCES users(id) ON DELETE CASCADE,
            FOREIGN KEY (doctor_id) REFERENCES users(id) ON DELETE CASCADE
        );
    )");

    // 问卷模板表 (surveys)
    // content_json: 存储题目数组
    success &= createTable("surveys", R"(
        CREATE TABLE IF NOT EXISTS surveys (
            id SERIAL PRIMARY KEY,
            doctor_id INT NOT NULL,
            title VARCHAR(100) NOT NULL,
            content_json JSON NOT NULL,
            create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (doctor_id) REFERENCES users(id) ON DELETE CASCADE
        );
    )");

    // 问卷回答表 (survey_answers)
    // answers_json: 存储学生提交的答案
    success &= createTable("survey_answers", R"(
        CREATE TABLE IF NOT EXISTS survey_answers (
            id SERIAL PRIMARY KEY,
            appt_id INT NOT NULL,
            student_id INT NOT NULL,
            answers_json JSON NOT NULL,
            submit_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (appt_id) REFERENCES appointments(id) ON DELETE CASCADE,
            FOREIGN KEY (student_id) REFERENCES users(id) ON DELETE CASCADE
        );
    )");

    // 咨询记录/报告表 (consultation_records)
    success &= createTable("consultation_records", R"(
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
    )");

    // 初始化默认数据
    if (success) {
        seedDefaultAdmin();
    }

    return success;
}

bool DBManager::createTable(const QString &tableName, const QString &sql) {
    QSqlQuery query(m_mainDb);
    if (!query.exec(sql)) {
        qCritical() << "Failed to create table" << tableName << ":" << query.lastError().text();
        return false;
    }
    return true;
}

void DBManager::seedDefaultAdmin() {
    QSqlQuery query(m_mainDb);
    query.exec("SELECT count(*) FROM users WHERE role = 3"); 
    if (query.next() && query.value(0).toInt() > 0) {
        return; 
    }

    qDebug() << "Seeding default admin account...";
    query.prepare("INSERT INTO users (username, password, role, real_name) VALUES (:u, :p, :r, :n)");
    query.bindValue(":u", "admin");
    QString hashedPassword = QString(QCryptographicHash::hash("123456", QCryptographicHash::Sha256).toHex());
    query.bindValue(":p", hashedPassword);
    query.bindValue(":r", 3);
    query.bindValue(":n", "System Admin");
    
    if (!query.exec()) {
        qWarning() << "Failed to seed admin:" << query.lastError().text();
    } else {
        qDebug() << "Default Admin created. User: admin, Pass: 123456";
    }
}
