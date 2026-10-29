# Oyama's HRT Tracker — Linux / BSD 桌面版设计

- **状态**：草案，等待所有者批准
- **日期**：2026-10-29
- **目标仓库**：`xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop`
- **上游仓库**：`xunxunProjects/Oyama-s-HRT-Tracker`（v1.5.1）
- **许可证**：MIT

---

## 0. 一句话

用 Qt 6 / QML 为 Linux 和 BSD 重写一个与网页版视觉一致的原生桌面应用：没有登录、没有云端、没有遥测；药代动力学核心用 C99 实现，界面用 C++20 + QML，数据存在本机 PostgreSQL，导入导出沿用上游的 `oyama-hrt-complete-backup` 格式。

---

## 1. 背景

上游应用由 React + TypeScript 前端、Cloudflare Worker 后端、Tauri v1 桌面壳组成。Tauri 的发布矩阵只构建 macOS（dmg）和 Windows（msi），Linux 用户目前只能通过 Docker/本地服务加浏览器使用，BSD 没有官方路径。

本仓库不是上游的补丁分支，而是一个独立实现：

- 上游继续服务 Web、Windows、macOS 和账号体系。
- 本仓库只服务 Linux/BSD 桌面，允许长期与上游分叉。
- 两边通过**模型向量**和**备份格式**保持可验证的一致性，而不是共享代码。

---

## 2. 已定决策

| # | 决策 | 说明 |
|---|------|------|
| D1 | 独立仓库，允许长期分叉 | 不做上游的 fork；不合并上游提交 |
| D2 | Qt 6 + QML 原生界面 | 视觉对齐网页版，不嵌 WebEngine |
| D3 | C99 核心 + C++20 应用层 | C++23 只用于确认可用的特性；不使用 C++23 作为最低要求 |
| D4 | 本地 PostgreSQL 存储 | 所有者决定；通过 libpq 访问，数据库层隔离在 repository 接口后 |
| D5 | 无登录、无账号 | 同时移除分享、社区投票、设备配对、F2F、Tool API、云同步、管理后台 |
| D6 | 保留导出/导入 | 沿用上游完整备份格式 v1，保证用户在两个版本之间迁移 |
| D7 | 只支持 Linux 与 BSD | Windows/macOS 不在范围内 |
| D8 | X11 + Wayland | 以 KDE、XFCE、Niri 为主要测试目标，不写 DE 专用代码 |
| D9 | 模型一致性靠 golden vectors | 上游测试导出为 JSON；C 实现跑同一组向量 |
| D10 | 默认零网络 | 不发起任何网络请求；PostgreSQL 走 Unix socket |

> D4 是本设计里唯一一个与“本地优先、易安装”目标存在张力的决定。见 §14 风险与 §5.3 的缓解方案。

---

## 3. 目标、非目标、反目标

### 3.1 目标

1. Linux/BSD 用户拿到一个原生、离线、可自行编译的桌面应用。
2. 核心计算结果与上游一致，任何差异都可被向量测试发现。
3. 用户的健康数据只存在本机，导出文件可被上游导入，反之亦然。
4. 在 KDE（Wayland/X11）、XFCE（X11）、Niri（Wayland）和 FreeBSD（X11）上可用。
5. 源码构建路径简单：CMake + Ninja，一条命令配置、一条命令构建。

### 3.2 非目标

- 账号、登录、密码、2FA、会话管理。
- 云同步、实时分享、社区投票、F2F、Tool API。
- 多人或家庭共享。
- Windows、macOS、移动端。
- 第一版就对齐上游全部功能。
- 内置 Postgres 二进制或自动安装系统包。

### 3.3 反目标

- 模型结果与上游悄悄不一致。
- 导入/导出丢记录或改错字段。
- 为了“原生”而要求用户安装 Node、npm、Tauri 或 Rust。
- 任何形式的遥测、崩溃上报、远程配置。
- 把健康数据发到网络。

---

## 4. 架构

```text
┌──────────────────────────────────────────────┐
│ app/  C++20 + Qt 6                           │
│  QML 视图  ←→  C++ view models               │
│  charts / dialogs / i18n / settings          │
└───────────────┬──────────────────────────────┘
                │ 纯函数调用（C ABI）
┌───────────────▼──────────────────────────────┐
│ core/  C99                                   │
│  PK 模型、单位、剂量建议、校准、方案推算       │
│  无 I/O、无全局状态、确定性                   │
└───────────────┬──────────────────────────────┘
                │ repository 接口
┌───────────────▼──────────────────────────────┐
│ storage/  C++20 + libpq                      │
│  schema migrations、事务、查询、导入/导出      │
└───────────────┬──────────────────────────────┘
                │ Unix socket
┌───────────────▼──────────────────────────────┐
│ 本地 PostgreSQL 14+                          │
└──────────────────────────────────────────────┘
```

### 4.1 分层

| 目录 | 语言 | 职责 | 依赖 |
|------|------|------|------|
| `core/` | C99 | 药代动力学、单位换算、剂量建议、校准、方案节奏 | 仅 libm |
| `storage/` | C++20 | libpq 封装、migration、事务、repository | libpq、core |
| `app/` | C++20 + QML | 窗口、视图模型、图表、设置、i18n | Qt 6、storage、core |
| `cli/` | C99/C++20 | 初始化数据库、导入/导出、跑向量 | core、storage |
| `tests/` | C99/C++20 + Qt Test | 单元、集成、QML、向量 | 全部 |

### 4.2 为什么 C99 核心

- 无 Qt 依赖，可单独编译成静态库，CLI 和测试都能复用。
- C ABI 稳定，C++ 侧调用简单，不需要绑定生成器。
- 模型是纯数学，不需要 RAII、异常或模板。
- 上游 `logic.ts` 是纯函数风格，逐函数移植的成本最低。

### 4.3 为什么 C++20 而不是 C++23

- Qt 6 本身只要求 C++17；C++20 提供 concepts/ranges/`std::span`，足够。
- 没有哪个标准库完整实现 C++23；FreeBSD/OpenBSD 的 Clang 版本参差。
- 最低工具链：GCC 11+ 或 Clang 14+。
- C++23 特性（如 `std::expected`、`std::print`）只在 detection macro 后使用，不作为公共接口。

### 4.4 线程模型

- QML/UI 线程只做渲染和轻量状态。
- 仿真在 `QThreadPool` 的工作线程执行；输入是不可变事件数组，输出通过信号回传。
- libpq 连接不跨线程共享；每个工作线程持有自己的连接，或使用小型连接池。
- 数据库写入在事务中完成；UI 先更新乐观状态，失败时回滚并提示。

---

## 5. 数据模型（PostgreSQL）

### 5.1 表

```sql
-- schema 版本（由迁移 runner 维护；name/checksum 用于防止已应用文件被改写）
schema_migrations(version integer primary key, name text not null,
                  checksum text not null, applied_at timestamptz not null);

-- 全局设置，单行；与上游一致：体重/PK 参数/校准/出生年不按模式分开
settings(
  id boolean primary key default true check (id),
  body_weight_kg double precision,
  birth_year integer,
  pk_params jsonb,                 -- null = 使用默认参数
  pk_params_updated_at bigint,
  calibration_method text not null default 'mipd',
  calibration_history_mode text not null default 'retrospective',
  updated_at bigint not null
);

dose_events(
  id uuid primary key,
  mode text not null check (mode in ('transfem','transmasc')),
  route text not null,
  ester text not null,
  time_h double precision not null,   -- 1970 起的小时数，与上游一致
  dose_mg double precision not null,
  extras jsonb not null default '{}',
  updated_at bigint,
  created_at bigint not null
);

lab_results(
  id uuid primary key,
  mode text not null check (mode in ('transfem','transmasc')),
  time_h double precision not null,
  value double precision not null,
  unit text not null,
  assay text not null check (assay in ('immuno','lcms')),
  updated_at bigint,
  created_at bigint not null
);

dose_templates(id uuid primary key, mode text not null, payload jsonb not null, updated_at bigint);
quick_doses(id uuid primary key, mode text not null, payload jsonb not null, updated_at bigint);
supplies(id uuid primary key, mode text not null, payload jsonb not null, updated_at bigint);
plan_items(id uuid primary key, mode text not null, payload jsonb not null, updated_at bigint);

-- 导入合并语义需要；单机不删除时可为空
tombstones(kind text not null, record_id uuid not null, deleted_at bigint not null,
           primary key (kind, record_id));

preferences(key text primary key, value jsonb not null, updated_at bigint not null);
```

设计说明：

- `time_h` 用 `double precision`，与上游 `number` 语义一致，避免整数化带来的曲线偏移。
- 记录主体用 `jsonb` 的表（templates/quick doses/supplies/plan）是为了在早期快速跟上上游字段变化；稳定后再决定是否拆列。
- `updated_at` 用 epoch 毫秒，保留上游的 last-write-wins 语义，导入合并时行为一致。
- 两种模式（transfem/transmasc）各自独立，切换模式不共享剂量/化验/方案记录；但**体重、出生年、PK 参数、校准方式和历史模式是全局设置**，与上游 `SHARED_SUFFIXES` 的行为一致。
- 默认校准为 `mipd` / `retrospective`，与上游 `normalizeCalibrationMethod` 和 `cal-history-mode` 的缺省行为一致；年龄修正默认关闭，由设置中的开关启用。
- 不建 `users`、`sessions`、`shares`、`community_vote`、`f2f_*` 等表。

### 5.2 迁移

- SQL 文件放在 `storage/migrations/NNNN_*.sql`，按文件名排序。
- 启动时在事务中应用，使用 `pg_advisory_xact_lock` 防止多实例并发迁移。
- 每个已应用文件记录 FNV-1a 64 位校验和；文件被改写、缺失或版本重复时拒绝启动迁移。
- 迁移只前进，不回滚；破坏性变更必须附带导出/重建策略。
- 第一版 schema 由 M1 固化，之后只增不改。

### 5.3 连接与初始化（Postgres 的 UX 缓解）

首次启动提供两种方式：

1. **连接已有集群（默认推荐）**
   - 通过 Unix socket 连接 `$XDG_RUNTIME_DIR`、`/run/postgresql` 或用户指定的 socket 目录。
   - 使用 peer/ident 认证，不保存密码。
   - 数据库名默认 `oyama_hrt`，角色使用当前系统用户。

2. **应用托管的本地集群（可选）**
   - 检测 PATH 中的 `initdb`、`pg_ctl`、`postgres`。
   - 在 `$XDG_DATA_HOME/oyama-hrt/pgdata` 初始化，目录权限 0700。
   - 只监听 Unix socket（`$XDG_RUNTIME_DIR/oyama-hrt`），不开放 TCP。
   - 应用启动时拉起、退出时停止；崩溃后下次启动恢复。
   - 不自动安装 Postgres 包；缺失时显示发行版安装命令。

如果两者都不可用，应用显示设置向导和文档链接，不进入主界面。这个流程必须在 M1 完成，否则应用无法启动。

---

## 6. 模型契约（golden vectors）

### 6.1 权威来源

- 算法说明：上游 `Algorithm Explanation.md`。
- 行为来源：上游 `logic.ts` 及其测试。
- 独立交叉验证：上游 `tests/reference/pkcoreUpstream.ts`（Swift 模型的移植）。

### 6.2 向量格式

`docs/spec/pk-vectors.json`：

```json
{
  "format": "oyama-hrt-pk-vectors",
  "version": 1,
  "upstreamCommit": "<sha>",
  "cases": [
    {
      "id": "simulation/overlapping-routes-sum",
      "source": "tests/simulation.test.ts",
      "input": { "...": "..." },
      "expect": { "...": "..." },
      "tolerance": { "relative": 1e-6, "absolute": 1e-9 }
    }
  ]
}
```

- 每条向量必须能追溯到具体测试文件或用例。
- 分类：注射（EB/EV/EC/EN/EU/E2）、贴片、凝胶、口服、舌下、CPA、睾酮、校准（off/ekf/ou_kalman/mipd）、年龄、单位、清洁剂量、方案节奏、建议文案的阈值。
- 浮点比较：浓度类用相对 1e-6；分类/文本/选择结果精确匹配。

### 6.3 执行

- `cli/hrt-vectors` 读取 JSON，调用 `core`，输出失败用例和最大误差。
- CTest 注册为 `core-vectors`；CI 必须通过。
- 上游新增或修改模型时，在本仓库更新向量文件并跑通，才能发版。
- 任何**有意**偏离上游的行为，必须在 `docs/spec/divergences.md` 记录原因、影响和版本号。

### 6.4 验收

M1 结束时，`core` 必须通过 M0 导出的**全部**向量，覆盖两种模式、所有已建模路由与酯类、CPA、睾酮、年龄修正和四种校准方法（`off`/`ekf`/`ou_kalman`/`mipd`）。

不接受“先支持主线路由、其余以后再说”：未建模的路由如果能在 UI 里选到，就会静默产生错误曲线。确实不建模的药物（上游 `UNMODELLED_DRUGS`）只记录、不参与仿真，并在 UI 中明确标注。

如果某个类别暂时无法移植，必须在 UI 中禁用对应入口，而不是让它返回近似值。

---

## 7. 界面范围

### 7.1 视觉原则

- 对齐上游的配色、圆角、卡片、字体层级、暗色模式；不追求像素级一致，但用户一眼认得出是同一个产品。
- 桌面布局：左侧导航 + 内容区；窗口最小 900×600；窄窗口折叠为顶部导航。
- 图表复用上游的坐标轴、颜色和交互（缩放、悬停读数、预测区段）。
- 文案沿用上游 i18n key，避免两套术语。

### 7.2 第一版（M3–M4）

| 页面 | 内容 |
|------|------|
| Onboarding | 免责声明、选择模式、体重/出生年（可选）、数据库初始化 |
| Overview | 当前估算浓度、今日剂量、浓度曲线、预测开关 |
| Add dose | 路由、酯/药物、剂量、时间、 extras |
| History | 按天分组、编辑、删除 |
| Plan / What-if | 方案编辑、频率、模拟对比 |
| Settings | 模式、体重、出生年（含年龄修正开关）、PK 参数、校准方式、外观、语言 |
| Labs（只读） | 导入的化验结果列表，确保被校准使用的数据可见 |
| Export / Import | 完整备份导出、预览、合并/替换导入 |

### 7.3 第二版（M5+）

- Labs：化验结果、单位换算、校准点。
- Supplies：库存、消耗、用完预测。
- Forecast。
- Dose templates / quick doses。
- 高级校准方法与历史模式。

### 7.4 i18n

上游支持 `zh`、`zh-TW`、`yue`、`en`、`ja`、`ko`、`tr`。本仓库以 JSON/`.ts` 形式复用同一套 key，第一版至少 zh + en；其余语言作为数据补齐，不硬编码在 QML 里。

---

## 8. 桌面集成与兼容矩阵

### 8.1 原则

- 不写 DE 专用代码；只依赖 XDG 标准和 Qt 的跨平台抽象。
- 不假设有系统托盘、全局菜单、标题栏或窗口管理器装饰。
- 文件对话框优先走 XDG portal；portal 不可用时回退到 Qt 对话框。
- 通知走 `org.freedesktop.Notifications`，失败时静默。
- 单实例用 `QLocalServer`；第二次启动唤起已有窗口。

### 8.2 测试矩阵

| 系统 | 会话 | 桌面/WM | 优先级 |
|------|------|---------|--------|
| Linux | Wayland | KDE Plasma 6 | P0 |
| Linux | X11 | KDE Plasma 6 | P0 |
| Linux | X11 | XFCE 4.18 | P0 |
| Linux | Wayland | Niri | P0 |
| Linux | Wayland | GNOME 46 | P1 |
| FreeBSD 14 | X11 | XFCE | P0 |
| OpenBSD 7.x | X11 | XFCE | P1 |

说明：

- **Niri 只有 Wayland**，没有托盘；窗口装饰由客户端负责时不能假设有标题栏。
- **XFCE 主要是 X11**；Wayland 会话不作为第一版目标。
- **BSD 的 Wayland 生态很弱**，第一版按 X11 交付；Niri 不在 BSD 上运行。
- FreeBSD 的 Qt 6 版本较新；OpenBSD 的 Qt 6 包可能滞后，作为 P1。

### 8.3 桌面文件

- `.desktop` 文件、AppStream metainfo、SVG 图标。
- 应用 ID 待定（见 §15），候选：`com.smirnovayama.hrttracker.LinuxDesktop`。
- 可选：注册 `.json` 备份文件的 MIME 类型，双击导入（第一版可不做）。

---

## 9. 构建、依赖、打包

### 9.1 依赖

| 依赖 | 最低版本 | 用途 |
|------|----------|------|
| CMake | 3.21 | 构建 |
| Ninja | 任意 | 构建 |
| C 编译器 | C99（GCC 11+/Clang 14+） | core |
| C++ 编译器 | C++20（GCC 11+/Clang 14+） | app/storage |
| Qt | 6.4 | Quick、Quick Controls 2、Svg、Concurrent |
| libpq | 14 | 数据库客户端 |
| PostgreSQL server | 14 | 本机数据库 |
| OpenSSL | 3.x | 仅在上游备份加密进入范围时使用 |

不依赖 Node、npm、Rust、Tauri、WebEngine。

### 9.2 构建命令

```sh
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
cmake --install build/linux-release
```

`CMakePresets.json` 提供 `linux-release`、`linux-debug`、`freebsd-release`。

### 9.3 打包

- CPack 生成 `deb`、`rpm`、`tar.gz`。
- AppImage 作为可选产物（需要额外工具链）。
- Flatpak 清单在 M5 之后。
- FreeBSD port 在 M5 之后。
- 第一版不承诺二进制仓库；先保证源码可构建。

### 9.4 CI

| 平台 | 用途 |
|------|------|
| Ubuntu 24.04 | 主构建、单元测试、ASan/UBSan |
| Fedora 40 | Qt/编译器兼容性 |
| Arch Linux 容器 | 滚动发行版兼容性 |
| FreeBSD 14 VM | BSD 构建 + X11 冒烟 |
| Xvfb / offscreen | QML 冒烟测试 |

Postgres 集成测试使用临时 cluster（`initdb` + `pg_ctl`），不依赖外部服务。

---

## 10. 备份互通

### 10.1 格式

- 采用上游 `oyama-hrt-complete-backup`，`formatVersion = 1`。
- 顶层字段：`format`、`formatVersion`、`createdAt`、`mode`、`modes`、`weight`、`weightUpdatedAt`、`pkParams`、`pkParamsUpdatedAt`、`calibration`、`preferences`。
- `modes` 下：`events`、`labResults`、`doseTemplates`、`deletions`、`quickDoses`、`supplies`、`plan`。
- 本应用没有账号，导出时省略 `sourceAccount`；导入时忽略该字段。
- 上游 v1 备份没有 `birthYear` 字段。本应用在顶层增加可选扩展 `birthYear` 与 `birthYearUpdatedAt`；上游导入会忽略未知顶层字段，因此“本应用 → 上游 → 本应用”的往返会丢失出生年。导入预览必须提示这一点，不能宣称无损。
- 除 `birthYear` 外，第一版不增加其他扩展字段；避免制造上游无法保留的私有数据。

### 10.2 导入

- 先预览：记录数、冲突数、被删除数、标量变化。
- 合并规则与上游一致：`updatedAt` last-write-wins；tombstone 5 分钟未来容差、180 天 TTL、每类最多 5000 条；单类最多 20000 条记录。
- 导入在事务中执行；失败全部回滚。
- 不认识的字段：顶层保留忽略；记录内字段丢弃并报告，与上游行为一致。

### 10.3 导出

- 输出可直接被上游网页版导入的 JSON。
- 导出前做自检：不能导出上游导入器会拒绝的记录。
- 明文导出，文件内含健康数据；导出对话框必须明确提示。
- 加密 `.hrtb` 不在第一版；后续通过独立的 OpenPGP 兼容层实现。

---

## 11. 安全与隐私

- **零网络**：应用不包含 HTTP 客户端；Postgres 只走 Unix socket。若未来加同步，必须单独设计并默认关闭。
- **零遥测**：没有崩溃上报、使用统计、远程配置。
- **数据库权限**：数据目录 0700；socket 目录 0700；数据库角色只允许当前系统用户。
- **导出文件**：明文，提示用户妥善保管；建议全盘加密。
- **威胁模型**：能登录同一系统账号的本地用户可以读取数据库；这与任何本地应用一致，不在第一版解决。
- **内存安全**：C++ 侧只用 RAII/智能指针，禁止裸 `new/delete`；CI 跑 ASan/UBSan；C 核心跑 `-fanalyzer`/clang-tidy。
- **依赖更新**：Qt、libpq、Postgres 的最低版本随安全公告更新；不静态链接 OpenSSL。

---

## 12. 测试策略

| 层 | 内容 | 工具 |
|----|------|------|
| core | 向量、边界、随机差分 | CTest + 自定义 runner |
| storage | migration、事务、导入导出、并发 | CTest + 临时 Postgres |
| app | view model、单位、格式化 | Qt Test |
| QML | 页面冒烟、导航、对话框 | Qt Quick Test |
| 兼容 | X11/Wayland/DE 矩阵 | 手工 + CI 冒烟 |
| 安全 | ASan/UBSan、clang-tidy | CI |

验收门槛：

- 向量全部通过。
- 导入上游导出的样例文件后，记录数、时间、剂量、模式与源文件一致。
- 本应用导出的文件能被上游网页版导入（手工验收，每个里程碑一次）。

---

## 13. 里程碑

| 里程碑 | 交付 | 退出条件 |
|--------|------|----------|
| M0 | 仓库、设计文档、向量提取、UI 清单、备份样例、CMake 骨架 | 文档批准；向量文件可生成；CI 空跑通过 |
| M1 | `core` 模型 + 向量 runner；Postgres schema + migration；首次启动向导 | 全部向量通过；空库可初始化；导入/导出 CLI 可用 |
| M2 | QML 壳、Overview、Add dose、History | 可以记录、编辑、删除、看到曲线 |
| M3 | Plan/What-if、Settings、Export/Import UI、Labs 只读 | 完整备份往返不丢已知字段 |
| M4 | Labs 编辑、Supplies、Forecast | 上游核心功能对齐 |
| M5 | 打包、BSD CI、桌面集成、文档 | deb/rpm 可安装；FreeBSD 构建通过 |

“第一版”指 **M3**：能从源码构建、可以日常记录和查看曲线的版本，不包含公开二进制包。**M5 才是第一次公开二进制发布**。M4 不是 M3 的发布阻塞项，但导入的 Labs 必须在 M3 以只读形式可见，否则校准会使用用户看不见的数据。

---

## 14. 风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| Postgres 安装/运行门槛 | 用户装不上，应用无法启动 | 首次启动向导；应用托管 cluster；文档；M1 必须完成 |
| 模型漂移 | 计算结果与上游不一致 | golden vectors + 上游 CI；差异必须记录 |
| UI 重写工作量 | 长期无法交付 | 分阶段；先主线后 Labs/Supplies；复用设计 token |
| Qt/BSD 兼容 | BSD 用户无法构建 | Qt 6.4 下限；FreeBSD CI；X11 优先 |
| C++ 内存安全 | 崩溃/数据损坏 | RAII、智能指针、ASan/UBSan、clang-tidy |
| 范围蔓延 | 永远到不了第一版 | 明确非目标；账号/社交/同步一律拒绝 |
| 两套实现长期分叉 | 维护成本翻倍 | 向量契约；备份格式；不共享代码但共享测试 |

---

## 15. 待定项（需要所有者或实现者拍板）

| # | 问题 | 建议 |
|---|------|------|
| Q1 | 应用 ID / .desktop 名称 | `com.smirnovayama.hrttracker.LinuxDesktop`，避免与上游 Tauri 包冲突 |
| Q2 | 图表实现 | 自绘 `QQuickPaintedItem`，不依赖 QtCharts/Qt Graphs，便于对齐 Web 图表 |
| Q3 | Postgres 初始化默认方式 | 先探测已有集群；没有则提供应用托管 cluster |
| Q4 | 第一版语言 | zh + en；其余语言作为数据后续补 |
| Q5 | 加密备份 | 第一版不做；M5 后评估 OpenPGP 兼容 |
| Q6 | 包格式 | 第一版源码 + deb/rpm；AppImage 可选 |
| Q7 | 最低 Qt | 6.4（覆盖 Debian 12 / Ubuntu 24.04）；FreeBSD 用 ports 版本 |

实现者可以在不改变产品行为的前提下自行决定 Q2、Q3、Q6 的细节；Q1、Q4、Q5 需要所有者确认。

---

## 16. 仓库与分支策略

- `main` 受保护：
  - 必须通过 Pull Request 合并（允许 0 个批准，作者可自行合并）。
  - 必须通过 CI 检查（CI 建立后启用）。
  - 禁止 force push、禁止删除分支。
  - 要求线性历史；合并方式使用 squash。
- 分支命名：`feat/…`、`fix/…`、`docs/…`、`chore/…`。
- 提交信息使用 Conventional Commits。
- `docs/spec/` 下的向量和格式文件按版本冻结；修改需要单独 PR。
- 上游模型变更通过“更新向量”进入本仓库，不直接 cherry-pick 代码。

---

## 17. Builder check 处理记录

设计文档写完后由独立实现者做了一次“能否据此排计划”的检查，六个问题及处理如下：

| # | 问题 | 处理 |
|---|------|------|
| B1 | M1 要移植多少模型 | **完整移植**：两种模式、全部已建模路由、CPA、睾酮、年龄、四种校准。未覆盖的入口在 UI 中禁用，不返回近似值。这是较大的范围决定，所有者可以要求缩减，但缩减必须同时禁用对应入口。 |
| B2 | 体重/出生年/PK/校准是全局还是按模式 | **全局**。schema 改为单行 `settings`，记录类仍按 `mode` 分区；与上游 `SHARED_SUFFIXES` 一致。 |
| B3 | 出生年如何进出备份 | 顶层扩展字段 `birthYear`/`birthYearUpdatedAt`；上游会忽略，往返会丢失，导入预览提示。 |
| B4 | 第一版指哪个里程碑 | **M3 是第一个可日常使用的源码版本；M5 是第一个公开二进制发布。** |
| B5 | 只有存储、没有 UI 的备份类别怎么办 | M3 导入仍完整落库；Labs 提供只读列表，保证校准输入可见；Supplies/templates/quick doses 在导入预览显示数量，完整 UI 在 M4。 |
| B6 | 模型默认值是否对齐上游 | **对齐**：`mipd` / `retrospective`；年龄修正默认关闭，由设置开关启用。 |

---

## 18. 批准记录

- [ ] 所有者批准本设计文档
- [ ] 批准后进入实现计划（`writing-plans`）
- [ ] M0 开工
