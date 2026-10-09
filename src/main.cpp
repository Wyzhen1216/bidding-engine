#include "bidding_engine.h"
#include "data_generator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <iomanip>
#ifdef _WIN32
#include <windows.h>
#endif

// 全局引擎和生成器
BiddingEngine* engine = nullptr;
DataGenerator* generator = nullptr;

// 打印帮助信息
void printHelp() {
    std::cout << "\n========================================\n";
    std::cout << "  电商平台广告位分时段动态竞价引擎\n";
    std::cout << "========================================\n\n";
    std::cout << "可用命令:\n";
    std::cout << "  update <slot> <start> <end> <bidder> <price>  区间竞价更新\n";
    std::cout << "  query <slot> <time>                           单点最高价查询\n";
    std::cout << "  queryrange <slot> <start> <end>               区间最高价查询\n";
    std::cout << "  withdraw <bidId>                              全额撤价\n";
    std::cout << "  show <slot>                                   显示广告位时间轴(字符画)\n";
    std::cout << "  trace on/off                                  开启/关闭路径溯源\n";
    std::cout << "  gen <count>                                   生成模拟竞价数据\n";
    std::cout << "  stats                                         显示统计信息\n";
    std::cout << "  export                                        导出可视化HTML\n";
    std::cout << "  test                                          运行验证测试\n";
    std::cout << "  help                                          显示帮助信息\n";
    std::cout << "  quit                                          退出程序\n\n";
}

// 显示广告位时间轴(字符画)
void showTimeline(int slotId) {
    if (slotId < 0 || slotId >= engine->getNumSlots()) {
        std::cout << "[错误] 广告位编号无效, 范围: 0-" << engine->getNumSlots() - 1 << "\n";
        return;
    }

    std::cout << "\n=== 广告位" << slotId << " 时段竞价看板 ===\n\n";

    // 每个字符代表30分钟
    int charPerSlot = 30;
    int totalChars = SegmentTree::MAXN / charPerSlot; // 48个字符

    // 先获取所有数据
    std::vector<std::pair<int, std::vector<std::string>>> states(SegmentTree::MAXN);
    for (int t = 0; t < SegmentTree::MAXN; t++) {
        states[t] = engine->queryPoint(slotId, t);
    }

    // 字符代号映射
    std::map<std::string, char> bidderChar;
    char nextChar = 'A';
    for (int t = 0; t < SegmentTree::MAXN; t++) {
        if (!states[t].second.empty()) {
            std::string winner = states[t].second[0];
            if (bidderChar.find(winner) == bidderChar.end() && nextChar <= 'Z') {
                bidderChar[winner] = nextChar++;
            }
        }
    }

    // 打印时间刻度
    std::cout << "时间: ";
    for (int h = 0; h < 24; h++) {
        if (h % 4 == 0) {
            std::cout << std::setw(2) << std::setfill(' ') << h << "h";
        } else {
            std::cout << "   ";
        }
    }
    std::cout << "\n";

    // 打印时间轴
    std::cout << "竞价: ";
    for (int c = 0; c < totalChars; c++) {
        int t = c * charPerSlot + charPerSlot / 2; // 取中点
        if (t >= SegmentTree::MAXN) t = SegmentTree::MAXN - 1;
        if (states[t].second.empty()) {
            std::cout << ".";
        } else {
            std::string winner = states[t].second[0];
            std::cout << bidderChar[winner];
        }
    }
    std::cout << "\n";

    // 打印价格轴
    std::cout << "价格: ";
    for (int c = 0; c < totalChars; c++) {
        int t = c * charPerSlot + charPerSlot / 2;
        if (t >= SegmentTree::MAXN) t = SegmentTree::MAXN - 1;
        if (states[t].first > 0) {
            int level = std::min(states[t].first / 500, 9);
            std::cout << level;
        } else {
            std::cout << ".";
        }
    }
    std::cout << "\n\n";

    // 图例
    std::cout << "图例: . = 无人竞价  0-9 = 价格等级(0:<500, 1:<1000, ..., 9:>=4500)\n";
    std::cout << "竞价者代号:\n";
    for (const auto& [bidder, ch] : bidderChar) {
        std::cout << "  " << ch << " = " << bidder << "\n";
    }
    std::cout << "\n";
}

// 导出可视化HTML文件
void exportVisualization() {
    std::cout << "[信息] 正在生成可视化数据...\n";

    // 获取可视化数据
    auto visData = engine->getVisualizationData();

    // 生成HTML
    std::ofstream out("visualization/index.html");
    if (!out.is_open()) {
        std::cout << "[错误] 无法创建可视化文件\n";
        return;
    }

    out << "<!DOCTYPE html>\n<html lang='zh-CN'>\n<head>\n";
    out << "<meta charset='UTF-8'>\n";
    out << "<title>电商平台广告位分时段动态竞价引擎 - 可视化</title>\n";
    out << "<style>\n";
    out << "  * { margin: 0; padding: 0; box-sizing: border-box; }\n";
    out << "  body { font-family: 'Microsoft YaHei', sans-serif; background: #1a1a2e; color: #eee; padding: 20px; }\n";
    out << "  h1 { text-align: center; margin-bottom: 20px; color: #e94560; font-size: 24px; }\n";
    out << "  .slot-container { margin: 15px 0; background: #16213e; border-radius: 8px; padding: 15px; }\n";
    out << "  .slot-title { color: #4ECDC4; font-size: 16px; margin-bottom: 8px; font-weight: bold; }\n";
    out << "  canvas { border: 1px solid #0f3460; border-radius: 4px; cursor: crosshair; display: block; }\n";
    out << "  .info-panel { margin-top: 15px; background: #0f3460; border-radius: 8px; padding: 15px; }\n";
    out << "  .info-title { color: #FFEAA7; font-size: 16px; margin-bottom: 8px; font-weight: bold; }\n";
    out << "  #pointInfo { color: #96CEB4; font-size: 14px; min-height: 20px; }\n";
    out << "  #traceLog { color: #85C1E9; font-size: 13px; font-family: Consolas, monospace; ";
    out << "    max-height: 300px; overflow-y: auto; white-space: pre-wrap; line-height: 1.6; }\n";
    out << "  .legend { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 8px; }\n";
    out << "  .legend-item { display: flex; align-items: center; gap: 4px; font-size: 12px; }\n";
    out << "  .legend-color { width: 16px; height: 16px; border-radius: 3px; }\n";
    out << "  .stats { color: #BB8FCE; font-size: 13px; margin-top: 10px; }\n";
    out << "</style>\n</head>\n<body>\n";

    out << "<h1>电商平台广告位分时段动态竞价引擎</h1>\n";

    // 生成竞价者颜色映射
    std::map<std::string, std::string> colorMap;
    std::vector<std::string> colors = {
        "#FF6B6B", "#4ECDC4", "#45B7D1", "#96CEB4", "#FFEAA7",
        "#DDA0DD", "#98D8C8", "#F7DC6F", "#BB8FCE", "#85C1E9",
        "#F1948A", "#82E0AA", "#F8C471", "#AED6F1", "#D7BDE2"
    };

    // 收集所有竞价者
    std::set<std::string> allBidders;
    for (int s = 0; s < (int)visData.size(); s++) {
        for (int t = 0; t < SegmentTree::MAXN; t++) {
            for (const auto& w : visData[s][t].second) {
                allBidders.insert(w);
            }
        }
    }
    int colorIdx = 0;
    for (const auto& bidder : allBidders) {
        colorMap[bidder] = colors[colorIdx % colors.size()];
        colorIdx++;
    }

    // JavaScript数据
    out << "<script>\n";
    out << "const NUM_SLOTS = " << engine->getNumSlots() << ";\n";
    out << "const MAXN = " << SegmentTree::MAXN << ";\n";

    // 时间线数据
    out << "const timelineData = [\n";
    for (int s = 0; s < (int)visData.size(); s++) {
        out << "  [\n";
        for (int t = 0; t < SegmentTree::MAXN; t++) {
            std::string winner = visData[s][t].second.empty() ? "" : visData[s][t].second[0];
            out << "    {time:" << t << ", price:" << visData[s][t].first 
                << ", winner:\"" << winner << "\"}";
            if (t < SegmentTree::MAXN - 1) out << ",";
            out << "\n";
        }
        out << "  ],\n";
    }
    out << "];\n";

    // 颜色映射
    out << "const bidderColors = {\n";
    bool first = true;
    for (const auto& [bidder, color] : colorMap) {
        if (!first) out << ",\n";
        out << "  \"" << bidder << "\": \"" << color << "\"";
        first = false;
    }
    out << "\n};\n";
    out << "</script>\n";

    // 生成Canvas和交互
    out << "<div id='slots'></div>\n";
    out << "<div class='info-panel'>\n";
    out << "  <div class='info-title'>单点查询详情</div>\n";
    out << "  <div id='pointInfo'>点击时间轴上的任意位置查看竞价详情</div>\n";
    out << "</div>\n";
    out << "<div class='info-panel'>\n";
    out << "  <div class='info-title'>线段树路径溯源</div>\n";
    out << "  <div id='traceLog'>执行查询或更新操作时,此处显示线段树遍历路径</div>\n";
    out << "</div>\n";

    out << "<script>\n";
    out << R"(
// 渲染时间轴
const container = document.getElementById('slots');
const canvases = [];

for (let s = 0; s < NUM_SLOTS; s++) {
    const div = document.createElement('div');
    div.className = 'slot-container';
    
    const title = document.createElement('div');
    title.className = 'slot-title';
    title.textContent = '广告位' + (s + 1) + ' (Slot ' + s + ')';
    div.appendChild(title);
    
    const canvas = document.createElement('canvas');
    canvas.width = 1200;
    canvas.height = 60;
    canvas.id = 'canvas_' + s;
    div.appendChild(canvas);
    
    // 图例
    const legend = document.createElement('div');
    legend.className = 'legend';
    legend.id = 'legend_' + s;
    div.appendChild(legend);
    
    container.appendChild(div);
    canvases.push(canvas);
    
    // 绘制时间轴
    drawTimeline(s);
    
    // 点击事件
    canvas.addEventListener('click', function(e) {
        const rect = canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const time = Math.floor(x / canvas.width * MAXN);
        if (time >= 0 && time < MAXN) {
            showPointInfo(s, time);
        }
    });
}

function drawTimeline(slotId) {
    const canvas = canvases[slotId];
    const ctx = canvas.getContext('2d');
    const data = timelineData[slotId];
    const width = canvas.width;
    const height = canvas.height;
    
    ctx.fillStyle = '#0a0a23';
    ctx.fillRect(0, 0, width, height);
    
    // 绘制每个分钟
    for (let t = 0; t < MAXN; t++) {
        const x = t / MAXN * width;
        const w = Math.max(width / MAXN, 1);
        
        if (data[t].price > 0 && data[t].winner) {
            ctx.fillStyle = bidderColors[data[t].winner] || '#666';
            // 高度按价格缩放
            const h = Math.min(data[t].price / 2000 * (height - 10), height - 4);
            ctx.fillRect(x, height - h - 2, w + 0.5, h);
        }
    }
    
    // 时间刻度
    ctx.fillStyle = '#555';
    ctx.font = '10px sans-serif';
    for (let h = 0; h < 24; h += 3) {
        const x = h * 60 / MAXN * width;
        ctx.fillText(h + ':00', x, height - 2);
        ctx.strokeStyle = '#333';
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, height - 12);
        ctx.stroke();
    }
    
    // 图例
    const legendDiv = document.getElementById('legend_' + slotId);
    legendDiv.innerHTML = '';
    const seenBidders = new Set();
    for (let t = 0; t < MAXN; t++) {
        if (data[t].winner && !seenBidders.has(data[t].winner)) {
            seenBidders.add(data[t].winner);
            const item = document.createElement('div');
            item.className = 'legend-item';
            const colorBox = document.createElement('div');
            colorBox.className = 'legend-color';
            colorBox.style.background = bidderColors[data[t].winner] || '#666';
            item.appendChild(colorBox);
            item.appendChild(document.createTextNode(data[t].winner));
            legendDiv.appendChild(item);
        }
    }
}

function showPointInfo(slotId, time) {
    const data = timelineData[slotId][time];
    const h = Math.floor(time / 60);
    const m = time % 60;
    const timeStr = String(h).padStart(2, '0') + ':' + String(m).padStart(2, '0');
    
    let info = '广告位' + (slotId + 1) + ' | 时间: ' + timeStr + ' (分钟' + time + ')';
    if (data.price > 0) {
        info += ' | 胜出者: ' + data.winner + ' | 出价: ' + data.price;
    } else {
        info += ' | 无竞价';
    }
    document.getElementById('pointInfo').textContent = info;
    
    // 高亮显示该点
    for (let s = 0; s < NUM_SLOTS; s++) {
        drawTimeline(s);
    }
    const canvas = canvases[slotId];
    const ctx = canvas.getContext('2d');
    const x = time / MAXN * canvas.width;
    ctx.strokeStyle = '#FFD700';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, canvas.height);
    ctx.stroke();
    ctx.lineWidth = 1;
    
    // 模拟路径溯源
    let trace = '[Query] 单点查询 广告位' + (slotId+1) + ' 时间=' + time + '\n';
    trace += '访问节点 [0-1439]';
    let l = 0, r = 1439, depth = 1;
    while (l < r) {
        let mid = Math.floor((l + r) / 2);
        if (time <= mid) {
            trace += ' → 访问子节点 [' + l + '-' + mid + ']';
            r = mid;
        } else {
            trace += ' → 访问子节点 [' + (mid+1) + '-' + r + ']';
            l = mid + 1;
        }
        depth++;
    }
    trace += ' → ★ 命中叶子 [' + time + ']';
    trace += '\n查询深度: ' + depth + '层 (log₂1440 ≈ 10.5)';
    document.getElementById('traceLog').textContent = trace;
}
)" << "\n";
    out << "</script>\n";

    // 统计信息
    out << "<div class='info-panel'>\n";
    out << "  <div class='info-title'>系统统计</div>\n";
    out << "  <div class='stats'>" << engine->getStatistics() << "</div>\n";
    out << "</div>\n";

    out << "</body>\n</html>\n";
    out.close();

    std::cout << "[成功] 可视化文件已导出到 visualization/index.html\n";
    std::cout << "[提示] 用浏览器打开该文件查看可视化结果\n";
}

// 运行验证测试
void runTests() {
    std::cout << "\n=== 运行验证测试 ===\n\n";

    BiddingEngine testEngine(4);
    testEngine.setTrace(true);
    bool allPassed = true;

    // 测试1: 基本竞价与查询
    std::cout << "测试1: 基本竞价与查询\n";
    {
        int bid1 = testEngine.placeBid(0, 840, 870, "Adv_A", 500);
        int bid2 = testEngine.placeBid(0, 855, 900, "Adv_B", 600);

        auto [price1, winners1] = testEngine.queryPoint(0, 845);
        auto [price2, winners2] = testEngine.queryPoint(0, 860);
        auto [price3, winners3] = testEngine.queryPoint(0, 880);

        bool t1 = (price1 == 500 && winners1[0] == "Adv_A");
        bool t2 = (price2 == 600 && winners2[0] == "Adv_B");
        bool t3 = (price3 == 600 && winners3[0] == "Adv_B");

        std::cout << "  14:05(845) 出价=" << price1 << " 胜出=" << (winners1.empty() ? "无" : winners1[0]) 
                  << (t1 ? " ✓" : " ✗") << "\n";
        std::cout << "  14:20(860) 出价=" << price2 << " 胜出=" << (winners2.empty() ? "无" : winners2[0])
                  << (t2 ? " ✓" : " ✗") << "\n";
        std::cout << "  14:40(880) 出价=" << price3 << " 胜出=" << (winners3.empty() ? "无" : winners3[0])
                  << (t3 ? " ✓" : " ✗") << "\n";

        if (!(t1 && t2 && t3)) allPassed = false;

        // 打印路径溯源
        auto trace = testEngine.getTraceLog(0);
        if (!trace.empty()) {
            std::cout << "  路径溯源:\n";
            for (const auto& line : trace) {
                std::cout << "    " << line << "\n";
            }
        }
    }

    // 测试2: 撤价后次高价恢复
    std::cout << "\n测试2: 撤价后次高价恢复\n";
    {
        BiddingEngine eng2(4);
        int bid1 = eng2.placeBid(1, 0, 100, "Adv_A", 300);
        int bid2 = eng2.placeBid(1, 0, 100, "Adv_B", 500);
        int bid3 = eng2.placeBid(1, 0, 100, "Adv_C", 400);

        auto [p1, w1] = eng2.queryPoint(1, 50);
        std::cout << "  初始: 出价=" << p1 << " 胜出=" << (w1.empty() ? "无" : w1[0]) << "\n";

        eng2.withdrawBid(bid2);  // B撤价
        auto [p2, w2] = eng2.queryPoint(1, 50);
        bool t1 = (p2 == 400 && w2[0] == "Adv_C");
        std::cout << "  B撤价后: 出价=" << p2 << " 胜出=" << (w2.empty() ? "无" : w2[0])
                  << (t1 ? " ✓" : " ✗") << "\n";

        eng2.withdrawBid(bid3);  // C也撤价
        auto [p3, w3] = eng2.queryPoint(1, 50);
        bool t2 = (p3 == 300 && w3[0] == "Adv_A");
        std::cout << "  C撤价后: 出价=" << p3 << " 胜出=" << (w3.empty() ? "无" : w3[0])
                  << (t2 ? " ✓" : " ✗") << "\n";

        if (!(t1 && t2)) allPassed = false;
    }

    // 测试3: 同价多人处理
    std::cout << "\n测试3: 同价多人处理\n";
    {
        BiddingEngine eng3(4);
        eng3.placeBid(2, 500, 600, "Adv_A", 200);
        eng3.placeBid(2, 500, 600, "Adv_B", 200);

        auto [price, winners] = eng3.queryPoint(2, 550);
        bool t1 = (price == 200 && winners.size() >= 1);
        std::cout << "  550分钟: 出价=" << price << " 胜出者数量=" << winners.size()
                  << (t1 ? " ✓" : " ✗") << "\n";
        if (!t1) allPassed = false;
    }

    // 测试4: 区间查询
    std::cout << "\n测试4: 区间查询\n";
    {
        BiddingEngine eng4(4);
        eng4.placeBid(0, 0, 500, "Adv_A", 300);
        eng4.placeBid(0, 600, 1000, "Adv_B", 600);

        auto [price, winner] = eng4.queryRange(0, 0, 1000);
        bool t1 = (price == 600 && winner == "Adv_B");
        std::cout << "  区间[0-1000]: 最高价=" << price << " 胜出=" << winner
                  << (t1 ? " ✓" : " ✗") << "\n";
        if (!t1) allPassed = false;
    }

    // 测试4b: 区间查询部分重叠(懒惰分解节点真子集)
    std::cout << "\n测试4b: 区间查询部分重叠\n";
    {
        BiddingEngine eng4b(4);
        eng4b.placeBid(0, 840, 870, "Adv_A", 500);

        auto [price, winner] = eng4b.queryRange(0, 845, 860);
        bool t1 = (price == 500 && winner == "Adv_A");
        std::cout << "  区间[845-860](Adv_A出价[840-870]=500): 最高价=" << price
                  << " 胜出=" << winner << (t1 ? " ✓" : " ✗") << "\n";

        auto [pMid, wMid] = eng4b.queryPoint(0, 855);
        bool t2 = (pMid == 500 && !wMid.empty() && wMid[0] == "Adv_A");
        std::cout << "  单点855交叉验证: 出价=" << pMid << " 胜出="
                  << (wMid.empty() ? "无" : wMid[0]) << (t2 ? " ✓" : " ✗") << "\n";

        if (!(t1 && t2)) allPassed = false;
    }

    // 测试6: 区间查询 brute-force 对拍
    std::cout << "\n测试6: 区间查询 brute-force 对拍 (500次随机区间)\n";
    {
        BiddingEngine eng6(4);
        DataGenerator gen(54321);
        gen.generate(eng6, 500);

        int mismatch = 0;
        for (int i = 0; i < 500; i++) {
            int slot = i % 4;
            int ql = (i * 73) % 1440;
            int qr = ql + (i * 17) % 120;
            if (qr >= 1440) qr = 1439;

            auto [rangePrice, rangeWinner] = eng6.queryRange(slot, ql, qr);

            int bruteMax = 0;
            std::string bruteWinner;
            for (int t = ql; t <= qr; t++) {
                auto [p, winners] = eng6.queryPoint(slot, t);
                if (p > bruteMax) {
                    bruteMax = p;
                    bruteWinner = winners.empty() ? "" : winners[0];
                }
            }

            if (rangePrice != bruteMax) {
                mismatch++;
            }
        }

        bool t1 = (mismatch == 0);
        std::cout << "  500次随机区间对拍: 价格不一致 " << mismatch << " 次"
                  << (t1 ? " ✓" : " ✗") << "\n";
        if (!t1) allPassed = false;
    }

    // 测试5: 大规模数据压力测试
    std::cout << "\n测试5: 大规模数据压力测试 (5000条)\n";
    {
        BiddingEngine eng5(4);
        DataGenerator gen(12345);

        auto start = std::chrono::high_resolution_clock::now();
        auto bidIds = gen.generate(eng5, 5000);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "  导入5000条数据耗时: " << duration.count() << "ms\n";
        std::cout << "  实际生成竞价数: " << bidIds.size() << "\n";

        // 批量查询测试
        start = std::chrono::high_resolution_clock::now();
        int queryCount = 1000;
        for (int i = 0; i < queryCount; i++) {
            eng5.queryPoint(i % 4, (i * 37) % 1440);
        }
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "  " << queryCount << "次单点查询耗时: " << duration.count() << "ms\n";
        std::cout << "  平均单次查询: " << (duration.count() * 1000.0 / queryCount) << "μs\n";

        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < queryCount; i++) {
            int ql = (i * 53) % 1200;
            int qr = ql + 50 + (i * 11) % 100;
            if (qr >= 1440) qr = 1439;
            eng5.queryRange(i % 4, ql, qr);
        }
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "  " << queryCount << "次区间查询耗时: " << duration.count() << "ms\n";
    }

    std::cout << "\n=== 测试结果: " << (allPassed ? "全部通过 ✓" : "部分失败 ✗") << " ===\n\n";
}

// 解析并执行命令
bool executeCommand(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string action;
    iss >> action;

    if (action == "quit" || action == "exit") {
        return false;
    } else if (action == "help") {
        printHelp();
    } else if (action == "update") {
        int slot = -1, start = 0, end = 0, price = 0;
        std::string bidder;
        iss >> slot >> start >> end >> bidder >> price;
        if (iss.fail()) {
            std::cout << "[错误] 格式: update <slot> <start> <end> <bidder> <price>\n";
        } else {
            int bidId = engine->placeBid(slot, start, end, bidder, price);
            if (bidId > 0) {
                std::cout << "[成功] 竞价ID=" << bidId << " 广告位" << slot 
                          << " 区间[" << start << "-" << end << "] " << bidder << "出价" << price << "\n";
            } else {
                std::cout << "[失败] 参数无效\n";
            }
        }
        // 打印路径溯源
        if (slot >= 0 && engine->getTraceLog(slot).size() > 0) {
            std::cout << "[路径] ";
            for (const auto& line : engine->getTraceLog(slot)) {
                std::cout << line << " ";
            }
            std::cout << "\n";
        }
    } else if (action == "query") {
        int slot, time;
        iss >> slot >> time;
        if (iss.fail()) {
            std::cout << "[错误] 格式: query <slot> <time>\n";
        } else {
            auto [price, winners] = engine->queryPoint(slot, time);
            std::string timeStr = BiddingEngine::timeToStr(time);
            std::cout << "[结果] 广告位" << slot << " 时间" << time << "(" << timeStr << ")";
            if (price > 0) {
                std::cout << " → 胜出者: ";
                for (size_t i = 0; i < winners.size(); i++) {
                    if (i > 0) std::cout << ",";
                    std::cout << winners[i];
                }
                std::cout << " 出价: " << price;
            } else {
                std::cout << " → 无竞价";
            }
            std::cout << "\n";
        }
        // 路径溯源
        if (engine->getTraceLog(slot).size() > 0) {
            std::cout << "[路径] ";
            for (const auto& line : engine->getTraceLog(slot)) {
                std::cout << line << " ";
            }
            std::cout << "\n";
        }
    } else if (action == "queryrange") {
        int slot, start, end;
        iss >> slot >> start >> end;
        if (iss.fail()) {
            std::cout << "[错误] 格式: queryrange <slot> <start> <end>\n";
        } else {
            auto [price, winner] = engine->queryRange(slot, start, end);
            std::cout << "[结果] 广告位" << slot << " 区间[" << start << "-" << end << "]";
            if (price > 0) {
                std::cout << " → 最高价: " << price << " 胜出: " << winner;
            } else {
                std::cout << " → 无竞价";
            }
            std::cout << "\n";
        }
    } else if (action == "withdraw") {
        int bidId;
        iss >> bidId;
        if (iss.fail()) {
            std::cout << "[错误] 格式: withdraw <bidId>\n";
        } else {
            bool success = engine->withdrawBid(bidId);
            if (success) {
                auto record = engine->getBidRecord(bidId);
                std::cout << "[成功] 竞价ID=" << bidId << "已撤销\n";
                if (record) {
                    // 查询撤价后的状态
                    auto [price, winners] = engine->queryPoint(record->slotId, (record->start + record->end) / 2);
                    if (price > 0) {
                        std::cout << "[恢复] 当前最高出价: " << (winners.empty() ? "无" : winners[0]) 
                                  << " " << price << "\n";
                    } else {
                        std::cout << "[恢复] 该区间当前无竞价\n";
                    }
                }
            } else {
                std::cout << "[失败] 竞价ID无效或已撤销\n";
            }
        }
    } else if (action == "show") {
        int slot;
        iss >> slot;
        if (iss.fail()) {
            std::cout << "[错误] 格式: show <slot>\n";
        } else {
            showTimeline(slot);
        }
    } else if (action == "trace") {
        std::string onoff;
        iss >> onoff;
        if (onoff == "on") {
            engine->setTrace(true);
            std::cout << "[成功] 路径溯源已开启\n";
        } else {
            engine->setTrace(false);
            std::cout << "[成功] 路径溯源已关闭\n";
        }
    } else if (action == "gen") {
        int count = 5000;
        iss >> count;
        std::cout << "[信息] 生成" << count << "条模拟竞价数据...\n";
        auto start = std::chrono::high_resolution_clock::now();
        auto bidIds = generator->generate(*engine, count);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "[成功] 生成" << bidIds.size() << "条竞价, 耗时" << duration.count() << "ms\n";
    } else if (action == "stats") {
        std::cout << engine->getStatistics();
    } else if (action == "export") {
        exportVisualization();
    } else if (action == "test") {
        runTests();
    } else {
        std::cout << "[错误] 未知命令: " << action << " (输入help查看帮助)\n";
    }

    return true;
}

int main() {
    // 设置Windows控制台为UTF-8编码
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    #endif

    // 初始化引擎和生成器
    engine = new BiddingEngine(4);
    generator = new DataGenerator(42);

    std::cout << "\n========================================\n";
    std::cout << "  电商平台广告位分时段动态竞价引擎\n";
    std::cout << "  基于 Segment Tree (线段树) 实现\n";
    std::cout << "  广告位: 4个 | 时间粒度: 1分钟 | 全天: 1440分钟\n";
    std::cout << "========================================\n\n";

    // 自动生成5000条数据
    std::cout << "[初始化] 生成5000条模拟竞价数据...\n";
    auto start = std::chrono::high_resolution_clock::now();
    generator->generate(*engine, 5000);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "[完成] 数据生成完毕, 耗时" << duration.count() << "ms\n";
    std::cout << engine->getStatistics() << "\n";

    // 开启路径溯源
    engine->setTrace(true);

    // 命令行交互
    std::string cmd;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, cmd)) break;
        if (cmd.empty()) continue;

        // 去除首尾空格
        size_t start2 = cmd.find_first_not_of(" \t");
        size_t end2 = cmd.find_last_not_of(" \t");
        if (start2 == std::string::npos) continue;
        cmd = cmd.substr(start2, end2 - start2 + 1);

        if (!executeCommand(cmd)) break;
    }

    // 清理
    delete engine;
    delete generator;

    std::cout << "\n[退出] 感谢使用竞价引擎系统!\n";
    return 0;
}
