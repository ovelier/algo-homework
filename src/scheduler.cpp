#include "scheduler.h"
#include <algorithm>
#include <iostream>

Scheduler::Scheduler(Database* db) : database(db) {}

std::vector<TimeSlot> Scheduler::getAllPossibleSlots() {
    std::vector<TimeSlot> slots;
    // 两周(第9周和第10周), 每周5天(周一到周五), 每天2个时段(上午/下午)
    for (int week = 9; week <= 10; week++) {
        for (int day = 0; day < 5; day++) {  // 0-4 表示周一到周五
            for (int period = 0; period < 2; period++) {  // 0-上午, 1-下午
                slots.push_back({week, day, period});
            }
        }
    }
    return slots;
}

bool Scheduler::isSlotExcluded(const TimeSlot& slot, const std::vector<TimeSlot>& excludedSlots) {
    return std::find(excludedSlots.begin(), excludedSlots.end(), slot) != excludedSlots.end();
}

bool Scheduler::isLabAvailable(int labId, const TimeSlot& slot) {
    auto it = labOccupancy.find(labId);
    if (it == labOccupancy.end()) {
        return true;  // 该实验室还没有任何占用
    }
    return it->second.find(slot) == it->second.end();
}

void Scheduler::markLabOccupied(int labId, const TimeSlot& slot) {
    labOccupancy[labId].insert(slot);
}

bool Scheduler::allocateRequest(const LabRequest& request, const std::vector<Laboratory>& labs) {
    // 阶段1: 优先尝试分配到期望的时间段
    for (const auto& preferredSlot : request.preferredSlots) {
        // 检查是否在排除列表中
        if (isSlotExcluded(preferredSlot, request.excludedSlots)) {
            continue;
        }
        
        // 遍历所有实验室,寻找合适的实验室
        for (const auto& lab : labs) {
            // 检查容量是否满足
            if (lab.capacity < request.studentCount) {
                continue;
            }
            
            // 检查实验室是否可用
            if (!isLabAvailable(lab.id, preferredSlot)) {
                continue;
            }
            
            // 找到合适的实验室和时间段,进行分配
            Schedule schedule;
            schedule.requestId = request.id;
            schedule.labId = lab.id;
            schedule.timeSlot = preferredSlot;
            
            if (database->addSchedule(schedule)) {
                markLabOccupied(lab.id, preferredSlot);
                std::cout << "成功分配: 班级 " << request.classId 
                          << " -> 实验室 " << lab.location 
                          << " (第" << preferredSlot.week << "周 "
                          << "周" << (preferredSlot.day + 1) << " "
                          << (preferredSlot.period == 0 ? "上午" : "下午") << ")" << std::endl;
                return true;
            }
        }
    }
    
    // 阶段2: 如果期望时间段都无法满足,尝试其他可用时间段
    std::vector<TimeSlot> allSlots = getAllPossibleSlots();
    for (const auto& slot : allSlots) {
        // 跳过排除的时间段
        if (isSlotExcluded(slot, request.excludedSlots)) {
            continue;
        }
        
        // 跳过已经尝试过的期望时间段
        if (std::find(request.preferredSlots.begin(), request.preferredSlots.end(), slot) 
            != request.preferredSlots.end()) {
            continue;
        }
        
        // 遍历所有实验室
        for (const auto& lab : labs) {
            if (lab.capacity < request.studentCount) {
                continue;
            }
            
            if (!isLabAvailable(lab.id, slot)) {
                continue;
            }
            
            // 找到可用的实验室和时间段
            Schedule schedule;
            schedule.requestId = request.id;
            schedule.labId = lab.id;
            schedule.timeSlot = slot;
            
            if (database->addSchedule(schedule)) {
                markLabOccupied(lab.id, slot);
                std::cout << "备选分配: 班级 " << request.classId 
                          << " -> 实验室 " << lab.location 
                          << " (第" << slot.week << "周 "
                          << "周" << (slot.day + 1) << " "
                          << (slot.period == 0 ? "上午" : "下午") << ")" << std::endl;
                return true;
            }
        }
    }
    
    // 无法为该申请分配合适的时间段和实验室
    std::cout << "分配失败: 班级 " << request.classId << " (教师: " << request.teacher << ")" << std::endl;
    return false;
}

int Scheduler::generateSchedule() {
    // 1. 清空旧的课程安排
    database->clearSchedules();
    labOccupancy.clear();
    
    // 2. 获取所有实验室和申请
    std::vector<Laboratory> labs = database->getAllLaboratories();
    std::vector<LabRequest> requests = database->getAllRequests();
    
    if (labs.empty()) {
        std::cerr << "错误: 没有可用的实验室!" << std::endl;
        return 0;
    }
    
    if (requests.empty()) {
        std::cerr << "提示: 没有待处理的申请。" << std::endl;
        return 0;
    }
    
    std::cout << "\n========== 开始生成课程安排 ==========" << std::endl;
    std::cout << "可用实验室数量: " << labs.size() << std::endl;
    std::cout << "待处理申请数量: " << requests.size() << std::endl;
    std::cout << "====================================\n" << std::endl;
    
    // 3. 申请已按priority排序(在数据库查询时已排序)
    
    // 4. 对每个申请进行分配
    int successCount = 0;
    for (const auto& request : requests) {
        if (allocateRequest(request, labs)) {
            successCount++;
        }
    }
    
    std::cout << "\n========== 课程安排生成完成 ==========" << std::endl;
    std::cout << "成功分配: " << successCount << " / " << requests.size() << std::endl;
    std::cout << "成功率: " << (successCount * 100.0 / requests.size()) << "%" << std::endl;
    std::cout << "====================================\n" << std::endl;
    
    return successCount;
}

Scheduler::ScheduleStats Scheduler::getScheduleStats() {
    ScheduleStats stats;
    
    std::vector<LabRequest> allRequests = database->getAllRequests();
    std::vector<Schedule> allSchedules = database->getAllSchedules();
    
    stats.totalRequests = allRequests.size();
    
    // 统计成功分配的申请
    std::set<int> scheduledRequestIds;
    for (const auto& schedule : allSchedules) {
        scheduledRequestIds.insert(schedule.requestId);
    }
    
    stats.successfulRequests = scheduledRequestIds.size();
    stats.failedRequests = stats.totalRequests - stats.successfulRequests;
    stats.successRate = stats.totalRequests > 0 ? 
        (stats.successfulRequests * 100.0 / stats.totalRequests) : 0.0;
    
    // 找出失败的班级
    for (const auto& request : allRequests) {
        if (scheduledRequestIds.find(request.id) == scheduledRequestIds.end()) {
            stats.failedClasses.push_back(request.classId + " (" + request.teacher + ")");
        }
    }
    
    return stats;
}
