#pragma once
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <iomanip>

/**
 * 线段树 - 支持区间竞价更新、单点/区间查询、全额撤价
 * 
 * 设计原理:
 * - 全天1440分钟映射到1440个叶子节点
 * - 每个节点代表一个时间区间[start, end]
 * - 区间竞价更新通过"懒惰分解"将区间拆分为O(log n)个节点
 * - 每个节点存储直接分配到该区间的竞价记录
 * - overallMaxBid维护该区间(含子区间)的最高出价
 */
class SegmentTree {
public:
    static const int MAXN = 1440;  // 全天1440分钟

private:
    // 竞价条目 - 存储在线段树节点中
    struct BidEntry {
        int bidId;           // 竞价唯一ID
        std::string bidderId; // 竞价者ID
        int price;           // 出价金额
        bool active;         // 是否有效(撤价后置为false)
    };

    // 线段树节点
    struct Node {
        int start, end;                       // 区间范围
        std::vector<BidEntry> bids;           // 直接分配到该节点的竞价记录
        int directMaxBid;                     // 当前直接竞价中的最高价
        std::vector<std::string> directMaxBidders; // 最高价竞价者列表
        int overallMaxBid;                    // 该区间内的最高出价(含子区间)
        std::string overallMaxBidder;         // 该区间内最高出价者(取一个代表)
        bool lazyMarked;                      // 懒惰分解标记: 该节点直接存储了区间竞价
    };

    Node tree[4 * MAXN + 10];  // 线段树数组存储
    std::vector<std::string> traceLog;  // 路径溯源日志
    bool enableTrace;                  // 是否启用路径溯源

    // 建树
    void build(int idx, int l, int r) {
        tree[idx].start = l;
        tree[idx].end = r;
        tree[idx].directMaxBid = 0;
        tree[idx].overallMaxBid = 0;
        tree[idx].overallMaxBidder = "";
        tree[idx].lazyMarked = false;
        if (l == r) return;
        int mid = (l + r) / 2;
        build(idx * 2, l, mid);
        build(idx * 2 + 1, mid + 1, r);
    }

    // 用节点的直接竞价更新区间查询结果
    void considerDirectMax(int idx, int& maxPrice, std::string& winner) {
        if (tree[idx].directMaxBid > maxPrice) {
            maxPrice = tree[idx].directMaxBid;
            winner = tree[idx].directMaxBidders.empty()
                ? "" : tree[idx].directMaxBidders[0];
        }
    }

    // 重新计算节点的直接最高价
    void recalcDirect(int idx) {
        tree[idx].directMaxBid = 0;
        tree[idx].directMaxBidders.clear();
        tree[idx].lazyMarked = false;
        for (auto& entry : tree[idx].bids) {
            if (!entry.active) continue;
            tree[idx].lazyMarked = true;
            if (entry.price > tree[idx].directMaxBid) {
                tree[idx].directMaxBid = entry.price;
                tree[idx].directMaxBidders.clear();
                tree[idx].directMaxBidders.push_back(entry.bidderId);
            } else if (entry.price == tree[idx].directMaxBid) {
                // 避免重复添加
                if (std::find(tree[idx].directMaxBidders.begin(),
                              tree[idx].directMaxBidders.end(),
                              entry.bidderId) == tree[idx].directMaxBidders.end()) {
                    tree[idx].directMaxBidders.push_back(entry.bidderId);
                }
            }
        }
    }

    // 上推聚合信息: 从子节点更新当前节点的overallMaxBid
    void pushUp(int idx) {
        int leftMax = 0, rightMax = 0;
        if (idx * 2 < 4 * MAXN + 10) leftMax = tree[idx * 2].overallMaxBid;
        if (idx * 2 + 1 < 4 * MAXN + 10) rightMax = tree[idx * 2 + 1].overallMaxBid;

        int childMax = std::max(leftMax, rightMax);
        tree[idx].overallMaxBid = std::max(tree[idx].directMaxBid, childMax);

        // 设置overallMaxBidder
        if (tree[idx].directMaxBid >= childMax && tree[idx].directMaxBid > 0) {
            tree[idx].overallMaxBidder = tree[idx].directMaxBidders.empty() 
                ? "" : tree[idx].directMaxBidders[0];
        } else if (leftMax >= rightMax && leftMax > 0) {
            tree[idx].overallMaxBidder = tree[idx * 2].overallMaxBidder;
        } else if (rightMax > 0) {
            tree[idx].overallMaxBidder = tree[idx * 2 + 1].overallMaxBidder;
        } else {
            tree[idx].overallMaxBidder = "";
        }
    }

    // 从指定节点向上更新到根
    void pushUpToRoot(int idx) {
        while (idx >= 1) {
            pushUp(idx);
            idx /= 2;
        }
    }

    // 区间竞价更新(内部递归)
    void rangeUpdateImpl(int idx, int ql, int qr, int bidId,
                         const std::string& bidder, int price,
                         std::vector<int>& affectedNodes) {
        // 记录路径
        if (enableTrace) {
            std::ostringstream oss;
            oss << "访问节点 [" << tree[idx].start << "-" << tree[idx].end << "]";
            traceLog.push_back(oss.str());
        }

        // 无交集
        if (ql > tree[idx].end || qr < tree[idx].start) return;

        // 完全覆盖 - 懒惰分解: 将竞价直接分配到该节点
        if (ql <= tree[idx].start && tree[idx].end <= qr) {
            BidEntry entry{bidId, bidder, price, true};
            tree[idx].bids.push_back(entry);
            tree[idx].lazyMarked = true;

            // 更新直接最高价
            if (price > tree[idx].directMaxBid) {
                tree[idx].directMaxBid = price;
                tree[idx].directMaxBidders.clear();
                tree[idx].directMaxBidders.push_back(bidder);
            } else if (price == tree[idx].directMaxBid) {
                if (std::find(tree[idx].directMaxBidders.begin(),
                              tree[idx].directMaxBidders.end(),
                              bidder) == tree[idx].directMaxBidders.end()) {
                    tree[idx].directMaxBidders.push_back(bidder);
                }
            }

            // 更新overallMaxBid
            tree[idx].overallMaxBid = std::max(tree[idx].overallMaxBid, price);
            if (price >= tree[idx].overallMaxBid) {
                tree[idx].overallMaxBidder = bidder;
            }

            affectedNodes.push_back(idx);

            if (enableTrace) {
                std::ostringstream oss;
                oss << "命中区间 [" << tree[idx].start << "-" << tree[idx].end << "] 竞价者:" << bidder << " 出价:" << price;
                traceLog.push_back(oss.str());
            }
            return;
        }

        // 部分覆盖 - 递归到子节点
        rangeUpdateImpl(idx * 2, ql, qr, bidId, bidder, price, affectedNodes);
        rangeUpdateImpl(idx * 2 + 1, ql, qr, bidId, bidder, price, affectedNodes);
        pushUp(idx);
    }

    // 单点查询(内部递归)
    void pointQueryImpl(int idx, int t, int& maxPrice,
                        std::vector<std::string>& winners) {
        if (enableTrace) {
            std::ostringstream oss;
            oss << "访问节点 [" << tree[idx].start << "-" << tree[idx].end << "]";
            traceLog.push_back(oss.str());
        }

        // 检查当前节点的直接竞价
        if (tree[idx].directMaxBid > maxPrice) {
            maxPrice = tree[idx].directMaxBid;
            winners = tree[idx].directMaxBidders;
        } else if (tree[idx].directMaxBid == maxPrice && tree[idx].directMaxBid > 0) {
            for (const auto& w : tree[idx].directMaxBidders) {
                if (std::find(winners.begin(), winners.end(), w) == winners.end()) {
                    winners.push_back(w);
                }
            }
        }

        // 叶子节点
        if (tree[idx].start == tree[idx].end) {
            if (enableTrace) {
                std::ostringstream oss;
                oss << "命中叶子 [" << tree[idx].start << "] 最高价:" << maxPrice;
                traceLog.push_back(oss.str());
            }
            return;
        }

        int mid = (tree[idx].start + tree[idx].end) / 2;
        if (t <= mid) {
            pointQueryImpl(idx * 2, t, maxPrice, winners);
        } else {
            pointQueryImpl(idx * 2 + 1, t, maxPrice, winners);
        }
    }

    // 区间查询(内部递归) - 返回区间内最高出价
    void rangeQueryImpl(int idx, int ql, int qr,
                        int& maxPrice, std::string& winner) {
        if (ql > tree[idx].end || qr < tree[idx].start) return;

        if (enableTrace) {
            std::ostringstream oss;
            oss << "访问节点 [" << tree[idx].start << "-" << tree[idx].end << "] overallMax=" << tree[idx].overallMaxBid;
            traceLog.push_back(oss.str());
        }

        // 完全覆盖
        if (ql <= tree[idx].start && tree[idx].end <= qr) {
            if (tree[idx].overallMaxBid > maxPrice) {
                maxPrice = tree[idx].overallMaxBid;
                winner = tree[idx].overallMaxBidder;
            }
            return;
        }

        // 部分覆盖: directMaxBid 覆盖整个节点区间, 与查询区间交集非空
        considerDirectMax(idx, maxPrice, winner);
        rangeQueryImpl(idx * 2, ql, qr, maxPrice, winner);
        rangeQueryImpl(idx * 2 + 1, ql, qr, maxPrice, winner);
    }

public:
    SegmentTree() : enableTrace(false) {
        build(1, 0, MAXN - 1);
    }

    // 设置路径溯源
    void setTrace(bool enabled) {
        enableTrace = enabled;
        traceLog.clear();
    }

    // 获取路径溯源日志
    std::vector<std::string> getTraceLog() const {
        return traceLog;
    }

    // 清空路径溯源日志
    void clearTrace() {
        traceLog.clear();
    }

    // 区间竞价更新
    // 返回受影响的线段树节点索引列表(用于撤价时定位)
    std::vector<int> rangeUpdate(int ql, int qr, int bidId,
                                 const std::string& bidder, int price) {
        std::vector<int> affectedNodes;
        clearTrace();
        if (enableTrace) {
            std::ostringstream oss;
            oss << "[Update] 区间[" << ql << "-" << qr << "] 竞价者:" << bidder << " 出价:" << price;
            traceLog.push_back(oss.str());
        }
        rangeUpdateImpl(1, ql, qr, bidId, bidder, price, affectedNodes);
        // 上推所有受影响节点的祖先
        // (rangeUpdateImpl中的递归已经通过pushUp处理了部分,但为确保完全正确)
        for (int nodeIdx : affectedNodes) {
            pushUpToRoot(nodeIdx);
        }
        return affectedNodes;
    }

    // 单点查询: 返回(最高价, 胜出者列表)
    std::pair<int, std::vector<std::string>> pointQuery(int t) {
        int maxPrice = 0;
        std::vector<std::string> winners;
        clearTrace();
        if (enableTrace) {
            std::ostringstream oss;
            oss << "[Query] 单点查询 时间=" << t;
            traceLog.push_back(oss.str());
        }
        pointQueryImpl(1, t, maxPrice, winners);
        return {maxPrice, winners};
    }

    // 区间查询: 返回(最高价, 胜出者)
    std::pair<int, std::string> rangeQuery(int ql, int qr) {
        int maxPrice = 0;
        std::string winner;
        clearTrace();
        if (enableTrace) {
            std::ostringstream oss;
            oss << "[Query] 区间查询 [" << ql << "-" << qr << "]";
            traceLog.push_back(oss.str());
        }
        rangeQueryImpl(1, ql, qr, maxPrice, winner);
        return {maxPrice, winner};
    }

    // 撤价: 在指定节点中标记指定bidId为不活跃,并重新计算
    void withdrawAtNodes(int bidId, const std::vector<int>& nodeIndices) {
        for (int idx : nodeIndices) {
            // 标记该竞价为不活跃
            for (auto& entry : tree[idx].bids) {
                if (entry.bidId == bidId) {
                    entry.active = false;
                }
            }
            // 重新计算直接最高价
            recalcDirect(idx);
            // 上推到根
            pushUpToRoot(idx);
        }
    }

    // 获取所有叶子节点的状态(用于可视化)
    // 返回: 每个分钟点 -> (最高价, 胜出者列表)
    std::vector<std::pair<int, std::vector<std::string>>> getAllLeafStates() {
        std::vector<std::pair<int, std::vector<std::string>>> states(MAXN);
        for (int t = 0; t < MAXN; t++) {
            states[t] = pointQuery(t);
        }
        return states;
    }

    // 获取节点区间信息(用于调试)
    std::string getNodeInfo(int idx) const {
        std::ostringstream oss;
        oss << "[" << tree[idx].start << "-" << tree[idx].end << "] "
            << "directMax=" << tree[idx].directMaxBid
            << " overallMax=" << tree[idx].overallMaxBid
            << " winner=" << tree[idx].overallMaxBidder
            << " bidsCount=" << tree[idx].bids.size();
        return oss.str();
    }

    // 时间转可读格式 (分钟 -> HH:MM)
    static std::string timeToStr(int minutes) {
        int h = minutes / 60;
        int m = minutes % 60;
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << h << ":"
            << std::setfill('0') << std::setw(2) << m;
        return oss.str();
    }
};
