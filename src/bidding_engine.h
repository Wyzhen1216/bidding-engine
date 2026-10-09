#pragma once
#include "segment_tree.h"
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <iomanip>

/**
 * 竞价记录 - 记录每次竞价操作的完整信息
 */
struct BidRecord {
    int bidId;               // 竞价唯一ID
    std::string bidderId;    // 竞价者ID
    int price;               // 出价金额
    int start, end;          // 时间区间
    int slotId;              // 广告位编号
    std::vector<int> nodeIndices; // 线段树中存储该竞价的节点索引
    bool active;             // 是否有效
};

/**
 * 竞价引擎 - 管理多个广告位的竞价系统
 * 
 * 每个广告位独立维护一棵线段树
 * 支持区间竞价更新、单点/区间查询、全额撤价
 */
class BiddingEngine {
private:
    std::vector<SegmentTree> slots;     // 广告位数组,每个元素是一棵线段树
    std::map<int, BidRecord> bidRecords; // bidId -> 竞价记录
    int nextBidId;                      // 下一个竞价ID
    int numSlots;                       // 广告位数量
    bool traceEnabled;                  // 全局路径溯源开关

public:
    BiddingEngine(int numSlots = 4) : nextBidId(1), numSlots(numSlots), traceEnabled(false) {
        slots.reserve(numSlots);
        for (int i = 0; i < numSlots; i++) {
            slots.emplace_back();
        }
    }

    // 设置路径溯源
    void setTrace(bool enabled) {
        traceEnabled = enabled;
        for (auto& slot : slots) {
            slot.setTrace(enabled);
        }
    }

    // 区间竞价更新
    // 返回竞价ID (用于后续撤价)
    int placeBid(int slotId, int start, int end,
                 const std::string& bidder, int price) {
        if (slotId < 0 || slotId >= numSlots) return -1;
        if (start < 0 || end >= SegmentTree::MAXN || start > end) return -1;
        if (price <= 0) return -1;

        int bidId = nextBidId++;

        // 设置路径溯源
        slots[slotId].setTrace(traceEnabled);

        // 执行线段树区间更新
        std::vector<int> affectedNodes = slots[slotId].rangeUpdate(start, end, bidId, bidder, price);

        // 记录竞价记录
        BidRecord record;
        record.bidId = bidId;
        record.bidderId = bidder;
        record.price = price;
        record.start = start;
        record.end = end;
        record.slotId = slotId;
        record.nodeIndices = affectedNodes;
        record.active = true;
        bidRecords[bidId] = record;

        return bidId;
    }

    // 单点查询: 查询某分钟在某广告位的最高出价
    std::pair<int, std::vector<std::string>> queryPoint(int slotId, int t) {
        if (slotId < 0 || slotId >= numSlots) return {0, {}};
        if (t < 0 || t >= SegmentTree::MAXN) return {0, {}};

        slots[slotId].setTrace(traceEnabled);
        return slots[slotId].pointQuery(t);
    }

    // 区间查询: 查询某时间段在某广告位的最高出价
    std::pair<int, std::string> queryRange(int slotId, int start, int end) {
        if (slotId < 0 || slotId >= numSlots) return {0, ""};
        if (start < 0 || end >= SegmentTree::MAXN || start > end) return {0, ""};

        slots[slotId].setTrace(traceEnabled);
        return slots[slotId].rangeQuery(start, end);
    }

    // 全额撤价: 撤销指定竞价,恢复次高出价
    bool withdrawBid(int bidId) {
        auto it = bidRecords.find(bidId);
        if (it == bidRecords.end()) return false;
        if (!it->second.active) return false;

        BidRecord& record = it->second;
        record.active = false;

        // 在线段树中撤价
        slots[record.slotId].withdrawAtNodes(bidId, record.nodeIndices);

        return true;
    }

    // 获取竞价记录
    const BidRecord* getBidRecord(int bidId) const {
        auto it = bidRecords.find(bidId);
        if (it == bidRecords.end()) return nullptr;
        return &it->second;
    }

    // 获取所有活跃竞价记录
    std::vector<const BidRecord*> getActiveBids() const {
        std::vector<const BidRecord*> result;
        for (const auto& pair : bidRecords) {
            if (pair.second.active) {
                result.push_back(&pair.second);
            }
        }
        return result;
    }

    // 获取路径溯源日志
    std::vector<std::string> getTraceLog(int slotId) const {
        if (slotId < 0 || slotId >= numSlots) return {};
        return slots[slotId].getTraceLog();
    }

    // 获取广告位数量
    int getNumSlots() const { return numSlots; }

    // 获取广告位可视化数据
    // 返回: 每个广告位 -> 每个分钟点的(最高价, 胜出者列表)
    std::vector<std::vector<std::pair<int, std::vector<std::string>>>> getVisualizationData() {
        std::vector<std::vector<std::pair<int, std::vector<std::string>>>> data;
        for (int s = 0; s < numSlots; s++) {
            data.push_back(slots[s].getAllLeafStates());
        }
        return data;
    }

    // 获取统计信息
    std::string getStatistics() const {
        int totalBids = 0;
        int activeBids = 0;
        int withdrawnBids = 0;
        std::map<std::string, int> bidderCount;

        for (const auto& pair : bidRecords) {
            totalBids++;
            if (pair.second.active) {
                activeBids++;
                bidderCount[pair.second.bidderId]++;
            } else {
                withdrawnBids++;
            }
        }

        std::ostringstream oss;
        oss << "总竞价数: " << totalBids << "\n";
        oss << "活跃竞价: " << activeBids << "\n";
        oss << "已撤竞价: " << withdrawnBids << "\n";
        oss << "竞价者数量: " << bidderCount.size() << "\n";
        oss << "广告位数量: " << numSlots << "\n";

        return oss.str();
    }

    // 时间转可读格式
    static std::string timeToStr(int minutes) {
        return SegmentTree::timeToStr(minutes);
    }
};
