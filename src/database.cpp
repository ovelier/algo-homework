#include "database.h"
#include <sstream>
#include <iostream>

Database::Database(const std::string& dbPath) : db(nullptr), dbPath(dbPath) {}

Database::~Database() {
    if (db) {
        sqlite3_close(db);
    }
}

bool Database::initialize() {
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    // 创建实验室表
    std::string createLabTable = R"(
        CREATE TABLE IF NOT EXISTS laboratories (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            location TEXT NOT NULL,
            capacity INTEGER NOT NULL
        );
    )";
    
    // 创建申请表
    std::string createRequestTable = R"(
        CREATE TABLE IF NOT EXISTS requests (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            class_id TEXT NOT NULL,
            student_count INTEGER NOT NULL,
            teacher TEXT NOT NULL,
            preferred_slots TEXT NOT NULL,
            excluded_slots TEXT NOT NULL,
            priority INTEGER NOT NULL
        );
    )";
    
    // 创建课程安排表
    std::string createScheduleTable = R"(
        CREATE TABLE IF NOT EXISTS schedules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            request_id INTEGER NOT NULL,
            lab_id INTEGER NOT NULL,
            week INTEGER NOT NULL,
            day INTEGER NOT NULL,
            period INTEGER NOT NULL,
            FOREIGN KEY (request_id) REFERENCES requests(id),
            FOREIGN KEY (lab_id) REFERENCES laboratories(id)
        );
    )";
    
    return executeSQL(createLabTable) && 
           executeSQL(createRequestTable) && 
           executeSQL(createScheduleTable);
}

bool Database::executeSQL(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL 错误: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

std::string Database::serializeTimeSlots(const std::vector<TimeSlot>& slots) {
    std::ostringstream oss;
    for (size_t i = 0; i < slots.size(); i++) {
        if (i > 0) oss << ";";
        oss << slots[i].week << "," << slots[i].day << "," << slots[i].period;
    }
    return oss.str();
}

std::vector<TimeSlot> Database::deserializeTimeSlots(const std::string& data) {
    std::vector<TimeSlot> slots;
    if (data.empty()) return slots;
    
    std::istringstream iss(data);
    std::string slot;
    while (std::getline(iss, slot, ';')) {
        std::istringstream slotStream(slot);
        TimeSlot ts;
        char comma;
        slotStream >> ts.week >> comma >> ts.day >> comma >> ts.period;
        slots.push_back(ts);
    }
    return slots;
}

// 实验室管理
bool Database::addLaboratory(const std::string& location, int capacity) {
    std::string sql = "INSERT INTO laboratories (location, capacity) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, location.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, capacity);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool Database::deleteLaboratory(int id) {
    std::string sql = "DELETE FROM laboratories WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

std::vector<Laboratory> Database::getAllLaboratories() {
    std::vector<Laboratory> labs;
    std::string sql = "SELECT id, location, capacity FROM laboratories;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return labs;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Laboratory lab;
        lab.id = sqlite3_column_int(stmt, 0);
        lab.location = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        lab.capacity = sqlite3_column_int(stmt, 2);
        labs.push_back(lab);
    }
    
    sqlite3_finalize(stmt);
    return labs;
}

Laboratory Database::getLaboratory(int id) {
    Laboratory lab = {0, "", 0};
    std::string sql = "SELECT id, location, capacity FROM laboratories WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return lab;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        lab.id = sqlite3_column_int(stmt, 0);
        lab.location = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        lab.capacity = sqlite3_column_int(stmt, 2);
    }
    
    sqlite3_finalize(stmt);
    return lab;
}

// 申请管理
bool Database::addRequest(const LabRequest& request) {
    std::string sql = "INSERT INTO requests (class_id, student_count, teacher, preferred_slots, excluded_slots, priority) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    std::string preferredStr = serializeTimeSlots(request.preferredSlots);
    std::string excludedStr = serializeTimeSlots(request.excludedSlots);
    
    sqlite3_bind_text(stmt, 1, request.classId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, request.studentCount);
    sqlite3_bind_text(stmt, 3, request.teacher.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, preferredStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, excludedStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, request.priority);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool Database::deleteRequest(int id) {
    std::string sql = "DELETE FROM requests WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

std::vector<LabRequest> Database::getAllRequests() {
    std::vector<LabRequest> requests;
    std::string sql = "SELECT id, class_id, student_count, teacher, preferred_slots, excluded_slots, priority FROM requests ORDER BY priority;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return requests;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        LabRequest req;
        req.id = sqlite3_column_int(stmt, 0);
        req.classId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        req.studentCount = sqlite3_column_int(stmt, 2);
        req.teacher = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        req.preferredSlots = deserializeTimeSlots(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        req.excludedSlots = deserializeTimeSlots(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
        req.priority = sqlite3_column_int(stmt, 6);
        requests.push_back(req);
    }
    
    sqlite3_finalize(stmt);
    return requests;
}

LabRequest Database::getRequest(int id) {
    LabRequest req = {0, "", 0, "", {}, {}, 0};
    std::string sql = "SELECT id, class_id, student_count, teacher, preferred_slots, excluded_slots, priority FROM requests WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return req;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        req.id = sqlite3_column_int(stmt, 0);
        req.classId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        req.studentCount = sqlite3_column_int(stmt, 2);
        req.teacher = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        req.preferredSlots = deserializeTimeSlots(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        req.excludedSlots = deserializeTimeSlots(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
        req.priority = sqlite3_column_int(stmt, 6);
    }
    
    sqlite3_finalize(stmt);
    return req;
}

// 课程安排管理
bool Database::clearSchedules() {
    return executeSQL("DELETE FROM schedules;");
}

bool Database::addSchedule(const Schedule& schedule) {
    std::string sql = "INSERT INTO schedules (request_id, lab_id, week, day, period) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, schedule.requestId);
    sqlite3_bind_int(stmt, 2, schedule.labId);
    sqlite3_bind_int(stmt, 3, schedule.timeSlot.week);
    sqlite3_bind_int(stmt, 4, schedule.timeSlot.day);
    sqlite3_bind_int(stmt, 5, schedule.timeSlot.period);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

std::vector<Schedule> Database::getAllSchedules() {
    std::vector<Schedule> schedules;
    std::string sql = "SELECT id, request_id, lab_id, week, day, period FROM schedules;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return schedules;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Schedule sch;
        sch.id = sqlite3_column_int(stmt, 0);
        sch.requestId = sqlite3_column_int(stmt, 1);
        sch.labId = sqlite3_column_int(stmt, 2);
        sch.timeSlot.week = sqlite3_column_int(stmt, 3);
        sch.timeSlot.day = sqlite3_column_int(stmt, 4);
        sch.timeSlot.period = sqlite3_column_int(stmt, 5);
        schedules.push_back(sch);
    }
    
    sqlite3_finalize(stmt);
    return schedules;
}

std::vector<Schedule> Database::getSchedulesByLab(int labId) {
    std::vector<Schedule> schedules;
    std::string sql = "SELECT id, request_id, lab_id, week, day, period FROM schedules WHERE lab_id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return schedules;
    }
    
    sqlite3_bind_int(stmt, 1, labId);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Schedule sch;
        sch.id = sqlite3_column_int(stmt, 0);
        sch.requestId = sqlite3_column_int(stmt, 1);
        sch.labId = sqlite3_column_int(stmt, 2);
        sch.timeSlot.week = sqlite3_column_int(stmt, 3);
        sch.timeSlot.day = sqlite3_column_int(stmt, 4);
        sch.timeSlot.period = sqlite3_column_int(stmt, 5);
        schedules.push_back(sch);
    }
    
    sqlite3_finalize(stmt);
    return schedules;
}

std::vector<Schedule> Database::getSchedulesByClass(const std::string& classId) {
    std::vector<Schedule> schedules;
    std::string sql = R"(
        SELECT s.id, s.request_id, s.lab_id, s.week, s.day, s.period 
        FROM schedules s 
        JOIN requests r ON s.request_id = r.id 
        WHERE r.class_id = ?;
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return schedules;
    }
    
    sqlite3_bind_text(stmt, 1, classId.c_str(), -1, SQLITE_TRANSIENT);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Schedule sch;
        sch.id = sqlite3_column_int(stmt, 0);
        sch.requestId = sqlite3_column_int(stmt, 1);
        sch.labId = sqlite3_column_int(stmt, 2);
        sch.timeSlot.week = sqlite3_column_int(stmt, 3);
        sch.timeSlot.day = sqlite3_column_int(stmt, 4);
        sch.timeSlot.period = sqlite3_column_int(stmt, 5);
        schedules.push_back(sch);
    }
    
    sqlite3_finalize(stmt);
    return schedules;
}

bool Database::clearAllData() {
    return executeSQL("DELETE FROM schedules;") &&
           executeSQL("DELETE FROM requests;") &&
           executeSQL("DELETE FROM laboratories;");
}
