# 桌面版界面重做 — 暖色设计系统、四语言与 RTL

- **状态**：已批准（2026-10-30）
- **日期**：2026-10-30
- **目标仓库**：`xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop`
- **关联设计**：`docs/superpowers/specs/2026-10-29-linux-desktop-design.md`
- **范围**：只改 `app/` 的 Qt/QML 界面与相关 C++ 状态文案；不改模型、存储、备份格式和 CLI 行为

---

## 0. 一句话

把现有“深色顶栏 + 深色卡片”的 Qt 界面，换成以暖米色为底、左侧图标导航、留白充足、动效柔和的原生桌面界面；首次启动先选语言，支持中文、英文、俄语、波斯语，波斯语整界面 RTL。

---

## 1. 为什么现在做

现有界面功能完整，但视觉上是“能用的工程界面”：

- 深色背景 `#0e1318`、顶栏贯穿窗口、卡片带描边，和网页版的温和感差距明显。
- 导航图标是 `◉ ＋ ≡ ⚙` 这类字符，风格不统一。
- 页面之间切换是硬切，按钮和输入框是 Qt Basic 默认样式。
- 文案全部英文硬编码，无法选择语言；波斯语需要 RTL，现有布局没有考虑。

所有者已经确定：结构向网页版靠拢（左侧导航 + 卡片内容区），默认亮色，使用暖米色系统；并支持四种语言在首次启动时选择。

---

## 2. 目标、非目标、反目标

### 2.1 目标

1. 第一眼看起来像同一个 Oyama 产品：暖色、低对比、大圆角、留白、轻动效。
2. 桌面结构向网页版靠拢：左侧导航、页面标题、卡片内容区、悬停高亮。
3. 默认亮色，暗色可切换；两套颜色都达到可读对比度。
4. 首次启动可选中文 / English / Русский / فارسی，之后可在设置里切换，无需重启。
5. 波斯语界面整体 RTL：侧栏在右、布局镜像、文字右对齐；图表坐标保持从左到右。
6. 现有功能、C++ 接口、存储行为、测试全部保留。
7. Linux 与 FreeBSD 共用同一份 QML，构建产物一致。

### 2.2 非目标

- 不新增 Labs、Supplies、Plan、账号、社区投票等网页版功能。
- 不改 PostgreSQL schema、备份格式、CLI 参数。
- 不做无边框窗口、系统托盘、全局菜单。
- 不引入 Qt Quick 3D、QtCharts、WebEngine。
- 不为四种语言做完整本地化审校；第一版由实现者提供初译，母语审校可以作为后续 PR。

### 2.3 反目标

- 为了“高级感”堆毛玻璃、渐变、发光、重阴影。
- 每个组件各用一套圆角、字号、颜色。
- 语言切换需要重启，或波斯语只翻译文字却不镜像布局。
- 改 UI 时动到模型、存储或备份逻辑。
- 把中文字体或波斯语字体硬编码成某个 Linux 发行版才有的家族名。

---

## 3. 已定决策

| # | 决策 | 说明 |
|---|------|------|
| U1 | 左侧固定侧栏，不做悬浮展开 | 桌面窗口宽度足够；固定 236px，导航项悬停高亮，选中项填充 |
| U2 | 默认亮色，暗色可切 | `ui.theme` 默认 `light`；可选 `dark`、`system` |
| U3 | 四语言：`zh`、`en`、`ru`、`fa` | 首次启动整屏选择；设置里可改；波斯语 RTL |
| U4 | 翻译用运行时 JSON，不编译 `.qm` | 5 个页面、约 120 个 key；运行时切换简单，不依赖 `lrelease` |
| U5 | C++ 状态消息保留英文 `status`，另加 `statusKey`/`statusArgs` | 不破坏现有测试和 CLI；QML 显示翻译后的 key |
| U6 | 字体：内置 Inter + Vazirmatn，中文用系统 CJK 字体 | Inter 覆盖英/俄；Vazirmatn 覆盖波斯语；中文在 FreeBSD 安装 Noto CJK 或文泉驿 |
| U7 | 图标用 QML `Shape` + `PathSvg` 自绘 | 统一 1.75px 线宽，不依赖图标字体或 SVG 图片插件 |
| U8 | 阴影几乎不用，靠明度分层 | 软件渲染环境性能稳定，也符合“轻”的视觉 |
| U9 | 圆角三档：卡片 18、控件 12、胶囊 999 | 全应用统一 |
| U10 | 字号五档：28 / 20 / 15 / 13 / 11 | 字重 400 / 500 / 600 |

---

## 4. 视觉系统

### 4.1 颜色 token

亮色：

| Token | 值 | 用途 |
|-------|----|------|
| `bg` | `#EFE6D0` | 窗口底色 |
| `sidebar` | `#F5EDDF` | 侧栏，比底色略亮 |
| `surface` | `#FFFDF7` | 卡片 |
| `surfaceAlt` | `#EFE9D8` | 输入框、次级分区、列表行 |
| `surfaceHover` | `#F2E8D6` | 悬停 |
| `surfacePressed` | `#E7DCC6` | 选中 / 按下 |
| `text` | `#33302A` | 正文、标题 |
| `textSoft` | `#7A7466` | 20px 以上的次级标题、装饰性文字 |
| `textMuted` | `#6E6759` | 15px 正文次级、导航默认文字（与 `#7A7466` 同色相，略深，达到 4.5:1） |
| `textFaint` | `#6B6457` | 11–13px 说明、占位符 |
| `textDisabled` | `#A69E8E` | 禁用 |
| `border` | `#DED3BD` | 极少量分隔线、弹出层边界 |
| `accent` | `#B5793C` | 主色、主按钮、图表线 |
| `accentHover` | `#A66B31` | 主按钮悬停 |
| `accentSubtle` | `#F3E3CC` | 主色浅底、空状态图标底 |
| `accentInk` | `#8C5A28` | 浅底上的主色文字 |
| `green` | `#4F7A5A` | 成功、CPA 数据 |
| `brick` | `#B4573E` | 危险、T 数据、删除 |
| `onAccent` | `#2B241B` | 主按钮文字（暖深色；浅色按钮 + 深色字，对比度约 5.2:1） |
| `chartLine` | `#B5793C` | 曲线主色（等同 `accent`） |
| `chartGrid` | `#E4D8C2` | 图表网格 |
| `shadow` | `rgba(60, 45, 20, 0.06)` | 极轻阴影（只在浮层用） |

暗色：

| Token | 值 | 用途 |
|-------|----|------|
| `bg` | `#1F1D1A` | 窗口底色 |
| `sidebar` | `#1B1917` | 侧栏，比底色略暗 |
| `surface` | `#2B2823` | 卡片 |
| `surfaceAlt` | `#35312B` | 输入框、浮层、次级分区 |
| `surfaceHover` | `#302C27` | 悬停 |
| `surfacePressed` | `#3A352E` | 选中 / 按下 |
| `text` | `#E6E0D4` | 正文、标题 |
| `textMuted` | `#A39C8D` | 次级文字 |
| `textFaint` | `#9A9385` | 11–13px 说明（在暗色卡片上达到 4.5:1） |
| `textDisabled` | `#6F695D` | 占位符、禁用 |
| `border` | `#3D3830` | 分隔线 |
| `accent` | `#C98F52` | 暗色下提亮的主色 |
| `accentHover` | `#D9A268` | 主按钮悬停 |
| `accentSubtle` | `#3A2F23` | 主色浅底 |
| `accentInk` | `#DFA96F` | 浅底上的主色文字 |
| `green` | `#7FA98A` | 成功、CPA |
| `brick` | `#D07A62` | 危险、T、删除 |
| `onAccent` | `#1F1D1A` | 暗色主按钮文字 |
| `chartGrid` | `#3A352E` | 图表网格 |

说明：

- 主色只有赭石 `#B5793C` 一个；墨绿和砖红只承担“成功 / 危险 / 第三条数据”的语义，不作为大面积装饰。
- 暗色不是简单反色：越靠上层越亮（`bg < surface < surfaceAlt`），符合“暗色靠明度递进”。
- 正文 `#33302A` 在 `#EFE6D0` 上约 10.6:1；暗色正文约 12:1。`textSoft`（`#7A7466`）只用于 20px 以上的次级标题；15px 正文次级、导航默认文字用 `textMuted`（`#6E6759`，约 4.5:1）；11–13px 用 `textFaint`（`#6B6457`，约 4.7:1）。暗色 `textFaint` 提到 `#9A9385`，在 `surface` 上约 4.5:1。主按钮用暖深色字 `#2B241B` 配赭石底（约 5.2:1），和网页版“浅色按钮 + 深色字”一致。

### 4.2 字体

| 语言 | 主字体 | 回退 |
|------|--------|------|
| `en` / `ru` | Inter | Noto Sans、Liberation Sans、DejaVu Sans |
| `zh` | Noto Sans CJK SC | 文泉驿正黑、Noto Sans |
| `fa` | Vazirmatn | Noto Naskh Arabic、DejaVu Sans |

- Inter Regular / Medium / SemiBold 与 Vazirmatn Regular / Medium / SemiBold 作为资源打包（OFL 1.1），`QFontDatabase::addApplicationFont` 在启动时加载。
- 中文不打包完整 CJK 字体（体积大）；Linux 使用系统 Noto CJK，FreeBSD 在部署时安装 `font-noto-cjk` 或 `zh-wqy-zenhei`。
- 标题与正文同一家族，靠字号和字重分层；不使用宋体标题，避免跨平台字体不一致。
- 字号：页面标题 28、区块标题 20、正文 15、标签 13、说明 11。正文行高 1.55；标题行高 1.2。
- 字重只用 400 / 500 / 600；不整页加粗。

### 4.3 圆角、间距、阴影

- 圆角：卡片 18；按钮、输入框、导航项 12；胶囊 / 头像 999。全应用只有这三档。
- 间距：4 / 8 / 12 / 16 / 20 / 24 / 32 / 40。页面左右边距 40（窄窗口 24）；卡片内边距 20；区块之间 24–32。
- 阴影：默认不用。只有弹出层、Toast 使用 `0 4px 24px rgba(60, 45, 20, 0.06)`（暗色 `rgba(0,0,0,0.35)`）。
- 分割线：只在设置分组、弹出层边界使用 `border`，其余靠 `surface` 与 `bg` 的明度差。

### 4.4 动效

| 场景 | 时长 | 曲线 |
|------|------|------|
| 悬停变色 | 150ms | `Easing.OutCubic` |
| 按下缩放 0.97 | 90ms | `Easing.OutCubic` |
| 页面切换：淡入 + 上移 8px | 280ms | `Easing.OutCubic` |
| 主题切换：颜色过渡 | 240ms | `Easing.InOutCubic` |
| 保存成功：对勾缩放 + 文案淡入 | 320ms | `Easing.OutBack` |
| Toast 进入 / 退出 | 220 / 160ms | `Easing.OutCubic` |

- 不使用线性动画；不做大幅滑动。
- 页面切换只让新页面淡入并上移，旧页面不参与动画，避免布局重排。
- 保存剂量成功后，主按钮内容在 320ms 内变成“对勾 + 已记录”，停留约 700ms，再回到概览；期间按钮禁用。

---

## 5. 窗口与页面结构

### 5.1 外壳

```text
┌──────────────┬────────────────────────────────────────────┐
│  Oyama       │  页面标题                       模式 / 状态 │
│  Tracker     │  一句说明                                   │
│              │                                            │
│  ◉ 概览      │  ┌──────────┐ ┌──────────┐ ┌──────────┐   │
│  ＋ 记录剂量  │  │ 读数卡    │ │ 读数卡    │ │ 读数卡    │   │
│  ☰ 历史      │  └──────────┘ └──────────┘ └──────────┘   │
│  ⚙ 设置      │  ┌──────────────────────────────────────┐  │
│              │  │ 曲线卡                                │  │
│              │  └──────────────────────────────────────┘  │
│              │                                            │
│  ● 本地连接   │  ┌──────────────────────────────────────┐  │
│  ☾ 主题      │  │ 最近记录                              │  │
└──────────────┴────────────────────────────────────────────┘
```

- 窗口默认 1200×800，最小 980×680。
- 侧栏固定 236px，背景 `sidebar`，右侧一条极淡分隔线（`border`）。
- 内容区左右边距 40，顶部 28，底部 32；概览内容最大宽度 960，表单/设置最大宽度 680。
- 不再使用 `ApplicationWindow.header` / `footer`。后端状态放到侧栏底部；操作反馈用 Toast。
- 页面标题在内容区顶部，不吸顶；滚动时标题随内容滚走，减少 Qt 的 sticky/玻璃实现复杂度。

### 5.2 侧栏

- 顶部：36×36 圆角应用图标（复用现有品牌色块或字母 `O`），右侧 `Oyama`（600）+ `Tracker`（400，muted）。
- 导航项：高 48，左右内边距 12，圆角 12，图标 20，文字 15，间距 14。
  - 默认：文字 `textMuted`，图标 `textMuted`，背景透明。
  - 悬停：背景 `surfaceHover`，文字 `text`，150ms。
  - 选中：背景 `surfacePressed`，文字 `text`（500），图标 `accent`。
  - 按下：整体缩放 0.98。
- 导航：概览、记录剂量、历史、设置。`记录剂量` 保持独立入口；点击时清空编辑状态。
- 底部：模式分段控件（transfem / transmasc，高 32，两个等宽胶囊，选中项 `surfacePressed`）；连接状态圆点 + 文案（`本地 · 仅 socket` / `未连接`）；主题切换图标按钮（太阳 / 月亮）；版本号小字（如能拿到）。
- 模式切换是全局的，所有页面都能用；切换后重新加载数据，未保存的剂量表单会被清空（与现行为一致）。
- 波斯语：侧栏整体移到窗口右侧，图标与文字顺序镜像，文字右对齐；模式分段控件同样镜像。

### 5.3 页面通用

- 标题区：标题 28/600，说明 13/`textMuted`；右侧可放模式切换、后端 chip 或主操作。
- 卡片：`Card` 组件，背景 `surface`，圆角 18，内边距 20，无边框；暗色下比底亮一档。
- 空状态：64px 圆形 `accentSubtle` 底 + 40px 线性图标；标题 20/600；说明 15/`textMuted`；一个主按钮。
- 列表行：高 56–64，圆角 12，悬停 `surfaceHover`；左侧图标、主文字 15、次级 13、右侧数值 15/`accent`。
- 所有可点击控件有键盘焦点态：2px `accent` 外圈 + 1px 内圈背景色。
- **数据库未配置**（`app.backend === "none"`）：不阻塞进入主界面；内容区顶部显示常驻提示条（`surfaceAlt`、圆角 12、信息图标 + `数据库尚未配置。先在终端运行 oyama-hrt-cli db setup。`），侧栏状态显示 `未连接`。空状态的 `记录剂量` 主按钮禁用，并在按钮下方说明原因。首次启动的语言选择页仍然先于主界面出现。
- 数据库连接成功（`managed` / `existing`）：不显示提示条；侧栏圆点变绿，文案 `本地 · 仅 socket`。
- 运行中数据库报错：用 Toast 显示 `T.t(app.statusKey, app.statusArgs)`，不弹阻塞对话框；`app.backend` 保持原值。

---

## 6. 页面明细

### 6.1 概览

- 标题：`概览` / `Overview` / `Обзор` / `نمای کلی`；说明：`当前暴露量与本地模型曲线。`
- 右上：连接状态 chip 可以省略（侧栏已有）；模式切换在侧栏底部，全局可用。
- 三张读数卡：E2、CPA、T。标签 11 大写/字距 0.06em；数值 32/600；单位 13，分别用赭石 / 墨绿 / 砖红。
- 曲线卡：标题 20/600；说明 13；Canvas 高度 320。
  - **只画 E2 一条曲线**；CPA 和 T 只在读数卡显示，避免双 Y 轴和不同单位带来的复杂度。`curve` 里的 `cpa`/`tLevel` 保留给后续版本。
  - 网格 `chartGrid`；坐标轴文字 11 `textMuted`；曲线 2.5px `chartLine`（等同 `accent`）；曲线下方 10% 透明度填充。
  - 无数据：居中空状态（药丸图标 + `暂无剂量记录` + `记录下第一次剂量，就能看到估算曲线。` + 主按钮 `记录剂量`）。
- 最近记录：标题 20/600；最多 5 条；每行显示 route · ester、时间、剂量；悬停高亮；点击进入历史。
- 模式切换会重新加载数据，保持现有 `app.mode` 行为。

### 6.2 记录剂量 / 编辑剂量

- 标题随状态变化：`记录剂量` 或 `编辑剂量`；说明一句。
- 表单卡片最大宽度 640，居中；内边距 24；字段间距 16。
- 两列网格（窄窗口自动一列）：Route / Ester、Dose (mg) / Days ago。
- 条件字段（舌下、凝胶、贴片）淡入出现，不突然撑开：`opacity` + `visible`，高度由布局自然变化。
- 输入框：高 44，背景 `surfaceAlt`，圆角 12，无边框；聚焦时 1.5px `accent` 边；占位符 `textFaint`。
- 主按钮：`记录剂量` / `保存修改`，赭石底，高 44，圆角 12；取消为幽灵按钮。
- 错误：字段下方 13px `brick` 文案；不弹窗。
- 成功：按钮变对勾 + `已记录`，320ms `OutBack`；700ms 后回到概览；错误不跳转。

### 6.3 历史

- 标题 + 记录数（`N 条记录`）。
- 列表卡片：每行 64px，左侧药丸图标，主文字 `route · ester`，次级时间 + 剂量，右侧编辑 / 删除图标按钮。
- 悬停整行 `surfaceHover`；编辑/删除按钮默认 `textMuted`，悬停变 `text`；删除悬停变 `brick`。
- 删除先弹确认对话框（`确认删除这条记录？` / `删除后无法恢复。` / `取消` / `删除`），确认后调 `app.removeDose`。
- 空状态：`还没有记录` + `记录第一剂后，这里会按时间列出。` + 主按钮。

### 6.4 设置

分组卡片：

1. **身体**：体重、出生年。
2. **估算模型**：校准方式、校准历史、PK 参数 JSON（等宽字体，最小高度 140）。
3. **语言与外观**：
   - 语言：四个胶囊按钮（中文 / English / Русский / فارسی），当前项填充 `surfacePressed`，点击即时切换。
   - 主题：三个分段按钮（亮色 / 暗色 / 跟随系统），选中项有滑动指示块。
4. **关于**：应用名、版本、`本地 · 仅 socket`、数据库状态。

- 保存按钮放在“身体/模型”卡片下方；语言和主题即时生效，不需要保存。
- 设置页最大宽度 680；标签 13/500，输入框同记录页。

---

## 7. 国际化与 RTL

### 7.1 语言与选择时机

- 语言代码：`zh`、`en`、`ru`、`fa`。
- 首次启动（`ui.language` 为空）显示整屏语言选择：
  - 暖色底，中央最大宽度 720。
  - 品牌图标 + `Choose your language`（英文，因为此时还没有语言）。
  - 2×2 卡片：`中文`、`English`、`Русский`、`فارسی`；每张卡片用本语言大写字样 + 英文名小字。
  - 悬停：`surfaceHover` + `accent` 边框；按下缩放 0.98；点击即写入 `ui.language`，选择页淡出，主界面淡入。
  - 底部小字：`You can change this later in Settings.`
- 设置页可随时切换；切换后所有 `T.t()` 绑定立即重算，无需重启。
- 测试 / 截图可用环境变量 `OYAMA_UI_LANGUAGE=en|zh|ru|fa` 跳过选择页；该变量不写入配置。

### 7.2 运行时翻译

- C++ `UiPreferences`（注册为 QML 单例 `Ui`）负责：
  - `language`、`theme`、`systemDark`、`rtl`、`strings`、`fallbackStrings`。
  - 用 `QSettings` 持久化 `ui/language`、`ui/theme`。
  - 启动时从 `:/i18n/{code}.json` 读取；缺失 key 回退到 `en`，再回退到 key 本身。
  - 系统主题：Qt 6.5+ 用 `QStyleHints::colorScheme()` 并监听 `colorSchemeChanged`；Qt 6.4 用 `QGuiApplication::palette().color(QPalette::Window).lightness() < 128` 做一次性判断，无法感知运行中变化。两条路径都取不到时回退 `light`。最低 Qt 保持 6.4，不因为“跟随系统”提高门槛。
- QML 单例 `T.qml` 提供 `t(key, args)`、`rtl`、`language`。`t()` 内部读取 `Ui.strings`，使绑定在语言变化时自动重算。
- 占位符用 `%1`…`%9`，由 `t()` 替换；参数可以是数字或字符串。
- Key 命名沿用网页版风格：`nav.overview`、`btn.add_dose`、`label.e2`、`status.dose_added` 等；网页版已有的中文直接复用，减少术语分叉。
- C++ `AppController` 新增：
  - `Q_PROPERTY(QString statusKey)`
  - `Q_PROPERTY(QStringList statusArgs)`
  - 所有 `fail()` / 成功赋值同时设置 key 与 args；`status` 保留英文原文，现有测试和 CLI 不受影响。
  - QML Toast 显示 `T.t(app.statusKey, app.statusArgs)`；`statusKey` 为空时回退 `app.status`。
- 需要翻译的 C++ 状态约 25 条；动态数据库错误把原始错误文本作为 `%1` 参数，不翻译技术细节。

### 7.3 RTL（波斯语）

- `ApplicationWindow.layoutDirection: T.rtl ? Qt.RightToLeft : Qt.LeftToRight`。
- 侧栏通过 `RowLayout` 自动换到右侧；导航图标与文字顺序镜像；文字使用 `T.textAlign`（RTL 时 `Text.AlignRight`）。
- 数字、剂量、单位、时间保持 LTR 数字排版；Qt 的 bidi 算法负责混排。
- 图表坐标保持从左到右（时间轴不镜像）；图例和标题跟随 RTL。
- 方向性图标（返回、 chevron）在 RTL 时水平翻转；中性图标（齿轮、太阳）不翻。
- 表单标签右对齐，输入框文字右对齐；ComboBox 箭头在左侧。

### 7.4 翻译范围

- 页面标题、导航、按钮、字段标签、占位符、空状态、错误、状态、设置分组、语言/主题名称。
- 不翻译：药物代号 `E2`、`CPA`、`T`、单位 `pg/mL`、`ng/mL`、`ng/dL`、路由内部值 `oral` 等（显示时可用本地化名称，但提交给模型的仍是英文枚举）。
- 俄语 / 波斯语初译由实现者完成；医药术语对照英文和中文版本，母语审校后续处理。
- 在 `app/i18n/README.md` 和每种语言的 JSON 文件头注明：欢迎母语者提交 PR 纠正翻译；不要求发布前完成母语审校。中文、英文同样欢迎纠正。

---

## 8. 组件清单

| 文件 | 职责 |
|------|------|
| `Theme.qml`（单例） | 亮/暗 token、圆角、间距、字号、动效时长、字体家族 |
| `T.qml`（单例） | `t(key, args)`、`rtl`、`language`、`textAlign` |
| `Icon.qml` | 统一线性图标（`Shape` + `PathSvg`），支持 `name`、`size`、`color`、`strokeWidth` |
| `Card.qml` | 卡片容器，内边距、圆角、背景、可选悬停 |
| `NavItem.qml` | 侧栏导航项，悬停/选中/按下态，键盘焦点 |
| `AppButton.qml` | primary / secondary / ghost / danger 四种样式 |
| `AppTextField.qml` | 圆角输入框、聚焦环、占位符颜色 |
| `AppComboBox.qml` | 圆角下拉、弹出层、悬停项 |
| `PageHeader.qml` | 标题、说明、右侧操作区 |
| `StatCard.qml` | 读数卡（标签、数值、单位、强调色） |
| `EmptyState.qml` | 图标、标题、说明、操作按钮 |
| `Toast.qml` | 成功/错误提示，自动消失 |
| `ConfirmDialog.qml` | 删除确认，遮罩 + 卡片 |
| `LanguagePicker.qml` | 首次启动语言选择整屏 |
| `Sidebar.qml` | 品牌、导航、连接状态、主题切换 |

页面文件：`Main.qml`、`OverviewPage.qml`、`AddDosePage.qml`、`HistoryPage.qml`、`SettingsPage.qml` 全部重写。

---

## 9. 代码与数据改动

- `app/qml/`：新增上述组件与单例；页面重写。
- `app/i18n/en.json`、`zh.json`、`ru.json`、`fa.json`：扁平 key 表。
- `app/fonts/`：Inter 与 Vazirmatn 的 Regular / Medium / SemiBold；附 `OFL.txt` 和来源说明。
- `app/src/ui_preferences.h/.cpp`：语言、主题、翻译表、系统暗色监听。
- `app/src/app_controller.h/.cpp`：`statusKey` / `statusArgs`；所有状态赋值补 key。
- `app/src/main.cpp`：加载字体、注册 `Ui` 单例、读取 `OYAMA_UI_LANGUAGE`。
- `CMakeLists.txt`：QML 文件、单例属性、字体/i18n 资源、测试环境变量。
- `app/tests/`：更新 QML 冒烟测试，确保首次启动不因语言选择页失败；状态 key 的映射如可测则加断言。
- `THIRD_PARTY_NOTICES.md`：Lucide（ISC）、Inter（OFL）、Vazirmatn（OFL）。

---

## 10. 验证

1. **构建**：Linux Release 76/76；FreeBSD Debug/Release 构建通过。
   - FreeBSD 验收环境已就绪：`/mnt/big/freebsd.qcow2`，FreeBSD 15.1，Qt 6.11.2，源码 `/home/oyama/oyama-src`，Plasma X11，SSH 主机端口 2222，账号 `oyama`/`oyama12345`（root 同密码；root SSH 关闭，用 `su -`）。
2. **测试**：现有 CTest 12/12；QML 冒烟测试在 `OYAMA_UI_LANGUAGE=en` 下通过；设置/历史/记录的交互不回归。
3. **视觉**：Xvfb 1200×800 截图；至少覆盖：
   - 亮色英文概览；
   - 暗色中文概览；
   - 波斯语概览（侧栏在右、文字右对齐）；
   - 语言选择页；
   - 记录剂量成功态；
   - 历史删除确认。
4. **主题**：亮/暗切换时所有页面无残留旧色；图表重新绘制。
5. **语言**：四种语言切换后无截断、无 `%1` 残留、无 key 泄漏；波斯语布局镜像正确。
6. **字体**：FreeBSD 上中文与波斯语不出现方块；如缺字体，文档记录安装包名。
7. **回归**：模型向量、存储、备份、CLI 测试全部通过。

**2026-10-30 FreeBSD 验收记录**

- 分支 `desktop-ui-overhaul`，代码提交 `dcf0fde`（本次构建与测试基于该提交；其后仅有文档提交）；源码以 git bundle 同步到 `/home/oyama/oyama-src`，旧的非 git 源码保留为 `~/oyama-src.pre-ui-overhaul`。
- 环境：FreeBSD 15.1-RELEASE amd64，Qt 6.11.2，CMake 3.31.12，Ninja 1.13.2，PostgreSQL 18.6。
- 构建：`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DOYAMA_BUILD_QT_SHELL=ON` 成功，118/118 个目标完成。
- 测试：CTest **19/19** 通过，耗时 27.62 s（包含 6 个 Qt shell 冒烟用例、`qml_components` 与 `app_controller_test`）。
- 字体：FreeBSD ports 中对应包名为 `noto-sc`（不存在 `font-noto-cjk` 包）；安装后 `fc-list :lang=zh` 可列出 Noto Sans CJK SC。波斯语继续使用随应用打包的 Vazirmatn。
- 截图：在 FreeBSD 上用 `--screenshot` 配合 `QT_QPA_PLATFORM=offscreen`、`QT_QUICK_BACKEND=software` 生成 `zh-light.png`、`fa-light.png`、`fa-dark.png`（1200×800），已复制到主机 `/tmp/opencode/freebsd-shots/` 检查。中文与波斯文字形均正常显示，波斯语侧栏镜像到右侧，暗色主题颜色正确。
- 验收后的修复（commit `7fcff39`）：下拉框委托补上 `required property var model` / `required property int index`，修复选项文字空白；侧栏悬停颜色去掉动画，避免软件渲染下多个悬停背景残留。新增 `qml_components` 回归测试，FreeBSD 重新构建并 **19/19** 通过；在 Plasma X11 中实测路线下拉六个选项均可见，鼠标移出后侧栏无残影。

---

## 11. 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| FreeBSD 缺 CJK / 阿拉伯字体 | 中文或波斯语显示方块 | 打包 Vazirmatn；FreeBSD 安装 Noto CJK / 文泉驿；启动时检测并提示 |
| 翻译术语不一致 | 医药信息误导 | 复用网页版中文；俄/波斯语对照英文；保留英文枚举值 |
| 运行时翻译绑定不刷新 | 切换语言后部分文字不变 | `T.t()` 显式读取 `Ui.strings`；截图逐页验证 |
| RTL 布局错位 | 波斯语不可用 | 用 `RowLayout` + `LayoutMirroring`；逐页截图；图表保持 LTR |
| QML 软件渲染性能 | 动画卡顿 | 不用 shader/模糊/重阴影；只做颜色、透明度、位移、缩放 |
| 状态 key 改动破坏测试 | CI 失败 | 保留英文 `status`；新增 key 不改旧字段 |
| 范围蔓延 | 交付延期 | 只做本文列出的页面与组件；Labs/Plan/Supplies 不做 |

---

## 12. 实施顺序

1. `UiPreferences` + `T.qml` + 四份 JSON + 字体资源；语言选择页。
2. `Theme.qml` + 基础组件（Icon、Card、Button、TextField、ComboBox、NavItem）。
3. `Main.qml` + `Sidebar.qml` 外壳；页面切换动画；Toast。
4. 概览页（读数卡、曲线、空状态、最近记录）。
5. 记录剂量页（表单、条件字段、成功态）。
6. 历史页（列表、悬停、删除确认、空状态）。
7. 设置页（分组、语言、主题、保存）。
8. 暗色与 RTL 全页面走查；字体与对比度修正。
9. Linux Release 构建 + CTest + 截图；FreeBSD 同步构建 + CTest + 截图。
10. 安装到 `~/oyama-hrt-dist`，交付路径与运行说明。

---

## 13. Builder check 处理记录

实现前让一个不了解上下文的新构建者读过本文，提出 6 个会改变计划的问题；处理如下：

| # | 问题 | 处理 |
|---|------|------|
| 1 | 没有数据库时，原来的 footer 提示被移除，空状态 CTA 会失败 | **本文件决定**：不加阻塞式数据库设置页；保留主界面，顶部常驻提示条 + 侧栏“未连接” + 空状态 CTA 禁用。语言选择页先于一切。不新增 `connecting` 等连接状态，沿用 `app.backend` 的 `none`/`managed`/`existing`。 |
| 2 | FreeBSD 验收环境是什么 | **已具备**：`/mnt/big/freebsd.qcow2`，FreeBSD 15.1，Qt 6.11.2，源码 `/home/oyama/oyama-src`，Plasma X11；SSH 2222，`oyama`/`oyama12345`。 |
| 3 | 是否把最低 Qt 从 6.4 提到 6.5 | **本文件决定**：保持 6.4。Qt 6.5+ 用 `QStyleHints::colorScheme`；6.4 用调色板明度做一次性判断；未知时回退 light。“跟随系统”在 6.4 上是尽力而为，文档写明。 |
| 4 | 曲线画几条 | **本文件决定**：只画 E2；CPA/T 只出现在读数卡。`chartLine` 就是 `accent`。多序列留到后续版本。 |
| 5 | WCAG AA 是硬门槛还是 hex 值优先 | **本文件决定**：小字可读性优先，但不改变整体色相。新增 `textSoft` 保留 `#7A7466` 给 20px+；15px/nav 用略深的 `textMuted` `#6E6759`；11–13px 用 `#6B6457`；暗色 `textFaint` 用 `#9A9385`；主按钮改用暖深色字 `#2B241B`（和网页版浅色按钮一致）。其余 hex 按表执行。 |
| 6 | transfem/transmasc 切换放哪 | **本文件决定**：放侧栏底部，全局可用；切换会重载数据并清空未保存的剂量表单。 |

第 1、3、4、5、6 项都是实现者可以自行决定的；如果所有者对其中任何一项有不同偏好，在批准时指出即可。

## 14. 所有者确认结果（2026-10-30）

1. 波斯语**整界面镜像**：侧栏到右侧，布局 RTL。已确认。
2. 俄语 / 波斯语**接受“实现者初译 + 后续母语审校”**；在翻译文件和 README 中写明欢迎母语者提交 PR 纠正。已确认。
3. 语言选择使用**整屏欢迎页**，不是进入应用后弹窗。已确认。

设计已获批准，进入实现计划阶段。
