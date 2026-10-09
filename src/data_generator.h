#pragma once
#include "bidding_engine.h"
#include <random>
#include <sstream>
#include <iomanip>

/**
 * 竞价数据模拟生成器
 * 
 * 生成至少5000条交错、重叠的竞价数据
 * 覆盖场景: 时间重叠(40%)、价格竞争(30%)、撤价(20%)、并发竞价(10%)
 */
class DataGenerator {
private:
    std::mt19937 rng;  // 随机数生成器

    // 生成随机竞价者ID
    std::string randomBidder() {
        static const std::string prefixes[] = {
            "Adv_A", "Adv_B", "Adv_C", "Adv_D", "Adv_E",
            "Adv_F", "Adv_G", "Adv_H", "Adv_I", "Adv_J",
            "Adv_K", "Adv_L", "Adv_M", "Adv_N", "Adv_O"
        };
        static const int numPrefixes = 15;
        return prefixes[rng() % numPrefixes];
    }

    // 生成随机时间区间
    std::pair<int, int> randomTimeRange() {
        int start = rng() % 1440;
        int duration = 1 + rng() % 180; // 1分钟到3小时
        int end = std::min(start + duration, 1439);
        return {start, end};
    }

    // 生成随机价格
    int randomPrice() {
        // 价格范围 50-2000, 偏向100-500
        int r = rng() % 100;
        if (r < 60) return 100 + rng() % 400;       // 100-499 (常见)
        else if (r < 85) return 500 + rng() % 500;   // 500-999 (较高)
        else if (r < 95) return 1000 + rng() % 500;  // 1000-1499 (高价)
        else return 1500 + rng() % 501;               // 1500-2000 (极高)
    }

public:
    DataGenerator(unsigned int seed = 42) : rng(seed) {}

    /**
     * 生成竞价数据并导入引擎
     * 返回生成的竞价ID列表
     */
    std::vector<int> generate(BiddingEngine& engine, int totalBids = 5000) {
        std::vector<int> bidIds;
        bidIds.reserve(totalBids);

        int numSlots = engine.getNumSlots();

        // 用于撤价测试: 记录部分竞价ID
        std::vector<int> forWithdraw;

        // 阶段1: 时间重叠场景 (40% = 2000条)
        int overlapCount = totalBids * 2 / 5;
        for (int i = 0; i < overlapCount; i++) {
            int slotId = rng() % numSlots;
            // 生成集中在热门时段(8:00-22:00)的竞价,增加重叠度
            int start = 480 + rng() % 840;  // 8:00-22:00
            int duration = 10 + rng() % 120; // 10分钟到2小时
            int end = std::min(start + duration, 1439);
            std::string bidder = randomBidder();
            int price = randomPrice();

            int bidId = engine.placeBid(slotId, start, end, bidder, price);
            if (bidId > 0) {
                bidIds.push_back(bidId);
                if (rng() % 5 == 0) {  // 20%概率标记为可撤价
                    forWithdraw.push_back(bidId);
                }
            }
        }

        // 阶段2: 价格竞争场景 (30% = 1500条)
        // 在同一时段内反复加价
        int competeCount = totalBids * 3 / 10;
        for (int i = 0; i < competeCount; i++) {
            int slotId = rng() % numSlots;
            int start = 480 + rng() % 840;
            int duration = 5 + rng() % 60;  // 较短时段,增加竞争密度
            int end = std::min(start + duration, 1439);
            std::string bidder = randomBidder();
            // 价格逐步攀升
            int basePrice = 100 + (i % 20) * 50;
            int price = basePrice + rng() % 200;

            int bidId = engine.placeBid(slotId, start, end, bidder, price);
            if (bidId > 0) {
                bidIds.push_back(bidId);
                if (rng() % 5 == 0) {
                    forWithdraw.push_back(bidId);
                }
            }
        }

        // 阶段3: 撤价场景 (20% = 1000条)
        // 先竞价后撤价
        int withdrawCount = totalBids / 5;
        for (int i = 0; i < withdrawCount; i++) {
            int slotId = rng() % numSlots;
            auto [start, end] = randomTimeRange();
            std::string bidder = randomBidder();
            int price = randomPrice();

            int bidId = engine.placeBid(slotId, start, end, bidder, price);
            if (bidId > 0) {
                bidIds.push_back(bidId);
                // 较高概率撤价
                if (rng() % 3 == 0) {
                    forWithdraw.push_back(bidId);
                }
            }
        }

        // 阶段4: 并发竞价场景 (10% = 500条)
        // 同一时段多个竞价者同时竞价
        int concurrentCount = totalBids / 10;
        for (int i = 0; i < concurrentCount; i++) {
            int slotId = rng() % numSlots;
            auto [start, end] = randomTimeRange();
            int samePrice = 200 + rng() % 300;  // 相同价格

            // 2-4个竞价者同时出相同价格
            int numBidders = 2 + rng() % 3;
            for (int j = 0; j < numBidders; j++) {
                std::string bidder = randomBidder();
                int price = samePrice + (rng() % 2 == 0 ? 0 : rng() % 50); // 微小差异
                int bidId = engine.placeBid(slotId, start, end, bidder, price);
                if (bidId > 0) {
                    bidIds.push_back(bidId);
                }
            }
        }

        // 执行部分撤价操作
        int withdrawExecCount = std::min((int)forWithdraw.size(), totalBids / 10);
        for (int i = 0; i < withdrawExecCount; i++) {
            int idx = rng() % forWithdraw.size();
            engine.withdrawBid(forWithdraw[idx]);
        }

        return bidIds;
    }

    /**
     * 生成可视化的JSON数据
     * 引擎先运行,再提取数据生成JSON
     */
    std::string generateVisualizationJSON(BiddingEngine& engine) {
        std::ostringstream oss;
        oss << "{\n";

        // 广告位数据
        oss << "  \"slots\": [\n";
        for (int s = 0; s < engine.getNumSlots(); s++) {
            oss << "    {\n";
            oss << "      \"slotId\": " << s << ",\n";
            oss << "      \"slotName\": \"广告位" << (s + 1) << "\",\n";
            oss << "      \"timeline\": [\n";

            // 获取每个分钟点的状态
            for (int t = 0; t < SegmentTree::MAXN; t++) {
                auto [price, winners] = engine.queryPoint(s, t);
                oss << "        {\"time\": " << t 
                    << ", \"price\": " << price
                    << ", \"winner\": \"" << (winners.empty() ? "" : winners[0]) << "\"}";
                if (t < SegmentTree::MAXN - 1) oss << ",";
                oss << "\n";
            }

            oss << "      ]\n";
            oss << "    }";
            if (s < engine.getNumSlots() - 1) oss << ",";
            oss << "\n";
        }
        oss << "  ],\n";

        // 竞价者颜色映射
        oss << "  \"bidderColors\": {\n";
        std::map<std::string, std::string> colorMap;
        static const std::string colors[] = {
            "#FF6B6B", "#4ECDC4", "#45B7D1", "#96CEB4", "#FFEAA7",
            "#DDA0DD", "#98D8C8", "#F7DC6F", "#BB8FCE", "#85C1E9",
            "#F1948A", "#82E0AA", "#F8C471", "#AED6F1", "#D7BDE2"
        };
        for (int i = 0; i < 15; i++) {
            std::string bidder = "Adv_" + std::string(1, 'A' + i);
            colorMap[bidder] = colors[i];
        }
        bool first = true;
        for (const auto& [bidder, color] : colorMap) {
            if (!first) oss << ",\n";
            oss << "    \"" << bidder << "\": \"" << color << "\"";
            first = false;
        }
        oss << "\n  },\n";

        // 统计信息
        oss << "  \"statistics\": \"" << engine.getStatistics() << "\"\n";
        oss << "}";

        return oss.str();
    }
};
