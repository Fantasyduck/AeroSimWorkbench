# AeroSimWorkbench UI 开发规范

> 状态：现行规范（以本仓库 `modules/ui` 实现为准）
> 技术基线：C++14、Qt 5.12.12、SARibbon 2.9.3，所有尺寸为 **Qt 逻辑像素**

本文档定义 AeroSimWorkbench 主界面（MainWindow）+ 全局布局配置（UiLayoutConfig）+ 中央功能页面（Page）之上的**页面开发契约**，使不同模块开发人员能依据同一套规范独立开发页面，并保证：统一排版、统一布局持久化规则、跨分辨率一致、可框架级回归验证。

阅读顺序建议：先看 §1 架构职责、§2 公共参数，再按 §3/§5 开发页面，遇到保存恢复问题查 §6，验收对照 §7，最后检查 §8 常见错误。

---

## 第一章 UI 整体架构

主要参与对象：

```
MainWindow  (SARibbonMainWindow + IWidgetHost)
 ├─ SARibbonBar                    顶部功能导航（Ribbon）
 ├─ projectDock（工作区树 workspaceTree）   左侧全局 Dock
 ├─ propertyDock（属性编辑器）             左侧全局 Dock
 ├─ outputDock（全局输出面板）             底部全局 Dock
 ├─ statusBar                          状态栏
 ├─ centralWorkspace (QStackedWidget)    中央页面容器（centralWidget）
 │   ├─ viewportPage_ / 各功能 Page
 │   └─ ...                             功能页面
 └─ UiLayoutConfig                     全局布局参数与屏幕/DPI 助手
```

### 职责与调用关系

| 对象 | 职责 | 拒绝承担 |
| --- | --- | --- |
| **MainWindow** | 主窗口唯一布局管理者：Ribbon、工作区树、中央容器、全部全局 Dock、输出面板、状态栏、页面导航切换、主窗口尺寸/位置、布局保存/恢复/默认、屏幕适配 | 业务参数校验、任务调度（交给注入的服务） |
| **UiLayoutConfig** | 全局布局默认值、约束与计算辅助的**单一来源**；不保存运行时尺寸、不充当可变全局状态容器 | 不管理具体页面内部布局 |
| **中央页面容器**（`centralWorkspace` 即 QStackedWidget） | 分配中央区域尺寸给当前页面、维护页面实例生命周期、承载页面切换 | 不添加全局外边距（外边距归页面根布局） |
| **功能 Page** | 自身控件创建与排列、内部 Layout / QSplitter / 表单图表 / 局部滚动 / 局部 UI 状态 | 决定主窗口分辨率、直接改全局 Dock、自行持久化全局布局 |

调用方向为单向依赖：**MainWindow 拥有并驱动页面**；页面只读公共参数、不反向操作全局框架。

---

## 第二章 公共布局参数

UiLayoutConfig（`modules/ui/include/aerosim/ui/UiLayoutConfig.h`）按**三层**组织。所有像素为 Qt 逻辑像素；比值为无单位小数。1920×1080 仅为设计参考，不是强制运行尺寸。

### 第一层：全局窗口参数（MainWindow / 顶层窗口专用）

| 参数 | 含义 | 默认值 | 单位 | 页面可否使用 |
| --- | --- | --- | --- | --- |
| `defaultWindowSize()` | 主窗口默认尺寸 | 1440×960 | 逻辑像素 | **否**（页面不得据此改自身尺寸） |
| `minimumWindowSize()` | 主窗口最小尺寸 | 1000×720 | 逻辑像素 | **否** |
| `designReferenceSize()` | 1920×1080 设计参考，仅供比例推演 | 1920×1080 | 逻辑像素 | 可读作比例参考 |
| `availableRect(widget)` | widget 所在屏幕可用区域 | 当前屏幕可用矩形 | 逻辑像素 | 顶层窗口布局计算使用 |

### 第二层：通用布局参数（页面可安全读取）

| 参数 | 含义 | 默认值 | 单位 | 适用场景 |
| --- | --- | --- | --- | --- |
| `pageMargins()` | 页面根布局外边距 | (24,20,24,20) | 逻辑像素 | 页面根容器统一外边距 |
| `sectionSpacing()` | 区块间间隔 | 16 | 逻辑像素 | 章节/模块分隔 |
| `panelSpacing()` | 控件行间距 | 12 | 逻辑像素 | 表单、工具栏、按钮行 |
| `controlSpacing()` | 紧密控件间距 | 8 | 逻辑像素 | 标签+输入、成对按钮 |
| `controlHeight()` | 控件基准高度（提示，非硬约束） | 28 | 逻辑像素 | 输入/列表行高提示 |
| `sectionTitleHeight()` | 区块标题高 | 28 | 逻辑像素 | 章节标题栏 |

### 第三层：布局约束与默认比例（MainWindow 布局管理器使用）

| 参数 | 含义 | 默认值 | 单位 |
| --- | --- | --- | --- |
| `projectDockWidth()` / `projectDockMinimumWidth()` | 工作区树 dock 默认/最小宽 | 260 / 220 | 逻辑像素 |
| `propertyDockMinimumWidth()` / `propertyDockDefaultWidth()` | 属性 dock 最小/默认宽 | 180 / 330 | 逻辑像素 |
| `outputExpandedHeight()` / `outputCollapsedHeight()` | 输出面板展开/收起高 | 140 / 40 | 逻辑像素 |
| `embeddedPageDockWidth()` | 内嵌页面时工作区树宽 | 260 | 逻辑像素 |
| `defaultProjectDockRatio()` … | 各 dock 默认比例 | 0.18 / 0.23 / 0.15 | 无单位 |
| `clampProjectDockRatio()` / `clampPropertyDockRatio()` / `clampOutputRatio()` | 将持久化比例收敛到安全区间 | — | 无单位 |

### 屏幕 / DPI 助手

- `screenOf(widget)`：widget 所在屏幕；独立弹窗优先父窗口所在屏幕；无窗口系统时回退到主屏或返回 `nullptr`。
- `availableRect(widget)`：可用矩形；无屏幕时回退到设计参考矩形。
- `logicalDpi` / `scaleFactor`：逻辑 DPI 与相对 96 的缩放，**仅作信息与罕见像素级目标**。

> 不要在页面里对所有尺寸再乘 `scaleFactor()`——Qt 已按高 DPI 自动缩放字体。常见的取整、边距、间距直接用第二层常量即可，避免重复缩放路径。

---

## 第三章 页面开发规范

### 3.1 根容器

- 根容器用 `QWidget`（或兼容 Qt Widget），由中央容器分配实际尺寸。
- 根节点不要调用 `setFixedSize()`，不要强制整屏分辨率。
- 通过根布局管理内部控件，并设置合理的 `QSizePolicy::Expanding`。

### 3.2 根布局与边距所有权

**约定：统一外边距由页面根布局负责，中央容器不添加外边距。** 普通表单/说明页面的根布局只应用一次 `pageMargins()`；内部区块再用 `sectionSpacing/panelSpacing/controlSpacing` 组织，二者明确区分。

标准根布局通过框架助手创建：

```cpp
auto *page = new QWidget(this);                 // 或由中央容器 new QWidget(parent)
QVBoxLayout *root = aerosim::uimetrics::createPageRootLayout(page);
root->addWidget(new QLabel(QStringLiteral("标题"), page));
root->addStretch();
```

`createPageRootLayout` 已设置 `pageMargins()` 与 `panelSpacing()`。内部如需更紧凑布局，可使用更小间距的局部 QHBoxLayout/QGridLayout，但不改变根外边距。

网格、VTK 视口、画布或多分隔条工作台属于**沉浸式工作台页**：若页头、画布或状态条需要贴合中央容器边缘，使用 `createWorkbenchRootLayout(page)`。该根布局固定为零外边距和零间距；页头直接加入根布局，正文再包入一个 content wrapper，并由 wrapper **恰好一次**应用 `pageMargins()`。这不是缩小普通表单间距的捷径。

### 3.3 控件尺寸策略

- 输入/列表行以 `controlHeight()` 作为高度提示，而不是硬编码一个固定高度。
- 区块标题用 `sectionTitleHeight()` 作为最小高度。
- 内容超出可用空间时，用 `QScrollArea` 或合理的内部 `QSplitter` 处理，不要通过放大主窗口掩盖。

### 3.4 内部布局多样性

框架不强制所有页面内部使用同一比例。表单布局、左右/上下分栏、图表+列表组合均允许；框架只约束**公共行为**（根外边距、页面向外适配、不得改全局尺寸）。

### 3.5 生命周期与切换

以现有 `centralWorkspace`（QStackedWidget）机制为准：

1. 页面在 `MainWindow::createWorkspace()` 一次性创建并 `addWidget` 到容器。
2. 页面通过 `workspacePages_` 映射注册：`addWidget` + 插入一个 pageId。
3. 切换时**复用实例**，不重新创建（`setCurrentWidget` 只切换索引）。
4. 页面隐藏后保留其局部 UI 状态；重新显示时由布局/页面自己保持，不在切换时无条件重置全局布局。
5. 避免重复初始化 VTK 等重控件：重控件在装配层注入一次、复用。
6. 避免 Ribbon 与工作区树双向联动造成重复切换：`showWorkspacePage` 内部以 `syncingRibbonTree_` 短路；模块不要自己再触发联动。

---

## 第四章 禁止事项

1. **不得**在中央页面根节点固定整屏分辨率（如 `setFixedSize(1920,1080)`）。
2. **不得**在功能页面调用主窗口 `resize()` 改变整个应用窗口。
3. **不得**自行决定主窗口/全局尺寸或强制整屏分辨率作为页面尺寸。
4. **不得**直接调整工作区树或全局输出面板的全局 Dock 尺寸；如需显示/隐藏公共 Dock，通过 MainWindow 已有的受控入口（或新增的最小化接口）请求。
5. **不得**独立定义与 UiLayoutConfig 冲突的全局布局参数；局部越权数值一律使用第二层公共参数。
6. **不得**在功能页面自行保存另一份全局布局配置；全局布局持久化只经 `MainWindow::saveUserLayout/restoreUserLayout`。
7. **不得**因初始化或页面切换无条件重置用户已经调整的全局布局。
8. **不得**通过扩大主窗口来掩盖页面内部尺寸管理问题。

---

## 第五章 页面接入流程

1. **创建页面**：`auto *page = new MyPage(containerParent);`
2. **设置根布局**：`auto *root = uimetrics::createPageRootLayout(page);`，再添加内部控件。
3. **使用 UiLayoutConfig**：内部间距/高度读第二层常量，不用魔法数字。
4. **接入中央页面管理**：在 `MainWindow::createWorkspace()` 中 `workspaceStack_->addWidget(page)` 并插入 `workspacePages_`（及需要时的模块映射 / 内部 stack 索引，参照智能数据处理页面）。
5. **实现局部布局状态（可选）**：提供 `layoutState()` / `restoreLayoutState()` 键值对方法，由主窗口保存调用；未接入的页面沿用默认行为，不强求。
6. **验证**：切换页面复用实例、父容器尺寸变化后页面跟随、不改变主窗口尺寸、不在 offscreen 下崩溃。

### 简化接入示例

```cpp
#include "aerosim/ui/UiLayoutConfig.h"

// MyPage.h
class MyPage : public QWidget {
    Q_OBJECT
public:
    explicit MyPage(QWidget *parent = nullptr);
    QVariantMap layoutState() const;
    void restoreLayoutState(const QVariantMap &state);
private:
    QSplitter *splitter_ = nullptr;
};

// MyPage.cpp
MyPage::MyPage(QWidget *parent)
    : QWidget(parent) {
    auto *root = aerosim::uimetrics::createPageRootLayout(this);
    auto *splitter = new QSplitter(this);
    splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    root->addWidget(splitter);
    splitter_ = splitter;
    root->addStretch();
}

QVariantMap MyPage::layoutState() const {
    const QList<int> sizes = splitter_->sizes();
    return QVariantMap{{QStringLiteral("splitter"), sizes}};
}

void MyPage::restoreLayoutState(const QVariantMap &state) {
    const QList<int> sizes = state.value(QStringLiteral("splitter")).toList().toIntList();
    if (sizes.size() >= 2) splitter_->setSizes(sizes);
}
```

> 以上不创建真实业务页；页面接入中央容器后即以统一外边距嵌入并随容器自适应。

---

## 第六章 布局保存与恢复

### 6.1 三种布局状态

| 状态 | 含义 | 由谁持有 |
| --- | --- | --- |
| **默认布局** | 框架默认尺寸/比例/可见性 | UiLayoutConfig 默认值 + `MainWindow::restoreLayout()` |
| **当前运行时布局** | 本次运行中用户的拖动/展开/收起 | MainWindow 内存（如 `projectDockWidth_`、Dock 可见性） |
| **用户保存布局** | 用户主动保存的唯一记录 | QSettings，作用域默认 `AeroSimWorkbench/UiLayout`，组 `UserLayout` |

### 6.2 优先级

1. 首次启动 / 恢复默认布局 → 应用框架默认值。
2. 运行中用户调整 → 尊重当前运行时状态。
3. 用户保存 → 覆盖唯一 `UserLayout` 记录。
4. 用户「恢复上次布局」→ 读取最近一次保存记录。
5. 用户「恢复默认布局」→ **不删除**已保存的个人布局。
6. 页面切换 **不自动覆盖**用户保存记录。
7. 已保存记录无效/损坏 → 安全回退到默认布局。

### 6.3 全局 vs 局部布局

- 全局：主窗口尺寸/状态、工作区树宽、全局 Dock 显示、输出面板高、Ribbon 展开态 → 都由 MainWindow 管理。
- 局部：页面内部 QSplitter 比例、可折叠区、局部面板 → 由页面自身持有。

页面可选通过 `layoutState()`/`restoreLayoutState()` **注册自身的局部布局状态**，由主窗口在保存/恢复时调用。规则：

- 不强制所有页面立即实现该接口；未接入的沿用默认行为。
- 不为每个页面建立独立全局布局文件；统一保存在同一 `UserLayout` 组。
- 不为保存布局强制初始化隐藏页面；仅在页面已被创建时读取。
- 优先复用 QSettings、QMainWindow（saveGeometry/saveState）与 QSplitter 状态机制，不额外新建抽象层。

### 6.4 跨分辨率恢复

恢复时：

- 优先维持合理比例（存/读比例而非裸像素，见 `restoreUserLayout` 的 ratio 读取）。
- 用 `clampProjectDockRatio()/clampPropertyDockRatio()/clampOutputRatio()` 收敛到安全区间，避免面板尺寸为零或超大。
- 通过 `constrainToAvailableScreen()` 把窗口约束在当前可用屏内，兼容最大化/普通态与多显示器。
- 恢复时机注意：Widget 尚未完成首次布局、宽高为零或尚未显示时，不在依赖实际尺寸的比例恢复处立即执行；必要时 `QTimer::singleShot(0,…)` 在布局稳定后恢复一次，避免重复执行与无限调整。

---

## 第七章 UI 验收规范

| 验收项 | 操作 | 通过标准 |
| --- | --- | --- |
| 默认窗口尺寸 | 无存档启动 | 主窗口为 1440×960（可被较小屏钳制），最小尺寸 1000×720 生效 |
| 较小窗口 | 缩到接近最小值 | 中央页面不溢出、Dock 不小于公共最小宽 |
| 较大窗口 | 拉大到接近整屏 | 页面随容器扩展，Splitter 正常分配，无残影 |
| 页面切换 | 在不同功能页来回切换 | 复用实例、状态保留、主窗口尺寸不变、无重复 Widget |
| 窗口拖动/缩放 | 拖动、缩放 | 各区域比例合理，不出现面板宽度为零 |
| 布局保存/恢复 | 保存→调整→恢复 | 「恢复上次布局」回到刚保存状态；「恢复默认布局」不删除个人保存 |
| 高 DPI | 在高分屏查看 | 字体自动缩放，页面不因 `scaleFactor()` 重复放大 |
| 多显示器人工验证 | 拖到辅助屏 | 窗口约束在目标屏可用区域，Dock 比例保持 |
| 帮助标签 | 切到「帮助」Ribbon | 不改变当前业务页面 |
| 回归 | 运行框架测试 | `ui_layout_framework` 等 CTest 全绿 |

---

## 第八章 常见错误示例

| 错误 | 正确 |
| --- | --- |
| `page->setFixedSize(1920, 1080);` | 用根布局 + `page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);` |
| 在页面内 `window()->resize(1440, 960);` | 不改主窗口；通过根布局/滚动区适配容器 |
| 页面手写 `setContentsMargins(24,20,24,20);` | `uimetrics::createPageRootLayout(this)`，或读 `pageMargins()` |
| 页面再 `projectDock()->setMinimumWidth(320);` | 全局 Dock 归属 MainWindow，页面不直接调整 |
| 页面另存一份自己定义的分辨率常量 | 复用第二/三层公共常量 |
| 页面写第二个 QSettings 保存全局布局 | 全局布局只经主窗口保存/恢复入口 |
| 切换页面时无条件重置窗口/样式 | 保留运行时与用户保存状态，仅在意图恢复时处理 |

---

## 附：文档与代码一致性

本文档的公共接口与常量以 `modules/ui/include/aerosim/ui/UiLayoutConfig.h`、`modules/ui/src/UiLayoutConfig.cpp` 和 `modules/ui/src/MainWindow.cpp` 的实现为准；若发现描述与实现不符，以代码为准并更新本文档。框架级回归见 `modules/ui/tests/UiLayoutFrameworkTest.cpp`（CMake 目标 `ui_layout_framework`）。
