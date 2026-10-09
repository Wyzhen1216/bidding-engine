# 电商平台广告位分时段动态竞价引擎

基于**线段树（Segment Tree）**实现的广告位分时段竞价引擎：支持区间竞价更新、单点/区间最高价查询、撤价后的次高价自动恢复，并提供零依赖的 Web 可视化面板，用来观察全天竞价归属与线段树查询路径。

## 项目背景

电商平台的广告位按时间片售卖，一天被划分为 1440 个分钟片。不同广告主可以在任意时间区间内对同一个广告位出价，同一时刻以出价最高者获得展示权。若系统中直接扫描区间求最值，在竞价条目较多、查询频繁的场景下开销较大，因此这里用线段树把区间更新与区间最值查询都压缩到 O(log n)。

## 功能

- 区间竞价更新：任意广告主在 `[start, end)` 时间段内为某个广告位出价
- 单点查询 / 区间查询：查询某一时刻或某段时间内的最高出价与获胜广告主
- 撤价：按竞价 ID 撤销出价，并自动恢复该区间的次高价
- 路径溯源：打印线段树从根到叶的访问路径，便于理解查询过程
- 模拟数据生成：一键生成 5000+ 条竞价记录用于压测
- 内置验证：与暴力算法对拍，校验大规模场景下的正确性
- Web 可视化：导出/打开 HTML 面板，4 行时间轴展示全天竞价归属

## 目录结构

```
.
├── CMakeLists.txt          CMake 构建配置
├── src/
│   ├── segment_tree.h      线段树核心实现（区间更新 + 区间最值）
│   ├── bidding_engine.h    竞价引擎（广告位、竞价记录、撤价与恢复）
│   ├── data_generator.h    模拟竞价数据生成器
│   └── main.cpp            命令行主程序
└── visualization/
    └── index.html          Web 可视化面板（双击即可打开）
```

## 构建与运行

### 方式一：Web 可视化（零安装）

直接用浏览器打开 `visualization/index.html`，页面会自动加载竞价数据并渲染可视化面板。

### 方式二：命令行程序

使用 CMake：

```bash
cmake -B build
cmake --build build
```

或不使用 CMake，直接用 GCC 编译：

```bash
g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ \
    -finput-charset=utf-8 -fexec-charset=utf-8 \
    -o bidding_engine.exe src/main.cpp
```

编译产物为单个可执行文件，Windows / Linux / macOS 均可构建运行，仅需支持 C++17 的编译器。

## 命令行指令

| 命令 | 说明 |
| --- | --- |
| `update <slot> <start> <end> <bidder> <price>` | 区间竞价更新 |
| `query <slot> <time>` | 单点最高价查询 |
| `queryrange <slot> <start> <end>` | 区间最高价查询 |
| `withdraw <bidId>` | 全额撤价（恢复次高价） |
| `show <slot>` | 以字符画显示时间轴 |
| `trace on/off` | 开启/关闭路径溯源 |
| `gen <count>` | 生成模拟数据 |
| `stats` | 显示统计信息 |
| `export` | 导出可视化 HTML |
| `test` | 运行内置验证测试 |
| `help` / `quit` | 显示帮助 / 退出 |

参数说明：`slot` 为广告位编号 0-3；`start`/`end`/`time` 为分钟数 0-1439（0 表示 0:00）；`price` 为正整数，单位元。

示例：

```
> update 0 840 870 Adv_A 500      # 广告位 0，14:00-14:30，出价 500
> update 0 855 900 Adv_B 600      # 广告位 0，14:15-15:00，出价 600
> query 0 860                     # 查询 14:20 的最高出价
> withdraw 1                      # 撤销竞价 ID=1
> test                            # 与暴力算法对拍
> export                          # 导出可视化 HTML
```

## 复杂度

- 区间更新：O(log n)
- 单点 / 区间查询：O(log n)
- 撤价：O(log n)（按竞价 ID 撤销并恢复次高价）

