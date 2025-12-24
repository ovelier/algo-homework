#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "database.h"
#include <map>
#include <set>
#include <vector>

/**
 * @brief 实验室调度算法类
 * 
 * 核心算法思想：
 * 1. 优先级贪心分配：按照申请优先级(申请时间顺序)进行分配
 * 2. 多阶段匹配策略：
 *    - 第一阶段：优先满足期望时间段(preferred slots)
 *    - 第二阶段：如果期望时间无法满足,尝试其他可用时间段
 * 3. 容量约束检查：确保实验室容量能够容纳班级人数
 * 4. 时间冲突检查：避免同一实验室同一时间段重复分配
 * 5. 排除时间段过滤：过滤掉教师不可用的时间段
 */
class Scheduler {
public:
    Scheduler(Database* db);
    
    /**
     * @brief 生成课程安排
     * @return 成功分配的申请数量
     * 
     * 算法流程：
     * 1. 清空旧的课程安排
     * 2. 获取所有实验室和申请
     * 3. 按优先级排序申请(先申请先满足)
     * 4. 对每个申请:
     *    a. 首先尝试分配到期望的时间段
     *    b. 如果期望时间段无法满足,尝试其他可用时间段
     *    c. 选择能容纳该班级的实验室
     *    d. 避免时间冲突
     * 5. 将成功的分配结果写入数据库
     */
    int generateSchedule();
    
    /**
     * @brief 获取调度统计信息
     */
    struct ScheduleStats {
        int totalRequests;      // 总申请数
        int successfulRequests; // 成功分配的申请数
        int failedRequests;     // 失败的申请数
        double successRate;     // 成功率
        std::vector<std::string> failedClasses; // 失败的班级列表
    };
    
    ScheduleStats getScheduleStats();
    
private:
    Database* database;
    
    // 实验室占用情况: lab_id -> set of occupied time slots
    std::map<int, std::set<TimeSlot>> labOccupancy;
    
    /**
     * @brief 尝试为申请分配实验室
     * @param request 实验申请
     * @param labs 可用实验室列表
     * @return 是否成功分配
     * 
     * 算法详细步骤：
     * 1. 首先尝试期望时间段(优先级最高)
     * 2. 对于每个期望时间段:
     *    - 遍历所有实验室
     *    - 检查容量是否满足
     *    - 检查时间段是否已被占用
     *    - 如果找到合适的实验室,分配并返回true
     * 3. 如果期望时间段都无法满足,尝试所有可用时间段
     * 4. 排除不可用时间段(excluded slots)
     * 5. 返回分配结果
     */
    bool allocateRequest(const LabRequest& request, const std::vector<Laboratory>& labs);
    
    /**
     * @brief 检查实验室在特定时间段是否可用
     */
    bool isLabAvailable(int labId, const TimeSlot& slot);
    
    /**
     * @brief 标记实验室时间段为已占用
     */
    void markLabOccupied(int labId, const TimeSlot& slot);
    
    /**
     * @brief 获取所有可能的时间段(两周,每周5天,每天2个时段)
     */
    std::vector<TimeSlot> getAllPossibleSlots();
    
    /**
     * @brief 检查时间段是否在排除列表中
     */
    bool isSlotExcluded(const TimeSlot& slot, const std::vector<TimeSlot>& excludedSlots);
};

#endif // SCHEDULER_H
