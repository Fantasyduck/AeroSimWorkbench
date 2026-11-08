#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QColorDialog>
#include <QSignalBlocker>
#include <QDockWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QStandardPaths>
#include <QFileDialog>
#include <QFileInfo>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTextDocument>
#include <QFormLayout>
#include <QFrame>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListWidget>
#include <QMap>
#include <QMenu>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QQuaternion>
#include <QVector3D>
#include <QVector>
#include <QPair>
#include <algorithm>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QPixmap>
#include <QSlider>
#include <QScrollArea>
#include <QSvgRenderer>
#include <QTimer>
#include <QThread>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>

#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>

#include <functional>
#include <QtAlgorithms>

static QWidget *initializePostProcessResources(QWidget *parent)
{
    Q_INIT_RESOURCE(PostProcessIcons);
    Q_INIT_RESOURCE(SARibbonResource);
    return parent;
}

namespace aerosim {
namespace {
template <size_t N> QString uiText(const char16_t (&text)[N])
{ return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1); }

QLabel *sectionLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent); label->setObjectName(uiText(u"sectionTitle")); return label;
}
// Render the same SVG line artwork at each requested size, including high DPI.
QIcon commandIcon(const QString &kind) {
    static const QMap<QString, QString> aliases = {
        {"import", "import-config"}, {"export", "export-config"},
        {"trace", "animation"}, {"analysis", "curve"}, {"add", "new-state"}};
    const QString key = aliases.value(kind, kind);
    QSvgRenderer renderer(QStringLiteral(":/aerosim/postprocess/") + key + QStringLiteral(".svg"));
    if (!renderer.isValid()) return QIcon();
    QIcon icon;
    for (int size : {16, 20, 24, 32, 40, 48, 64, 96}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

QWidget *scrollPanel(QWidget *contents) {
    auto *scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(contents);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return scroll;
}

// Native QDockWidget title metrics do not include QSS padding on Qt 5.12.
// A real layout gives the caption and controls their full font-derived height.
void installDockTitle(QDockWidget *dock) {
    auto *bar = new QWidget(dock);
    bar->setObjectName(QStringLiteral("postProcessDockTitle"));
    bar->setStyleSheet(QStringLiteral(
        "QWidget#postProcessDockTitle { background:#e9eef5; }"
        "QLabel { font:9pt 'Microsoft YaHei UI'; color:#25364c; }"
        "QToolButton { border:0; background:transparent; padding:3px; }"
        "QToolButton:hover { background:#dceafd; }"));
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8,3,4,3);
    layout->setSpacing(2);
    auto *label = new QLabel(dock->windowTitle(), bar);
    label->setObjectName(QStringLiteral("postProcessDockCaption"));
    layout->addWidget(label, 1);
    auto *floating = new QToolButton(bar);
    floating->setIcon(commandIcon("dock")); floating->setIconSize(QSize(16,16));
    floating->setToolTip(uiText(u"切换浮动 / 停靠"));
    auto *close = new QToolButton(bar);
    close->setIcon(commandIcon("close")); close->setIconSize(QSize(16,16));
    close->setToolTip(uiText(u"关闭面板"));
    layout->addWidget(floating); layout->addWidget(close);
    QObject::connect(floating, &QToolButton::clicked, dock, [dock] { dock->setFloating(!dock->isFloating()); });
    QObject::connect(close, &QToolButton::clicked, dock, &QWidget::hide);
    dock->setTitleBarWidget(bar);
}

QAction *ribbonAction(QWidget *owner, const QIcon &icon, const QString &text,
                      const QString &tip, const QString &objectName) {
    auto *action = new QAction(icon, text, owner);
    action->setObjectName(objectName); action->setToolTip(tip); action->setProperty("availableToolTip", tip);
    return action;
}

// Shown only until a real VTK widget is inserted. This is a visual preview, not a fake data source.
class ResultPreviewWidget final : public QWidget {
public:
    explicit ResultPreviewWidget(QWidget *parent = nullptr) : QWidget(parent) { setMinimumSize(280, 200); }
protected:
    void paintEvent(QPaintEvent *) override {
        // Keep the preview illustrative, but use the same light workspace
        // surface as the main window's viewport placeholder.
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing); QLinearGradient bg(rect().bottomLeft(), rect().topLeft()); bg.setColorAt(0, property("backgroundBottom").value<QColor>()); bg.setColorAt(1, property("backgroundTop").value<QColor>()); p.fillRect(rect(), bg);
        if (!property("showExample").toBool()) { p.setPen(QColor("#334155")); p.drawText(rect(), Qt::AlignCenter, uiText(u"导入可视化数据以开始查看")); return; }
        const QRect area = rect().adjusted(28, 58, -86, -48);
        QLinearGradient gradient(area.topLeft(), area.bottomRight());
        gradient.setColorAt(0, QColor("#174f88")); gradient.setColorAt(.32, QColor("#10a4bd")); gradient.setColorAt(.58, QColor("#79c76f")); gradient.setColorAt(.78, QColor("#f6c84b")); gradient.setColorAt(1, QColor("#d94e4e"));
        QPainterPath body; body.moveTo(area.left() + 12, area.center().y());
        body.cubicTo(area.left() + area.width() * .27, area.top() + 14, area.left() + area.width() * .68, area.top() + 24, area.right() - 12, area.center().y() - 8);
        body.cubicTo(area.left() + area.width() * .68, area.bottom() - 20, area.left() + area.width() * .25, area.bottom() - 24, area.left() + 12, area.center().y());
        p.setBrush(gradient); p.setPen(Qt::NoPen); p.drawPath(body);
        p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor("#c4d2e0"), 1));
        for (int i = 1; i < 9; ++i) { const int x = area.left() + area.width() * i / 9; p.drawLine(x, area.top() + 30, x, area.bottom() - 30); }
        for (int i = 1; i < 6; ++i) { const int y = area.top() + area.height() * i / 6; p.drawLine(area.left() + 24, y, area.right() - 24, y); }
        p.setPen(QPen(QColor("#234d7a"), 2)); p.drawPath(body);
        const QRect legend(rect().right() - 45, 80, 13, qMax(40, height() - 175)); QLinearGradient lg(legend.topLeft(), legend.bottomLeft());
        lg.setColorAt(0, QColor("#d94e4e")); lg.setColorAt(.35, QColor("#f6c84b")); lg.setColorAt(.67, QColor("#10a4bd")); lg.setColorAt(1, QColor("#174f88")); p.fillRect(legend, lg); p.setPen(QColor("#8fa5bb")); p.drawRect(legend);
        p.setPen(QColor("#25364c")); p.drawText(QRect(20, 14, width() - 50, 30), Qt::AlignLeft | Qt::AlignVCenter, uiText(u"界面示例（非真实数据）"));
        p.setPen(QColor("#52677f")); p.drawText(QRect(20, height() - 34, width() - 40, 20), Qt::AlignLeft | Qt::AlignVCenter, uiText(u"翼身组合 / 压力场 / 时间步 0"));
        p.drawText(QRect(width() - 65, 52, 50, 24), Qt::AlignCenter, uiText(u"压力"));
    }
};
// UI orientation preview. A renderer adapter must still apply camera requests.
class OrientationControl final : public QWidget {
public:
    explicit OrientationControl(QWidget *parent = nullptr) : QWidget(parent) {
        setFixedSize(112, 112); setCursor(Qt::OpenHandCursor);
        setToolTip(uiText(u"拖动预览观察方向；点击 ±X / ±Y / ±Z 切换标准视角。三维相机待接入。"));
        setAccessibleName(uiText(u"三维方向导航"));
    }
    std::function<void(const QString &)> onView;
    std::function<void(int, int)> onRotate;
    void setView(const QString &view) {
        if (view == "front") rotation_ = QQuaternion::fromEulerAngles(0, 0, 0);
        else if (view == "back") rotation_ = QQuaternion::fromEulerAngles(0, 180, 0);
        else if (view == "right") rotation_ = QQuaternion::fromEulerAngles(0, -90, 0);
        else if (view == "left") rotation_ = QQuaternion::fromEulerAngles(0, 90, 0);
        else if (view == "top") rotation_ = QQuaternion::fromEulerAngles(90, 0, 0);
        else if (view == "bottom") rotation_ = QQuaternion::fromEulerAngles(-90, 0, 0);
        else if (view == "reset") rotation_ = QQuaternion::fromEulerAngles(24, -32, 0);
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(110, 132, 154, 85), 1));
        p.setBrush(QColor(245, 248, 252, 160)); p.drawEllipse(QPointF(56,56), 48,48);
        // Back directions are drawn first so the front directions remain selectable.
        for (const auto &node : nodes()) {
            QColor color = node.color; if (node.depth < 0) color.setAlpha(130);
            p.setPen(QPen(color, node.positive ? 2.5 : 1.2)); p.drawLine(QPointF(56,56), node.point);
            p.setBrush(node.positive ? color : QColor(242,246,250,220)); p.drawEllipse(node.point, 11,11);
            p.setPen(node.positive ? Qt::white : color.darker(130));
            p.drawText(QRectF(node.point-QPointF(12,12), QSizeF(24,24)), Qt::AlignCenter, node.label);
        }
    }
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) { previous_ = start_ = event->pos(); dragging_ = false; setCursor(Qt::ClosedHandCursor); }
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (!(event->buttons() & Qt::LeftButton)) return;
        if (!dragging_ && (event->pos()-start_).manhattanLength() < 4) return;
        const QPoint delta = event->pos()-previous_; previous_ = event->pos(); dragging_ = true;
        rotation_ = (QQuaternion::fromAxisAndAngle(0,1,0,delta.x()*.7f)
                    * QQuaternion::fromAxisAndAngle(1,0,0,delta.y()*.7f) * rotation_).normalized();
        update();
        if (onRotate && !delta.isNull()) onRotate(delta.x(), delta.y());
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) return;
        setCursor(Qt::OpenHandCursor);
        if (dragging_ || !onView) return;
        const auto directions = nodes();
        for (auto it = directions.crbegin(); it != directions.crend(); ++it) {
            const QPointF delta = event->pos()-it->point;
            if (QPointF::dotProduct(delta,delta) <= 144) { setView(it->view); onView(it->view); break; }
        }
    }
private:
    struct Node { QPointF point; float depth; QColor color; QString label; QString view; bool positive; };
    QVector<Node> nodes() const {
        QVector<Node> result;
        const QVector3D axes[] = {{1,0,0},{0,1,0},{0,0,1}};
        const QColor colors[] = {QColor("#c95151"),QColor("#419b59"),QColor("#387fc1")};
        const QString views[] = {"right","left","top","bottom","front","back"};
        for (int axis=0; axis<3; ++axis) for (int sign : {1,-1}) {
            const QVector3D v = rotation_.rotatedVector(axes[axis]*float(sign));
            result.append({QPointF(56+v.x()*35,56-v.y()*35),v.z(),colors[axis],
                           (sign>0 ? QString() : QStringLiteral("-"))+QString(QChar('X'+axis)),
                           views[axis*2+(sign<0 ? 1 : 0)],sign>0});
        }
        std::sort(result.begin(),result.end(),[](const Node &a,const Node &b) { return a.depth < b.depth; });
        return result;
    }
    QQuaternion rotation_ = QQuaternion::fromEulerAngles(24, -32, 0);
    QPoint previous_; QPoint start_; bool dragging_ = false;
};

class ViewportHost final : public QFrame {
public:
    explicit ViewportHost(QWidget *parent = nullptr) : QFrame(parent) {}
    QWidget *orientation = nullptr;
    QWidget *ruler = nullptr;
protected:
    void resizeEvent(QResizeEvent *event) override {
        QFrame::resizeEvent(event);
        if (orientation) orientation->move(qMax(0, width() - orientation->width() - 12), 12);
        if (ruler) ruler->move(12, qMax(0, height() - ruler->height() - 12));
    }
};

}

PostProcessWorkbenchWindow::PostProcessWorkbenchWindow(QWidget *parent)
    : SARibbonMainWindow(initializePostProcessResources(parent)) {
    setObjectName(uiText(u"postProcessWorkbench")); setWindowTitle(uiText(u"AeroSimWorkbench · 可视化分析台")); resize(1440, 960); setMinimumSize(1120, 680);
    QFont workbenchFont(uiText(u"Microsoft YaHei UI")); workbenchFont.setPointSize(9); setFont(workbenchFont);
    setRibbonTheme(SARibbonTheme::RibbonThemeOffice2013);
    ribbonBar()->setRibbonStyle(SARibbonBar::RibbonStyleLooseThreeRow);
    ribbonBar()->applicationButton()->hide();
    ribbonBar()->setWindowTitleBackgroundBrush(QColor("#e5e4e4"));
    ribbonBar()->setWindowTitleTextColor(QColor("#222222"));
    createActions();
    auto *workbench = new QWidget(this); auto *root = new QVBoxLayout(workbench); root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
    workbench->setObjectName(QStringLiteral("postProcessWorkspace"));
    auto *content = new QSplitter(Qt::Horizontal, workbench); content->setObjectName(uiText(u"postProcessContent"));
    auto *tabs = new QTabWidget(content); tabs->setObjectName(uiText(u"postProcessNavigator")); tabs->setMinimumWidth(250);
    navigationPanels_[1] = createDataPanel(); navigationPanels_[2] = createObjectPanel();
    tabs->addTab(navigationPanels_[1], uiText(u"数据")); tabs->addTab(navigationPanels_[2], uiText(u"对象"));
    tabs->setTabsClosable(true);
    connect(tabs, &QTabWidget::tabCloseRequested, this, [tabs](int index) {
        tabs->removeTab(index);
        if (tabs->count() == 0) tabs->hide();
    });
    content->addWidget(tabs); content->addWidget(createViewportPanel()); content->setStretchFactor(1, 1); content->setSizes({270, 1170}); content->setChildrenCollapsible(false);
    root->addWidget(content, 1); root->addWidget(createTimeline()); setCentralWidget(workbench);
    statusBar()->showMessage(uiText(u"请选择可视化数据；读取服务尚未接入。"));
    // Scope workspace styling so SARibbon's buttons, tabs and window controls
    // retain their own metrics. Native inputs use the same style as MainWindow.
    workbench->setStyleSheet(QStringLiteral(
        "QWidget { font-family: 'Microsoft YaHei UI'; font-size:9pt; }"
        "QWidget#postProcessWorkspace { background:#e9eef5; color:#25364c; }"
        "QTabWidget::pane { border:0; background:#efefef; }"
        "QTabBar::tab { background:#e8e8e8; color:#222; border:1px solid #bfc4cb; padding:5px 12px; margin-right:1px; }"
        "QTabBar::tab:selected { background:#fff; border-bottom-color:#fff; }"
        "QTabBar::tab:hover:!selected { background:#e0eaf6; }"
        "QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox { background:#fff; color:#222; border:1px solid #aeb5be; padding:4px 6px; }"
        "QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; width:22px; border:0; }"
        "QComboBox::down-arrow { image:url(:/aerosim/postprocess/dropdown.svg); width:10px; height:6px; }"
        "QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QDoubleSpinBox:focus { border-color:#2563b6; }"
        "QPushButton { background:#fafafa; color:#222; border:1px solid #aeb5be; padding:5px 8px; }"
        "QPushButton:hover { background:#e0eaf6; border-color:#8baed7; }"
        "QPushButton:pressed { background:#dceafd; }"
        "QPushButton:focus { border-color:#2563b6; }"
        "QScrollArea { border:0; background:#efefef; }"
        "QSplitter::handle { background:#e9eef5; }"
        "QFrame#viewportFrame { border:0; background:#edf2f8; }"
        "QFrame#viewportHeader { border:0; background:#e9eef5; }"
        "QLabel#sectionTitle,QLabel#viewportTitle { color:#234d7a; font-weight:600; }"
        "QLabel#mutedText { color:#52677f; }"
        "QFrame#dataCard { border:0; background:transparent; }"
        "QTreeWidget,QListWidget { border:0; background:white; alternate-background-color:#f4f7fb; }"
        "QTreeWidget::item,QListWidget::item { padding:5px 2px; }"
        "QTreeWidget::item:selected,QListWidget::item:selected { background:#dceafd; color:#153b6b; }"

        "QFrame#postProcessTimeline { border:0; background:#e9eef5; }"
        "QToolButton { border:1px solid transparent; padding:4px; background:transparent; }"
        "QToolButton:hover { background:#e0eaf6; border-color:#b8cde6; }"
        "QToolButton:checked { background:#dceafd; border-color:#9cbce1; }"));
    QTimer::singleShot(0, this, [this] {
        setStyleSheet(styleSheet() + QStringLiteral(
            "SARibbonBar { background:#e5e4e4; }"
            "SARibbonTabBar::tab { color:#222; border:0; margin:0; padding:0 12px; }"
            "SARibbonTabBar::tab:selected { color:#2563b6; background:#fff; border-bottom:2px solid #2563b6; }"
            "SARibbonStackedWidget { border:0; }"
            "SARibbonToolButton { border-radius:0; }"
            "QDockWidget::title { background:#e9eef5; padding:3px; color:#25364c; font:9pt 'Microsoft YaHei UI'; }"
            "QStatusBar { background:#e9eef5; color:#25364c; }"
            "QStatusBar,QStatusBar QLabel { font:9pt 'Microsoft YaHei UI'; }"
            "QStatusBar::item { border:0; }"));
    });
    applyBackground(backgroundBottom_, backgroundTop_, gradientBackground_);
    timeSlider_->setEnabled(false);
    for (const char *name : {"postProcessFirstFrame", "postProcessPreviousFrame", "postProcessPlayFrame", "postProcessNextFrame", "postProcessLastFrame"})
        findChild<QToolButton *>(name)->setEnabled(false);
    timeLabel_->setText(uiText(u"无时间数据"));
    updateSelectionActions();
    connect(tabs, &QTabWidget::currentChanged, this, [this, tabs](int index) {
        if (tabs->widget(index) == navigationPanels_[1]) updateDataSelection(datasetTree_->currentItem(), 0);
        if (tabs->widget(index) == navigationPanels_[2]) updateSelectionDetails();
    });
}

void PostProcessWorkbenchWindow::createActions() {
    auto *importAction = ribbonAction(this, commandIcon("import"), uiText(u"导入数据"), uiText(u"选择一个或多个可视化数据文件"), uiText(u"postProcessImportAction"));
    auto *contourAction = ribbonAction(this, commandIcon("contour"), uiText(u"新建云图"), uiText(u"将选中字段以云图方式添加到图层"), uiText(u"postProcessContourAction"));
    auto *colorAction = ribbonAction(this, commandIcon("palette"), uiText(u"色标范围"), uiText(u"配置字段的色标和值域"), uiText(u"postProcessColorAction"));
    auto *sliceAction = ribbonAction(this, commandIcon("slice"), uiText(u"添加切面"), uiText(u"在体网格中创建可编辑的切面"), uiText(u"postProcessSliceAction"));
    auto *isoLineAction = ribbonAction(this, commandIcon("iso"), uiText(u"等值线"), uiText(u"从物面或切片的标量场提取等值线"), uiText(u"postProcessIsoLineAction"));
    auto *isoAction = ribbonAction(this, commandIcon("iso"), uiText(u"等值面"), uiText(u"根据选中字段提取三维等值面"), uiText(u"postProcessIsoAction"));
    auto *streamAction = ribbonAction(this, commandIcon("stream"), uiText(u"流线"), uiText(u"从种子点生成流线或表面流线"), uiText(u"postProcessStreamAction"));
    auto *traceAction = ribbonAction(this, commandIcon("trace"), uiText(u"迹线"), uiText(u"针对时间序结果生成粒子轨迹"), uiText(u"postProcessTraceAction"));
    auto *focusAction = ribbonAction(this, commandIcon("fit"), uiText(u"聚焦选中对象"), uiText(u"将选中对象调整到视图中心"), QStringLiteral("postProcessFocusAction"));
    connect(focusAction, &QAction::triggered, this, [this] { emit viewRequested(QStringLiteral("focusSelected")); appendMessage(uiText(u"已请求聚焦选中对象；视图服务尚未接入。")); });
    auto *centerAction = ribbonAction(this, commandIcon("rotate"), uiText(u"旋转中心…"), uiText(u"设置视图旋转中心"), QStringLiteral("postProcessRotationCenterAction"));
    connect(centerAction, &QAction::triggered, this, [this] {
        auto *dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setWindowTitle(uiText(u"旋转中心"));
        auto *layout = new QVBoxLayout(dialog); auto *form = new QFormLayout;
        auto *mode = new QComboBox(dialog); mode->addItems({uiText(u"选中对象中心"), uiText(u"手动坐标")}); form->addRow(uiText(u"中心位置"), mode);
        for (const QString &axis : {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")}) { auto *value = new QDoubleSpinBox(dialog); value->setRange(-1.e9, 1.e9); value->setDecimals(4); form->addRow(axis, value); value->setEnabled(false); connect(mode, QOverload<int>::of(&QComboBox::currentIndexChanged), value, [value](int index) { value->setEnabled(index == 1); }); }
        layout->addLayout(form); auto *hint = new QLabel(uiText(u"视图控制服务尚未接入。"), dialog); layout->addWidget(hint);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog); layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog] { emit postProcessCommandRequested(QStringLiteral("setRotationCenter")); dialog->accept(); });
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject); dialog->show();
    });
    auto *fitAction = ribbonAction(this, commandIcon("fit"), uiText(u"适配视图"), uiText(u"让可见图层适配当前视口"), uiText(u"postProcessFitAction"));
    auto *probeAction = ribbonAction(this, commandIcon("probe"), uiText(u"探针"), uiText(u"查询单元、节点的坐标和字段值"), uiText(u"postProcessProbeAction"));
    styleAction_ = ribbonAction(this, commandIcon("style"), uiText(u"对象样式…"), uiText(u"编辑选中对象的颜色、网格和可见性"), QStringLiteral("postProcessObjectStyleAction"));
    parameterAction_ = ribbonAction(this, commandIcon("parameters"), uiText(u"编辑参数…"), uiText(u"编辑选中对象的专属参数草稿"), QStringLiteral("postProcessParametersAction"));
    auto *deepSeekAction = ribbonAction(this, commandIcon("harness"), uiText(u"DeepSeek"), uiText(u"打开 DeepSeek Harness，基于当前选择组织后处理任务"), uiText(u"postProcessHarnessAction"));
    auto *animationExportAction = ribbonAction(this, commandIcon("trace"), uiText(u"时序动画"), uiText(u"导出当前时间序列动画"), uiText(u"postProcessAnimationExportAction"));
    auto *imageExportAction = ribbonAction(this, commandIcon("export"), uiText(u"保存图片"), uiText(u"设置并保存当前视图图片"), uiText(u"postProcessImageExportAction"));
    auto *selectAction = ribbonAction(this, commandIcon("select"), uiText(u"选择"), uiText(u"选择 Zone、图层或派生对象"), uiText(u"postProcessSelectAction"));
    auto *boxSelectAction = ribbonAction(this, commandIcon("boxSelect"), uiText(u"框选"), uiText(u"框选多个可见对象"), uiText(u"postProcessBoxSelectAction"));
    auto *panAction = ribbonAction(this, commandIcon("pan"), uiText(u"平移"), uiText(u"平移当前视图"), uiText(u"postProcessPanAction"));
    auto *zoomAction = ribbonAction(this, commandIcon("zoom"), uiText(u"缩放"), uiText(u"缩放当前视图"), uiText(u"postProcessZoomAction"));
    auto *rotateAction = ribbonAction(this, commandIcon("rotate"), uiText(u"旋转"), uiText(u"旋转三维视图"), uiText(u"postProcessRotateAction"));
    auto *frontAction = ribbonAction(this, commandIcon("front"), uiText(u"前视"), uiText(u"切换至前视图"), uiText(u"postProcessFrontViewAction"));
    auto *rightAction = ribbonAction(this, commandIcon("right"), uiText(u"右视"), uiText(u"切换至右视图"), uiText(u"postProcessRightViewAction"));
    auto *topAction = ribbonAction(this, commandIcon("top"), uiText(u"俯视"), uiText(u"切换至俯视图"), uiText(u"postProcessTopViewAction"));
    auto *interactionModes = new QActionGroup(this);
    interactionModes->setExclusive(true);
    for (QAction *action : {selectAction, boxSelectAction, panAction, zoomAction, rotateAction, probeAction}) {
        action->setCheckable(true);
        interactionModes->addAction(action);
    }
    selectAction->setChecked(true);
    // Use the same SARibbon hierarchy as MainWindow.  Categories group work
    // by intent; the data/scene workspace below remains the post-process UI.
    auto *dataCategory = ribbonBar()->addCategoryPage(uiText(u"数据与显示"));
    dataCategory->setObjectName(uiText(u"postProcessDataCategory"));
    auto *dataSourcePanel = dataCategory->addPanel(uiText(u"结果数据"));
    dataSourcePanel->addLargeAction(importAction);

    auto *interactionPanel = dataCategory->addPanel(uiText(u"交互"));
    interactionPanel->addLargeAction(selectAction); interactionPanel->addSmallAction(boxSelectAction);
    interactionPanel->addSmallAction(panAction); interactionPanel->addSmallAction(zoomAction); interactionPanel->addSmallAction(rotateAction);
    auto *layerPanel = dataCategory->addPanel(uiText(u"图层"));
    layerPanel->addLargeAction(contourAction); layerPanel->addSmallAction(colorAction); layerPanel->addSmallAction(styleAction_);
    auto *cameraPanel = dataCategory->addPanel(uiText(u"视图"));
    cameraPanel->addLargeAction(fitAction); cameraPanel->addSmallAction(focusAction); cameraPanel->addSmallAction(centerAction); cameraPanel->addSmallAction(frontAction); cameraPanel->addSmallAction(rightAction); cameraPanel->addSmallAction(topAction);

    auto *featureCategory = ribbonBar()->addCategoryPage(uiText(u"特征"));
    featureCategory->setObjectName(uiText(u"postProcessFeatureCategory"));
    auto *spatialPanel = featureCategory->addPanel(uiText(u"空间特征"));
    spatialPanel->addLargeAction(sliceAction); spatialPanel->addSmallAction(isoAction);
    spatialPanel->addSmallAction(streamAction); spatialPanel->addSmallAction(traceAction);
    spatialPanel->addSmallAction(isoLineAction);


    auto *toolsCategory = ribbonBar()->addCategoryPage(uiText(u"工具与输出"));
    toolsCategory->setObjectName(uiText(u"postProcessToolsCategory"));
    auto *outputPanel = toolsCategory->addPanel(uiText(u"导出结果"));
    outputPanel->addLargeAction(imageExportAction); outputPanel->addLargeAction(animationExportAction);

    auto *queryPanel = toolsCategory->addPanel(uiText(u"查询"));
    queryPanel->addLargeAction(probeAction);
    auto *rulerAction = ribbonAction(this, commandIcon("fit"), uiText(u"坐标尺"), uiText(u"显示或隐藏坐标尺"), QStringLiteral("postProcessRulerAction"));
    rulerAction->setCheckable(true); cameraPanel->addSmallAction(rulerAction);
    connect(rulerAction, &QAction::toggled, this, [this](bool visible) {
        if (auto *label = findChild<QLabel *>(QStringLiteral("postProcessRulerPlaceholder"))) label->setVisible(visible);
        emit postProcessCommandRequested(visible ? QStringLiteral("ruler:on") : QStringLiteral("ruler:off"));
        appendMessage(uiText(u"坐标尺界面设置已更改；刻度显示待视图服务接入。"));
    });
    auto *backgroundAction = ribbonAction(this, commandIcon("palette"), uiText(u"视口背景…"), uiText(u"调整当前视口背景，默认 ParaView 蓝灰渐变"), QStringLiteral("postProcessBackgroundAction"));
    cameraPanel->addSmallAction(backgroundAction);
    connect(backgroundAction, &QAction::triggered, this, &PostProcessWorkbenchWindow::openBackgroundSettings);
    auto *examples = toolsCategory->addPanel(uiText(u"界面示例"));
    for (bool structured : {true, false}) {
        auto *example = ribbonAction(this, commandIcon("mesh"), structured ? uiText(u"结构网格示例") : uiText(u"非结构网格示例"),
            uiText(u"仅加载界面元信息，用于检查菜单联动；不读取文件或计算结果"),
            structured ? QStringLiteral("postProcessStructuredExampleAction") : QStringLiteral("postProcessUnstructuredExampleAction"));
        examples->addSmallAction(example);
        connect(example, &QAction::triggered, this, [this, structured] { loadUiExample(structured); });
    }


    auto *multiExample = ribbonAction(this, commandIcon("mesh"), uiText(u"多文件示例"),
        uiText(u"展示多个文件、区域和字段的组织方式；不读取真实数据"), QStringLiteral("postProcessMultiFileExampleAction"));
    examples->addSmallAction(multiExample);
    connect(multiExample, &QAction::triggered, this, &PostProcessWorkbenchWindow::loadMultiFileUiExample);

    auto *resultsAction = ribbonAction(this, commandIcon("probe"), uiText(u"探针结果"), uiText(u"查看已有取点结果，不改变鼠标模式"), QStringLiteral("postProcessProbeResultsAction"));
    auto *panelGroup = toolsCategory->addPanel(uiText(u"面板"));
    for (int i=1; i<3; ++i) {
        const QStringList names = {QString(), uiText(u"数据面板"), uiText(u"对象面板")};
        const QStringList ids = {QString(), QStringLiteral("postProcessShowDataPanel"), QStringLiteral("postProcessShowObjectPanel")};
        auto *action = ribbonAction(this, commandIcon("dock"), names.at(i), uiText(u"打开") + names.at(i), ids.at(i));
        panelGroup->addSmallAction(action);
        connect(action, &QAction::triggered, this, [this, i] { showNavigationPanel(i); });
    }
    panelGroup->addSmallAction(resultsAction);
    connect(resultsAction, &QAction::triggered, this, &PostProcessWorkbenchWindow::showProbePanel);
    connect(styleAction_, &QAction::triggered, this, &PostProcessWorkbenchWindow::openObjectStyle);
    connect(parameterAction_, &QAction::triggered, this, &PostProcessWorkbenchWindow::openObjectParameters);

    auto *interactionHint = new QLabel(uiText(u"选择模式 · 左键选择 / 中键平移 / 滚轮缩放"), this);
    interactionHint->setObjectName(uiText(u"postProcessInteractionHint"));
    statusBar()->addPermanentWidget(interactionHint, 1);
    const auto command = [this](const QString &key, const QString &message) { emit postProcessCommandRequested(key); appendMessage(message); };
    connect(importAction, &QAction::triggered, this, &PostProcessWorkbenchWindow::openImportDialog);
    const auto setInteractionMode = [this, interactionHint](const QString &key, const QString &mode, const QString &hint) {
        interactionHint->setText(hint);
        if (auto *host = findChild<QWidget *>(QStringLiteral("postProcessViewportHost")))
            host->setCursor(key == "pan" ? Qt::OpenHandCursor : Qt::ArrowCursor);
        emit postProcessCommandRequested(QStringLiteral("interactionMode:") + key);
        appendMessage(uiText(u"已切换至") + mode + uiText(u"模式。"));
    };
    connect(selectAction, &QAction::triggered, this, [setInteractionMode] { setInteractionMode(QStringLiteral("select"), uiText(u"选择"), uiText(u"选择模式 · 左键选择 / 中键平移 / 滚轮缩放")); });
    connect(boxSelectAction, &QAction::triggered, this, [setInteractionMode] { setInteractionMode(QStringLiteral("boxSelect"), uiText(u"框选"), uiText(u"框选模式 · 在视口拖曳以选择对象")); });
    connect(panAction, &QAction::triggered, this, [setInteractionMode] { setInteractionMode(QStringLiteral("pan"), uiText(u"平移"), uiText(u"平移模式 · 在视口拖曳以移动视图")); });
    connect(zoomAction, &QAction::triggered, this, [setInteractionMode] { setInteractionMode(QStringLiteral("zoom"), uiText(u"缩放"), uiText(u"缩放模式 · 拖曳或滚轮缩放视图")); });
    connect(rotateAction, &QAction::triggered, this, [setInteractionMode] { setInteractionMode(QStringLiteral("rotate"), uiText(u"旋转"), uiText(u"旋转模式 · 拖曳以旋转三维视图")); });
    connect(frontAction, &QAction::triggered, this, [this] { emit viewRequested(uiText(u"front")); appendMessage(uiText(u"已切换至前视图。")); });
    connect(rightAction, &QAction::triggered, this, [this] { emit viewRequested(uiText(u"right")); appendMessage(uiText(u"已切换至右视图。")); });
    connect(topAction, &QAction::triggered, this, [this] { emit viewRequested(uiText(u"top")); appendMessage(uiText(u"已切换至俯视图。")); });
    connect(animationExportAction, &QAction::triggered, this, [this] { openExportDialog(QStringLiteral("animation")); });
    connect(imageExportAction, &QAction::triggered, this, [this] { openExportDialog(QStringLiteral("image")); });
    connect(colorAction, &QAction::triggered, this, [this] { openObjectParameters(true); });
    connect(contourAction, &QAction::triggered, this, [this] {
        addDerivedLayer("contour", selectedContext_.value("field").toString() + uiText(u"云图"), uiText(u"已创建云图草稿；可调整字段与色标，尚未执行渲染。"));
    });
    connect(sliceAction, &QAction::triggered, this, [this] {
        addDerivedLayer("slice", uiText(u"切面"), uiText(u"已创建切面草稿；编辑位置与法向后交给特征服务。"));
    });
    connect(isoAction, &QAction::triggered, this, [this] {
        addDerivedLayer("iso", uiText(u"等值面"), uiText(u"已创建等值面草稿；请编辑等值。"));
    });
    connect(isoLineAction, &QAction::triggered, this, [this] {
        addDerivedLayer("isoline", uiText(u"等值线"), uiText(u"已创建物面/切片等值线草稿；请编辑等值。"));
    });
    connect(streamAction, &QAction::triggered, this, [this] { openNewFeatureDialog(QStringLiteral("stream")); });
    connect(traceAction, &QAction::triggered, this, [this] {
        addDerivedLayer("trace", uiText(u"迹线"), uiText(u"已创建时序轨迹草稿；追踪服务尚未接入。"));
    });
    connect(fitAction, &QAction::triggered, this, [this] { emit viewRequested(uiText(u"reset")); appendMessage(uiText(u"视图已适配所有可见图层。")); }); connect(probeAction, &QAction::triggered, this, [command] { command(uiText(u"probe"), uiText(u"已进入探针查询模式：请在视口点选单元或节点。")); });
    connect(deepSeekAction, &QAction::triggered, this, &PostProcessWorkbenchWindow::openDeepSeekHarness);
    connect(probeAction, &QAction::triggered, this, [this, interactionHint] {
        interactionHint->setText(uiText(u"探针模式 · 点击数据位置取点；当前拾取服务待接入"));
        if (auto *host = findChild<QWidget *>(QStringLiteral("postProcessViewportHost"))) host->setCursor(Qt::CrossCursor);
    });
}

void PostProcessWorkbenchWindow::openDeepSeekHarness() {
    if (!deepSeekHarnessDock_) {
        deepSeekHarnessDock_ = new QDockWidget(uiText(u"DeepSeek Harness"), this);
        installDockTitle(deepSeekHarnessDock_);
        deepSeekHarnessDock_->setObjectName(uiText(u"deepSeekHarnessDock"));
        auto *panel = new QWidget(deepSeekHarnessDock_);
        auto *layout = new QVBoxLayout(panel);
        auto *hint = new QLabel(uiText(u"根据当前选择的 Zone、字段、图层或时间步组织后处理任务。"), panel);
        hint->setWordWrap(true);
        layout->addWidget(hint);
        auto *scope = new QComboBox(panel);
        scope->addItems({uiText(u"当前选择"), uiText(u"当前视口"), uiText(u"可视化数据")});
        layout->addWidget(scope);
        auto *prompt = new QTextEdit(panel);
        prompt->setObjectName(uiText(u"deepSeekHarnessPrompt"));
        prompt->setPlaceholderText(uiText(u"例如：对翼面 Zone 的压力场生成三个切面，并导出对比图。"));
        prompt->setMinimumHeight(180);
        layout->addWidget(prompt);
        auto *run = new QPushButton(uiText(u"提交后处理任务"), panel);
        run->setObjectName(uiText(u"primaryAction"));
        layout->addWidget(run);
        auto *state = new QLabel(uiText(u"Harness 服务尚未接入；此入口会发出任务请求并保留当前上下文。"), panel);
        state->setObjectName(uiText(u"mutedText")); state->setWordWrap(true);
        layout->addWidget(state); layout->addStretch();
        panel->setStyleSheet(centralWidget()->styleSheet());
        deepSeekHarnessDock_->setWidget(scrollPanel(panel));
        addDockWidget(Qt::RightDockWidgetArea, deepSeekHarnessDock_);
        connect(run, &QPushButton::clicked, this, [this, prompt, scope] {
            emit postProcessCommandRequested(uiText(u"openDeepSeekHarness"));
            appendMessage(prompt->toPlainText().trimmed().isEmpty()
                ? uiText(u"DeepSeek Harness 已打开；请输入后处理任务。")
                : uiText(u"已向 DeepSeek Harness 提交当前上下文与任务描述。"));
        });
    }
    deepSeekHarnessDock_->show();
    deepSeekHarnessDock_->raise();
    emit postProcessCommandRequested(uiText(u"openDeepSeekHarness"));
    appendMessage(uiText(u"DeepSeek Harness 入口已打开。"));
}

QWidget *PostProcessWorkbenchWindow::createDataPanel() {
    auto *panel = new QWidget(this); auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(8, 8, 8, 8); auto *row = new QHBoxLayout; row->addWidget(sectionLabel(uiText(u"可视化数据"), panel)); row->addStretch(); auto *import = new QPushButton(uiText(u"导入"), panel); import->setIcon(commandIcon("import")); import->setObjectName(uiText(u"primaryAction")); row->addWidget(import); layout->addLayout(row);
    auto *card = new QFrame(panel); card->setObjectName(uiText(u"dataCard")); auto *cardLayout = new QVBoxLayout(card); sessionContext_ = new QLabel(uiText(u"尚未导入结果"), card); sessionContext_->setWordWrap(true); sessionContext_->setStyleSheet(uiText(u"font-weight:600;color:#25364c;")); auto *description = new QLabel(uiText(u"选择 Zone、边界或字段后，工具栏仅点亮当前数据支持的功能。"), card); description->setObjectName(uiText(u"mutedText")); description->setWordWrap(true); cardLayout->addWidget(sessionContext_); cardLayout->addWidget(description); layout->addWidget(card);
    selectionSummary_ = new QLabel(uiText(u"当前未选择数据"), panel); selectionSummary_->setWordWrap(true); selectionSummary_->setObjectName(uiText(u"mutedText")); layout->addWidget(selectionSummary_); datasetTree_ = new QTreeWidget(panel); datasetTree_->setObjectName(uiText(u"postProcessDatasetTree")); datasetTree_->setHeaderHidden(true); datasetTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(datasetTree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int column) {
        updateDataSelection(item, column); openSelectedSettings();
    });
    datasetTree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(datasetTree_, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        auto *item = datasetTree_->itemAt(point); if (!item) return;
        datasetTree_->setCurrentItem(item); updateDataSelection(item,0);
        QMenu menu(this); menu.addAction(styleAction_); menu.addAction(parameterAction_);
        const QString itemKind = item->data(0, Qt::UserRole).toString();
        if (!item->parent() && (itemKind == QStringLiteral("exampleFile") || itemKind == QStringLiteral("pendingGroup"))) {
            auto *remove = menu.addAction(uiText(u"移除此项"));
            connect(remove, &QAction::triggered, this, [this, item] {
                const QString source = item->data(0, Qt::UserRole+3).toMap().value("sourceId").toString();
                for (int i = sceneList_->count()-1; i >= 0; --i) {
                    auto *derived = sceneList_->item(i);
                    if (!source.isEmpty() && derived->data(Qt::UserRole+3).toMap().value("sourceId").toString() == source) {
                        const QString id = derived->data(Qt::UserRole+1).toString(); styleDrafts_.remove(id); parameterDrafts_.remove(id); delete sceneList_->takeItem(i);
                    }
                }
                delete datasetTree_->takeTopLevelItem(datasetTree_->indexOfTopLevelItem(item));
                updateDataSelection(datasetTree_->currentItem(), 0);
                appendMessage(uiText(u"已移除所选界面条目。"));
            });
        }
        menu.exec(datasetTree_->viewport()->mapToGlobal(point));
    });
    connect(datasetTree_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
        if (column != 0 || item->data(0, Qt::UserRole).toString() != QStringLiteral("exampleFile")) return;
        const bool visible = item->checkState(0) == Qt::Checked;
        item->setToolTip(0, visible ? uiText(u"界面示例 · 已勾选显示") : uiText(u"界面示例 · 已取消显示"));
        appendMessage(visible ? uiText(u"示例文件已勾选；真实显示尚未接入。") : uiText(u"示例文件已取消勾选；真实显示尚未接入。"));
    });
    datasetTree_->expandAll(); layout->addWidget(datasetTree_, 1); auto *hint = new QLabel(uiText(u"数据集中的 Zone、字段与时间步由读取服务提供；派生对象在“对象”页管理。"), panel); hint->setObjectName(uiText(u"mutedText")); hint->setWordWrap(true); layout->addWidget(hint); connect(datasetTree_, &QTreeWidget::itemClicked, this, &PostProcessWorkbenchWindow::updateDataSelection); connect(datasetTree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current, QTreeWidgetItem *) { updateDataSelection(current, 0); }); connect(import, &QPushButton::clicked, this, &PostProcessWorkbenchWindow::openImportDialog); return panel;
}

void PostProcessWorkbenchWindow::openImportDialog() {
    if (importOrganizationDialog_) { importOrganizationDialog_->raise(); importOrganizationDialog_->activateWindow(); return; }
    auto *dialog = new QDialog(this); importOrganizationDialog_ = dialog;
    dialog->setObjectName(QStringLiteral("postProcessImportDataDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setWindowTitle(uiText(u"导入可视化数据")); dialog->resize(680, 410);
    auto *layout = new QVBoxLayout(dialog);
    auto *hint = new QLabel(uiText(u"选择一个或多个数据文件。Plot3D：仅查看网格可只选网格文件；查看流场需同时选择配套的网格（常见 .xyz）和结果（常见 .q）文件。多组文件的配对确认尚未接入。"), dialog); hint->setWordWrap(true); layout->addWidget(hint);
    auto *files = new QListWidget(dialog); files->setObjectName(QStringLiteral("postProcessImportFileList"));
    files->setSelectionMode(QAbstractItemView::ExtendedSelection); layout->addWidget(files, 1);
    auto *row = new QHBoxLayout; auto *add = new QPushButton(uiText(u"添加文件…"), dialog); auto *remove = new QPushButton(uiText(u"移除所选"), dialog);
    add->setObjectName(QStringLiteral("postProcessImportAdd")); remove->setObjectName(QStringLiteral("postProcessImportRemove"));
    row->addWidget(add); row->addWidget(remove); row->addStretch(); layout->addLayout(row);
    connect(add, &QPushButton::clicked, dialog, [dialog, files] {
        const QStringList selected = QFileDialog::getOpenFileNames(dialog, uiText(u"选择可视化数据"), QString(),
            uiText(u"可视化数据 (*.vtk *.vti *.vtu *.vts *.vtr *.vtp *.vtm *.cgns *.xyz *.q *.p3d *.tec *.dat *.plt);;VTK 文件 (*.vtk *.vti *.vtu *.vts *.vtr *.vtp *.vtm);;CGNS 文件 (*.cgns);;Plot3D 文件 (*.xyz *.q *.p3d);;Tecplot 文件 (*.tec *.dat *.plt);;所有文件 (*)"));
        for (const QString &path : selected) {
            bool duplicate = false;
            for (int i = 0; i < files->count(); ++i) if (files->item(i)->data(Qt::UserRole).toString() == path) { duplicate = true; break; }
            if (!duplicate) { auto *item = new QListWidgetItem(QFileInfo(path).fileName(), files); item->setData(Qt::UserRole, path); item->setToolTip(path); }
        }
    });
    connect(remove, &QPushButton::clicked, dialog, [files] { qDeleteAll(files->selectedItems()); });
    auto *mode = new QComboBox(dialog); mode->setObjectName(QStringLiteral("postProcessImportMode"));
    mode->addItem(uiText(u"追加显示"), QStringLiteral("append")); mode->addItem(uiText(u"替换现有数据"), QStringLiteral("replace"));
    auto *modeRow = new QWidget(dialog); auto *modeLayout = new QFormLayout(modeRow); modeLayout->setContentsMargins(0,0,0,0); modeLayout->addRow(uiText(u"导入方式"), mode);
    bool hasData = false;
    for (int i = 0; i < datasetTree_->topLevelItemCount(); ++i)
        if (!datasetTree_->topLevelItem(i)->data(0, Qt::UserRole).toString().startsWith(QStringLiteral("pending"))) { hasData = true; break; }
    modeRow->setVisible(hasData); layout->addWidget(modeRow);
    auto *status = new QLabel(uiText(u"读取服务尚未接入；提交后文件会显示为等待读取。"), dialog); status->setWordWrap(true); status->setObjectName(QStringLiteral("postProcessImportStatus")); layout->addWidget(status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog); buttons->button(QDialogButtonBox::Ok)->setText(uiText(u"导入")); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, files, mode, status, hasData] {
        if (files->count() == 0) { status->setText(uiText(u"请选择至少一个文件。")); return; }
        QStringList paths;
        for (int i = 0; i < files->count(); ++i) paths << files->item(i)->data(Qt::UserRole).toString();
        const QString importMode = hasData ? mode->currentData().toString() : QStringLiteral("initial");
        auto *request = new QTreeWidgetItem(datasetTree_, QStringList(importMode == "replace" ? uiText(u"待替换导入（等待读取）") : uiText(u"待导入文件（等待读取）")));
        request->setData(0, Qt::UserRole, QStringLiteral("pendingGroup")); request->setData(0, Qt::UserRole + 4, importMode);
        for (const QString &path : paths) {
            auto *item = new QTreeWidgetItem(request, QStringList(QFileInfo(path).fileName() + uiText(u" · 等待读取")));
            item->setData(0, Qt::UserRole, QStringLiteral("pending")); item->setData(0, Qt::UserRole + 4, path); item->setToolTip(0, path);
        }
        request->setExpanded(true); showNavigationPanel(1); datasetTree_->setCurrentItem(request);
        emit importFilesRequested(paths); emit postProcessCommandRequested(QStringLiteral("importResults:") + importMode);
        appendMessage(importMode == "replace" ? uiText(u"替换请求已提交；读取成功前保留现有数据。") : uiText(u"导入请求已提交；读取服务尚未接入。"));
        dialog->accept();
    });
    dialog->show();
}

void PostProcessWorkbenchWindow::openExportDialog(const QString &kind) {
    if (exportSetupDialog_) { exportSetupDialog_->raise(); exportSetupDialog_->activateWindow(); return; }
    auto *dialog = new QDialog(this); exportSetupDialog_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName(QStringLiteral("postProcessExportSetupDialog"));
    dialog->setWindowTitle(kind == "image" ? uiText(u"保存视图图片") : uiText(u"导出时序动画"));
    dialog->resize(520,360);
    auto *layout = new QVBoxLayout(dialog); auto *form = new QFormLayout;
    auto *object = new QLineEdit(selectedName_,dialog); object->setReadOnly(true); object->setObjectName(QStringLiteral("postProcessExportObject"));
    form->addRow(uiText(u"当前对象"),object);
    auto *format = new QComboBox(dialog); format->setObjectName(QStringLiteral("postProcessExportFormat"));
    if (kind == "image") format->addItems({"PNG", "JPEG"});
    else format->addItems({uiText(u"图像序列（待接入）"),uiText(u"动画（编码待定）")});
    form->addRow(uiText(u"目标格式"),format);
    if (kind == "animation") {
        auto *range = new QLineEdit(dialog); range->setPlaceholderText(uiText(u"起止时间待真实帧提供")); form->addRow(uiText(u"时间范围"),range);
        auto *fps = new QSpinBox(dialog); fps->setRange(1,120); fps->setValue(24); fps->setObjectName(QStringLiteral("postProcessExportFps")); form->addRow(uiText(u"帧率"),fps);
    } else {
        auto *background = new QComboBox(dialog); background->addItems({uiText(u"当前背景"),uiText(u"透明背景（待接入）")}); form->addRow(uiText(u"背景"),background);
    }
    auto *width = new QSpinBox(dialog); width->setRange(1,8192); width->setValue(1920); width->setObjectName(QStringLiteral("postProcessExportWidth")); form->addRow(uiText(u"宽度"),width);
    auto *height = new QSpinBox(dialog); height->setRange(1,8192); height->setValue(1080); height->setObjectName(QStringLiteral("postProcessExportHeight")); form->addRow(uiText(u"高度"),height);
    layout->addLayout(form);
    auto *hint = new QLabel(uiText(u"导出服务尚未接入；此窗口只演示操作流程，不会写入文件。"),dialog); hint->setWordWrap(true); layout->addWidget(hint); layout->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(uiText(u"选择保存位置…")); buttons->button(QDialogButtonBox::Cancel)->setText(uiText(u"取消")); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,dialog,[this,dialog,kind,format] {
        if (exportDialog_) { exportDialog_->raise(); exportDialog_->activateWindow(); return; }
        auto *picker = new QFileDialog(dialog, uiText(u"选择导出目标（服务待接入）"));
        exportDialog_ = picker; picker->setObjectName(QStringLiteral("postProcessExportDialog")); picker->setAttribute(Qt::WA_DeleteOnClose);
        picker->setAcceptMode(QFileDialog::AcceptSave); picker->setFileMode(QFileDialog::AnyFile);
        const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (!documents.isEmpty()) {
            const QString outputDir = QDir(documents).filePath(uiText(u"AeroSimWorkbench/后处理"));
            if (QDir().mkpath(outputDir)) picker->setDirectory(outputDir);
            else appendMessage(uiText(u"无法创建默认后处理目录，请选择其他保存位置。"));
        }

        const QString selectedFormat = format->currentText();
        if (selectedFormat == "PNG") picker->setNameFilter(uiText(u"PNG (*.png)"));
        else if (selectedFormat == "JPEG") picker->setNameFilter(uiText(u"JPEG (*.jpg *.jpeg)"));
        else picker->setNameFilter(uiText(u"所有文件 (*)"));
        connect(picker,&QFileDialog::fileSelected,dialog,[this,dialog,kind](const QString &path) {
            if (path.isEmpty()) return;
            emit exportFileRequested(kind,path); emit postProcessCommandRequested(QStringLiteral("export:")+kind);
            appendMessage(uiText(u"已选择导出目标；导出服务尚未接入，没有生成文件。")); dialog->accept();
        });
        picker->open();
    });
    dialog->show();
}

void PostProcessWorkbenchWindow::loadUiExample(bool structured) {
    // Do not replace result sets when opening a presentation fixture.
    for (int i = 0; i < datasetTree_->topLevelItemCount(); ++i)
        if (datasetTree_->topLevelItem(i)->data(0, Qt::UserRole).toString().startsWith(QStringLiteral("pending"))) { appendMessage(uiText(u"请在独立窗口中查看界面示例，以保留当前结果集。")); return; }
    if (emptyViewport_) { emptyViewport_->setProperty("showExample", true); emptyViewport_->update(); }
    if (styleDialog_) styleDialog_->close();
    if (parameterDialog_) parameterDialog_->close();
    datasetTree_->clear(); sceneList_->clear();
    styleDrafts_.clear(); parameterDrafts_.clear();
    auto *zones = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"网格与 Zone"))); zones->setData(0, Qt::UserRole, "group"); auto *volume = new QTreeWidgetItem(zones, QStringList(uiText(u"流场体网格"))); volume->setData(0, Qt::UserRole, "volume"); for (const QString &text : {uiText(u"翼面 Zone"), uiText(u"机身壁面")}) { auto *item = new QTreeWidgetItem(zones, QStringList(uiText(u"") + text)); item->setData(0, Qt::UserRole, "surface"); }
    auto *fields = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"物理字段"))); fields->setData(0, Qt::UserRole, "group"); for (const QString &text : {uiText(u"压力 [Pa]"), uiText(u"速度大小 [m/s]"), uiText(u"温度 [K]"), uiText(u"马赫数 [-]")}) { auto *item = new QTreeWidgetItem(fields, QStringList(uiText(u"") + text)); item->setData(0, Qt::UserRole, "field"); }
    auto *time = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"时间序列"))); time->setData(0, Qt::UserRole, "time"); for (const QString &text : {uiText(u"t = 0.0 s"), uiText(u"t = 0.5 s"), uiText(u"t = 1.0 s")}) { auto *item = new QTreeWidgetItem(time, QStringList(text)); item->setData(0, Qt::UserRole, "time"); }
    for (int i = 0; i < datasetTree_->topLevelItemCount(); ++i) {
        auto *group = datasetTree_->topLevelItem(i);
        const QString key = i == 0 ? "mesh" : i == 1 ? "field" : i == 2 ? "trace" : "analysis";
        group->setIcon(0, commandIcon(key));
        group->setData(0, Qt::UserRole + 1, QStringLiteral("group-%1").arg(i));
        for (int j = 0; j < group->childCount(); ++j) {
            group->child(j)->setIcon(0, commandIcon(key));
            group->child(j)->setData(0, Qt::UserRole + 1, QStringLiteral("data-%1-%2").arg(i).arg(j));
        }
    }

    auto *vector = new QTreeWidgetItem(fields, QStringList(uiText(u"速度矢量 [m/s]")));
    vector->setData(0, Qt::UserRole, "field");
    vector->setData(0, Qt::UserRole+1, "data-velocity-vector");
    vector->setIcon(0, commandIcon("stream"));
    QVariantMap context;
    context["sourceName"] = uiText(u"网格界面示例（未读取文件）");
    context["sourceId"] = structured ? "example-structured" : "example-unstructured";
    context["topology"] = structured ? uiText(u"结构网格") : uiText(u"非结构网格");
    context["dimension"] = 3;
    context["scalars"] = QStringList{uiText(u"压力 [Pa]"),uiText(u"速度大小 [m/s]"),uiText(u"温度 [K]"),uiText(u"马赫数 [-]")};
    context["vectors"] = QStringList{uiText(u"速度矢量 [m/s]")};
    context["field"] = uiText(u"压力 [Pa]"); context["components"] = 1;
    context["frames"] = 3; context["draft"] = true;
    for (int i=0; i<zones->childCount(); ++i) {
        auto zoneContext = context; zoneContext["dimension"] = i==0 ? 3 : 2;
        zones->child(i)->setData(0,Qt::UserRole+3,zoneContext);
    }
    for (int i=0; i<fields->childCount(); ++i) {
        auto fieldContext = context;
        fieldContext["field"] = fields->child(i)->text(0);
        fieldContext["components"] = i==4 ? 3 : 1;
        fields->child(i)->setData(0,Qt::UserRole+3,fieldContext);
    }
    // The field list is a view of each zone's own metadata, not a dataset-wide selection.
    datasetTree_->takeTopLevelItem(datasetTree_->indexOfTopLevelItem(fields));
    fields->setText(0, uiText(u"可用字段"));
    volume->addChild(fields);
    for (int i=1; i<zones->childCount(); ++i) {
        auto *zone = zones->child(i);
        auto *zoneFields = fields->clone();
        zone->addChild(zoneFields);
        for (int j=0; j<zoneFields->childCount(); ++j) {
            auto *field = zoneFields->child(j);
            auto fieldContext = field->data(0,Qt::UserRole+3).toMap();
            fieldContext["dimension"] = 2;
            field->setData(0,Qt::UserRole+3,fieldContext);
            field->setData(0,Qt::UserRole+1,QStringLiteral("zone-%1-field-%2").arg(i).arg(j));
        }
    }
    zones->setText(0, uiText(u"网格数据集 · 示例"));
    sceneList_->addItems({uiText(u"切面 1（X = 0.0）  可见"), uiText(u"等值面 1（压力） 可见"), uiText(u"流线 1           可见")});
    for (int i = 0; i < sceneList_->count(); ++i) {
        auto *item = sceneList_->item(i);
        const QString kind = i == 0 ? "slice" : i == 1 ? "iso" : "stream";
        item->setIcon(commandIcon(kind)); item->setData(Qt::UserRole, kind);
        item->setData(Qt::UserRole+1, QStringLiteral("object-%1").arg(nextObjectId_++));
        item->setData(Qt::UserRole+2, item->text().left(item->text().lastIndexOf(uiText(u"可见"))).trimmed());
    }

    for (int i=0; i<sceneList_->count(); ++i) {
        auto objectContext = context;
        objectContext["sourceDimension"] = context.value("dimension");
        objectContext["dimension"] = i==2 ? 1 : 2;
        if (i==2) { objectContext["vectors"] = QStringList(); objectContext["integrationField"] = uiText(u"速度矢量 [m/s]"); }
        sceneList_->item(i)->setData(Qt::UserRole+3,objectContext);
        sceneList_->item(i)->setText(sceneList_->item(i)->data(Qt::UserRole+2).toString()+uiText(u" · 草稿 · 可见"));
    }
    datasetTree_->expandAll();
    sessionContext_->setText(uiText(u"界面示例 · ")+context.value("topology").toString()+uiText(u"（未读取文件）"));
    {
        const QSignalBlocker blocker(timeSlider_);
        timeSlider_->setValue(0);
    }
    timeSlider_->setEnabled(true);
    if (auto *play = findChild<QToolButton *>(QStringLiteral("postProcessPlayFrame"))) play->setChecked(false);
    for (const char *name : {"postProcessFirstFrame", "postProcessPreviousFrame", "postProcessPlayFrame", "postProcessNextFrame", "postProcessLastFrame"})
        findChild<QToolButton *>(name)->setEnabled(true);
    timeLabel_->setText(uiText(u"示例 Frame 1 / 3 · t = 0.0 s"));
    datasetTree_->setCurrentItem(volume);
    updateDataSelection(volume,0);
    appendMessage(uiText(u"已加载界面示例：仅验证流程与菜单联动，不表示文件读取、特征计算或渲染已完成。"));
}

void PostProcessWorkbenchWindow::loadMultiFileUiExample() {
    for (int i = 0; i < datasetTree_->topLevelItemCount(); ++i) {
        const QString kind = datasetTree_->topLevelItem(i)->data(0, Qt::UserRole).toString();
        if (kind.startsWith(QStringLiteral("pending")) || kind == QStringLiteral("resultSet")) {
            appendMessage(uiText(u"请在独立窗口中查看示例，以保留当前数据。")); return;
        }
    }
    if (styleDialog_) styleDialog_->close(); if (parameterDialog_) parameterDialog_->close();
    datasetTree_->clear(); sceneList_->clear(); styleDrafts_.clear(); parameterDrafts_.clear();
    if (emptyViewport_) { emptyViewport_->setProperty("showExample", true); emptyViewport_->update(); }
    const QSignalBlocker blocker(datasetTree_);
    const QStringList fileNames = {QStringLiteral("flowfield.cgns"), QStringLiteral("surface.vtu")};
    const QStringList topology = {uiText(u"非结构体网格"), uiText(u"非结构表面网格")};
    QTreeWidgetItem *firstZone = nullptr;
    for (int i = 0; i < fileNames.size(); ++i) {
        QVariantMap ctx; ctx["sourceId"] = QStringLiteral("multifile-example-%1").arg(i);
        ctx["sourceName"] = fileNames.at(i) + uiText(u" · 示例");
        ctx["topology"] = topology.at(i); ctx["dimension"] = i == 0 ? 3 : 2;
        ctx["scalars"] = i == 0 ? QStringList{uiText(u"压力 [Pa]"), uiText(u"温度 [K]")} : QStringList{uiText(u"压力 [Pa]"), uiText(u"马赫数 [-]")};
        ctx["vectors"] = i == 0 ? QStringList{uiText(u"速度矢量 [m/s]")} : QStringList();
        ctx["field"] = ctx["scalars"].toStringList().first(); ctx["components"] = 1; ctx["frames"] = 1; ctx["draft"] = true;
        auto *file = new QTreeWidgetItem(datasetTree_, QStringList(fileNames.at(i) + uiText(u" · 示例")));
        file->setData(0, Qt::UserRole, QStringLiteral("exampleFile")); file->setData(0, Qt::UserRole+1, ctx["sourceId"]); file->setData(0, Qt::UserRole+3, ctx);
        file->setIcon(0, commandIcon("mesh")); file->setFlags(file->flags() | Qt::ItemIsUserCheckable); file->setCheckState(0, Qt::Checked);
        auto *zone = new QTreeWidgetItem(file, QStringList(i == 0 ? uiText(u"流场 Zone · 体网格") : uiText(u"翼面 Zone · 表面")));
        zone->setData(0, Qt::UserRole, i == 0 ? "volume" : "surface"); zone->setData(0, Qt::UserRole+1, QStringLiteral("multifile-zone-%1").arg(i)); zone->setData(0, Qt::UserRole+3, ctx); zone->setIcon(0, commandIcon("mesh"));
        if (!firstZone) firstZone = zone;
        auto *fields = new QTreeWidgetItem(zone, QStringList(uiText(u"可用字段"))); fields->setData(0, Qt::UserRole, "group"); fields->setIcon(0, commandIcon("field"));
        QStringList names = ctx["scalars"].toStringList(); names.append(ctx["vectors"].toStringList());
        for (int j = 0; j < names.size(); ++j) {
            auto *field = new QTreeWidgetItem(fields, QStringList(names.at(j)));
            QVariantMap fc = ctx; fc["field"] = names.at(j); fc["components"] = ctx["vectors"].toStringList().contains(names.at(j)) ? 3 : 1;
            field->setData(0, Qt::UserRole, "field"); field->setData(0, Qt::UserRole+1, QStringLiteral("multifile-field-%1-%2").arg(i).arg(j)); field->setData(0, Qt::UserRole+3, fc); field->setIcon(0, commandIcon("field"));
        }
        file->setExpanded(true); zone->setExpanded(true); fields->setExpanded(true);
    }
    timeSlider_->setEnabled(false); if (auto *play = findChild<QToolButton *>(QStringLiteral("postProcessPlayFrame"))) play->setChecked(false);
    for (const char *name : {"postProcessFirstFrame", "postProcessPreviousFrame", "postProcessPlayFrame", "postProcessNextFrame", "postProcessLastFrame"}) findChild<QToolButton *>(name)->setEnabled(false);
    timeLabel_->setText(uiText(u"无时间数据")); sessionContext_->setText(uiText(u"多文件界面示例（未读取文件）"));
    showNavigationPanel(1); datasetTree_->setCurrentItem(firstZone); updateDataSelection(firstZone, 0);
    appendMessage(uiText(u"多文件示例仅用于展示界面组织，未读取或渲染实际文件。"));
}

QWidget *PostProcessWorkbenchWindow::createObjectPanel() {
    auto *panel = new QWidget(this); auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(sectionLabel(uiText(u"对象与图层"), panel));
    auto *hint = new QLabel(uiText(u"对象属于当前 Frame。双击对象编辑参数，或通过“对象样式…”编辑外观；显隐和删除只影响选中对象。"), panel);
    hint->setObjectName(uiText(u"mutedText")); hint->setWordWrap(true); layout->addWidget(hint);
    auto *row = new QHBoxLayout;
    row->addWidget(sectionLabel(uiText(u"派生对象"), panel)); row->addStretch();
    auto *add = new QPushButton(uiText(u"新建"), panel); add->setIcon(commandIcon("add"));
    add->setObjectName(QStringLiteral("postProcessNewObjectButton"));
    add->setMinimumWidth(add->fontMetrics().horizontalAdvance(uiText(u"新建")) + 64);
    row->addWidget(add); layout->addLayout(row);
    sceneList_ = new QListWidget(panel); sceneList_->setObjectName(uiText(u"sceneLayerList"));
    layout->addWidget(sceneList_, 1);
    auto *actions = new QHBoxLayout; auto *visible = new QPushButton(uiText(u"显隐"), panel); auto *remove = new QPushButton(uiText(u"删除"), panel); visible->setIcon(commandIcon("visible")); remove->setIcon(commandIcon("delete")); actions->addWidget(visible); actions->addWidget(remove); layout->addLayout(actions);
    auto *newMenu = new QMenu(add);
    newMenu->setObjectName(QStringLiteral("postProcessNewObjectMenu"));
    newMenu->setMinimumWidth(newMenu->fontMetrics().horizontalAdvance(uiText(u"物面 / 切片等值线")) + 80);
    add->setMenu(newMenu);
    const QStringList newKinds = {"contour", "slice", "isoline", "iso", "stream", "trace"};
    const QStringList newNames = {uiText(u"云图"), uiText(u"添加切面"), uiText(u"等值线"), uiText(u"等值面"), uiText(u"流线"), uiText(u"迹线")};
    const QStringList newIcons = {"contour", "slice", "iso", "iso", "stream", "trace"};
    for (int i = 0; i < newKinds.size(); ++i) {
        auto *action = newMenu->addAction(commandIcon(newIcons.at(i)), newNames.at(i));
        action->setObjectName(QStringLiteral("postProcessNew_%1").arg(newKinds.at(i)));
        const QString kind = newKinds.at(i);
        connect(action, &QAction::triggered, this, [this, kind] { openNewFeatureDialog(kind); });
    }
    connect(sceneList_, &QListWidget::itemSelectionChanged, this, &PostProcessWorkbenchWindow::updateSelectionDetails);
    connect(sceneList_, &QListWidget::itemClicked, this, [this] { updateSelectionDetails(); });
    auto *edit = new QToolButton(panel); edit->setDefaultAction(parameterAction_);
    edit->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); layout->addWidget(edit);
    connect(sceneList_, &QListWidget::itemDoubleClicked, this, [this] { updateSelectionDetails(); openObjectParameters(); });
    sceneList_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(sceneList_, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        auto *item = sceneList_->itemAt(point); if (!item) return;
        sceneList_->setCurrentItem(item); updateSelectionDetails();
        QMenu menu(this); menu.addAction(styleAction_); menu.addAction(parameterAction_);
        menu.exec(sceneList_->viewport()->mapToGlobal(point));
    });
    connect(visible, &QPushButton::clicked, this, [this] {
        auto *item = sceneList_->currentItem(); if (!item) return;
        const bool isVisible = item->text().endsWith(uiText(u"可见"));
        item->setText(item->data(Qt::UserRole+2).toString() + uiText(u" · 草稿") + (isVisible ? uiText(u" · 隐藏") : uiText(u" · 可见")));
        styleDrafts_[item->data(Qt::UserRole+1).toString()]["visible"] = !isVisible;
        emit postProcessCommandRequested(QStringLiteral("setObjectVisibility"));
        appendMessage(isVisible ? uiText(u"已隐藏选中派生对象。") : uiText(u"已显示选中派生对象。"));
    });
    connect(remove, &QPushButton::clicked, this, [this] { if (sceneList_->currentItem()) { const QString id = sceneList_->currentItem()->data(Qt::UserRole+1).toString(); styleDrafts_.remove(id); parameterDrafts_.remove(id); delete sceneList_->takeItem(sceneList_->currentRow()); emit postProcessCommandRequested(QStringLiteral("removeObject")); updateSelectionDetails(); appendMessage(uiText(u"已移除选中派生对象。")); } });
    return panel;
}

QWidget *PostProcessWorkbenchWindow::createViewportPanel() {
    auto *frame = new QFrame(this); frame->setObjectName(uiText(u"viewportFrame"));
    auto *layout = new QVBoxLayout(frame); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    auto *header = new QFrame(frame); header->setObjectName(uiText(u"viewportHeader")); auto *headerLayout = new QHBoxLayout(header); headerLayout->setContentsMargins(10, 4, 10, 4);
    viewportTitle_ = new QLabel(uiText(u"Render View · 三维视图"), header); viewportTitle_->setObjectName(uiText(u"viewportTitle"));
    viewportHint_ = new QLabel(uiText(u"空会话 · 无结果"), header); viewportHint_->setObjectName(uiText(u"mutedText"));

    auto *caption = new QVBoxLayout;
    viewportTitle_->setWordWrap(true); viewportHint_->setWordWrap(true);
    caption->addWidget(viewportTitle_); caption->addWidget(viewportHint_);
    headerLayout->addLayout(caption, 1);
    layout->addWidget(header);
    auto *viewportHost = new ViewportHost(frame); viewportHost->setObjectName(uiText(u"postProcessViewportHost")); viewportLayout_ = new QVBoxLayout(viewportHost); viewportLayout_->setContentsMargins(0, 0, 0, 0);
    emptyViewport_ = new ResultPreviewWidget(viewportHost); emptyViewport_->setObjectName(uiText(u"postProcessResultPreview")); viewportLayout_->addWidget(emptyViewport_);
    auto *sphere = new OrientationControl(viewportHost); sphere->setObjectName(QStringLiteral("postProcessOrientationControl")); viewportHost->orientation = sphere;
    connect(this, &PostProcessWorkbenchWindow::viewRequested, sphere, [sphere](const QString &view) { sphere->setView(view); });
    sphere->onView = [this](const QString &orientation) { emit viewRequested(orientation); appendMessage(uiText(u"视角请求已发送；视图服务尚未接入。")); };
    sphere->onRotate = [this](int x, int y) { emit postProcessCommandRequested(QStringLiteral("orientationDrag:%1,%2").arg(x).arg(y)); };
    auto *ruler = new QLabel(uiText(u"坐标尺 · 刻度待数据接入"), viewportHost); ruler->setObjectName(QStringLiteral("postProcessRulerPlaceholder"));
    ruler->setStyleSheet(QStringLiteral("background:rgba(255,255,255,180); color:#25364c; padding:4px;")); ruler->adjustSize(); ruler->hide(); viewportHost->ruler = ruler;
    sphere->raise(); ruler->raise();
    layout->addWidget(viewportHost, 1); return frame;
}

QWidget *PostProcessWorkbenchWindow::createTimeline() {
    auto *frame = new QFrame(this); frame->setObjectName(uiText(u"postProcessTimeline"));
    auto *layout = new QHBoxLayout(frame); layout->setContentsMargins(8,5,8,5);
    layout->addWidget(new QLabel(uiText(u"时间"), frame));
    const auto addStepButton = [this, frame, layout](const QString &text, const QString &name, const std::function<int(int)> &next) {
        auto *button = new QToolButton(frame); button->setObjectName(name);
        const QString key = name.contains("First") ? "first" : name.contains("Previous") ? "previous" : name.contains("Next") ? "next" : "last";
        button->setIcon(commandIcon(key)); button->setIconSize(QSize(20,20));
        button->setToolTip(text); button->setAccessibleName(text); layout->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, next] { timeSlider_->setValue(next(timeSlider_->value())); });
    };
    addStepButton(uiText(u"首帧"), uiText(u"postProcessFirstFrame"), [](int) { return 0; });
    addStepButton(uiText(u"上一帧"), uiText(u"postProcessPreviousFrame"), [](int value) { return qMax(0, value - 1); });
    auto *play = new QToolButton(frame); play->setObjectName(QStringLiteral("postProcessPlayFrame"));
    play->setCheckable(true); play->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    play->setIconSize(QSize(20,20)); play->setToolTip(uiText(u"播放示例时间帧")); layout->addWidget(play);
    addStepButton(uiText(u"下一帧"), uiText(u"postProcessNextFrame"), [](int value) { return qMin(2, value + 1); });
    addStepButton(uiText(u"末帧"), uiText(u"postProcessLastFrame"), [](int) { return 2; });
    timeSlider_=new QSlider(Qt::Horizontal,frame); timeSlider_->setObjectName(uiText(u"postProcessTimeSlider")); timeSlider_->setRange(0,2); layout->addWidget(timeSlider_,1);
    auto *playTimer = new QTimer(frame); playTimer->setObjectName(QStringLiteral("postProcessPlayTimer")); playTimer->setInterval(500);
    connect(play, &QToolButton::toggled, frame, [this, play, playTimer](bool running) {
        play->setIcon(style()->standardIcon(running ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
        play->setToolTip(running ? uiText(u"暂停示例时间帧") : uiText(u"播放示例时间帧"));
        if (running) { if (timeSlider_->value() == timeSlider_->maximum()) timeSlider_->setValue(0); playTimer->start(); }
        else playTimer->stop();
    });
    connect(playTimer, &QTimer::timeout, frame, [this, play] {
        if (timeSlider_->value() >= timeSlider_->maximum()) { play->setChecked(false); return; }
        timeSlider_->setValue(timeSlider_->value() + 1);
    });
    timeLabel_=new QLabel(uiText(u"Frame 1 / 3 · t = 0.0 s"),frame); layout->addWidget(timeLabel_);
    connect(timeSlider_, &QSlider::valueChanged, this, [this](int value) {
        timeLabel_->setText(uiText(u"示例 Frame %1 / 3 · t = %2 s").arg(value + 1).arg(value * .5, 0, 'f', 1));
        updateSelectionActions();
        appendMessage(uiText(u"界面示例已定位至 t = %1 s；未读取真实帧。").arg(value * .5, 0, 'f', 1));
    }); return frame;
}

void PostProcessWorkbenchWindow::applyBackground(const QColor &bottom, const QColor &top, bool gradient) {
    backgroundBottom_ = bottom; backgroundTop_ = top; gradientBackground_ = gradient;
    auto *host = findChild<QWidget *>(QStringLiteral("postProcessViewportHost"));
    host->setProperty("backgroundBottom", bottom);
    host->setProperty("backgroundTop", gradient ? top : bottom);
    host->setProperty("gradientBackground", gradient);
    for (auto *widget : {host, emptyViewport_}) {
        if (!widget) continue;
        widget->setProperty("backgroundBottom", bottom);
        widget->setProperty("backgroundTop", gradient ? top : bottom);
        widget->update();
    }
    host->setStyleSheet(QStringLiteral("QFrame#postProcessViewportHost { background:%1; }").arg(bottom.name()));
    emit backgroundChanged(bottom, gradient ? top : bottom, gradient);
}

void PostProcessWorkbenchWindow::openBackgroundSettings() {
    if (auto *existing = findChild<QDialog *>(QStringLiteral("postProcessBackgroundDialog"))) {
        existing->raise(); existing->activateWindow(); return;
    }
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName(QStringLiteral("postProcessBackgroundDialog"));
    dialog->setWindowTitle(uiText(u"视口背景")); dialog->resize(430,250);
    auto *layout = new QVBoxLayout(dialog);
    auto *form = new QFormLayout;
    auto *preset = new QComboBox(dialog);
    preset->setObjectName(QStringLiteral("postProcessBackgroundPreset"));
    preset->addItems({uiText(u"自定义"),uiText(u"ParaView 蓝灰渐变（默认）"),uiText(u"ParaView 经典蓝灰"),uiText(u"蓝灰渐变"),uiText(u"白色"),uiText(u"黑色")});
    auto *gradient = new QCheckBox(uiText(u"使用垂直渐变"),dialog);
    gradient->setObjectName(QStringLiteral("postProcessBackgroundGradient")); gradient->setChecked(gradientBackground_);
    auto *bottom = new QLineEdit(backgroundBottom_.name(),dialog);
    auto *top = new QLineEdit(backgroundTop_.name(),dialog);
    bottom->setObjectName(QStringLiteral("postProcessBackgroundBottom"));
    top->setObjectName(QStringLiteral("postProcessBackgroundTop"));
    form->addRow(uiText(u"预设"),preset); form->addRow(gradient);
    const auto colorRow = [this, dialog, form](const QString &title, QLineEdit *input) {
        auto *row = new QWidget(dialog); auto *box = new QHBoxLayout(row); box->setContentsMargins(0,0,0,0);
        box->addWidget(input); auto *choose = new QPushButton(uiText(u"选色…"),row); box->addWidget(choose);
        QObject::connect(choose,&QPushButton::clicked,dialog,[dialog,input] {
            const QColor color = QColorDialog::getColor(QColor(input->text()),dialog,uiText(u"选择背景颜色"));
            if (color.isValid()) input->setText(color.name());
        });
        form->addRow(title,row);
    };
    colorRow(uiText(u"单色 / 渐变底色"),bottom); colorRow(uiText(u"渐变顶色"),top);
    top->setEnabled(gradient->isChecked());
    connect(gradient,&QCheckBox::toggled,top,&QWidget::setEnabled);
    connect(preset,QOverload<int>::of(&QComboBox::currentIndexChanged),dialog,[bottom,top,gradient](int index) {
        const QStringList colors = {QString(),"#ffffff","#52576e","#52576e","#ffffff","#000000"};
        if (index<1) return;
        bottom->setText(colors.at(index)); top->setText(index==1 ? "#52576e" : index==3 ? "#a6a6a6" : colors.at(index));
        gradient->setChecked(index==1 || index==3);
    });
    layout->addLayout(form);
    auto *error = new QLabel(dialog); error->setWordWrap(true); layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(uiText(u"应用"));
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText(u"取消"));
    buttons->button(QDialogButtonBox::RestoreDefaults)->setText(uiText(u"恢复默认"));
    connect(buttons->button(QDialogButtonBox::RestoreDefaults),&QPushButton::clicked,preset,[preset] { preset->setCurrentIndex(0); preset->setCurrentIndex(1); });
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,dialog,[this,dialog,bottom,top,gradient,error] {
        const QColor a(bottom->text()), b(top->text());
        if (!a.isValid() || (gradient->isChecked() && !b.isValid())) {
            error->setText(uiText(u"请输入有效颜色，例如 #625d5a。")); return;
        }
        applyBackground(a,gradient->isChecked() ? b : a,gradient->isChecked());
        dialog->accept();
    });
    layout->addWidget(buttons); dialog->show();
}

void PostProcessWorkbenchWindow::showNavigationPanel(int panel) {
    if (panel < 0 || panel >= 3 || !navigationPanels_[panel]) return;
    auto *tabs = findChild<QTabWidget *>(QStringLiteral("postProcessNavigator"));
    if (!tabs) return;
    int index = tabs->indexOf(navigationPanels_[panel]);
    if (index < 0) {
        index = 0;
        for (int i = 0; i < panel; ++i) if (tabs->indexOf(navigationPanels_[i]) >= 0) ++index;
        const QStringList labels = {uiText(u"视图"), uiText(u"数据"), uiText(u"对象")};
        tabs->insertTab(index, navigationPanels_[panel], labels.at(panel));
    }
    tabs->show(); tabs->setCurrentIndex(index);
}

void PostProcessWorkbenchWindow::updateDataSelection(QTreeWidgetItem *item, int) {
    selectedKind_ = item ? item->data(0, Qt::UserRole).toString() : QString();
    selectedId_ = item ? item->data(0, Qt::UserRole+1).toString() : QString();
    selectedName_ = item ? item->text(0).trimmed() : QString();
    selectedContext_ = item ? item->data(0, Qt::UserRole+3).toMap() : QVariantMap();
    if (selectionSummary_) selectionSummary_->setText(uiText(u"当前选择：") + selectedName_);
    if (selectedKind_ == "time" && timeSlider_ && item->parent())
        timeSlider_->setValue(item->parent()->indexOfChild(item));
    updateSelectionActions();
}

void PostProcessWorkbenchWindow::updateSelectionActions() {
    const bool spatial = !selectedContext_.value("sourceId").toString().isEmpty();
    const int dimension = selectedContext_.value("dimension").toInt();
    const int components = selectedContext_.value("components").toInt();
    const bool scalar = spatial && components == 1;
    const bool vector = spatial && components == 3 && !selectedContext_.value("vectors").toStringList().isEmpty();
    const bool sourceObject = selectedKind_ == "volume" || selectedKind_ == "surface" || selectedKind_ == "exampleFile";
    const bool vectorSource = vector || (spatial && sourceObject &&
        (!selectedContext_.value("vectors").toStringList().isEmpty() || selectedContext_.value("scalars").toStringList().size() >= 3));
    const bool derived = selectedKind_ == "slice" || selectedKind_ == "iso" || selectedKind_ == "isoline" || selectedKind_ == "stream" || selectedKind_ == "trace" || selectedKind_ == "contour";
    const auto enable = [this](const char *name, bool enabled, const QString &reason) {
        auto *action = findChild<QAction *>(name);
        if (!action) return;
        action->setEnabled(enabled);
        action->setToolTip(enabled ? action->property("availableToolTip").toString() : reason);
    };
    enable("postProcessFocusAction",spatial,uiText(u"请先选择空间对象。"));
    enable("postProcessContourAction",spatial && (scalar || vector),uiText(u"请先选择标量或矢量模长进行着色。"));
    enable("postProcessSliceAction",spatial && dimension==3,uiText(u"切片需要三维体网格；不要求存在标量场。"));
    enable("postProcessIsoAction",scalar && dimension==3,uiText(u"等值面需要三维体网格和标量；矢量请先选择模长标量。"));
    enable("postProcessIsoLineAction",scalar && dimension==2,uiText(u"等值线需要物面或切片，以及标量字段。"));
    enable("postProcessStreamAction",vectorSource && dimension>=2,uiText(u"流线需要矢量场；只有速度大小标量无法积分，请选择速度矢量。"));
    enable("postProcessTraceAction",vector && dimension==3 && selectedContext_.value("frames").toInt()>1,uiText(u"轨迹需要三维矢量场和多个有效时间帧。"));
    styleAction_->setEnabled((spatial && selectedKind_!="field") || selectedKind_=="curve");
    parameterAction_->setEnabled(derived || selectedKind_=="field" || selectedKind_=="curve");
    enable("postProcessColorAction",spatial && (scalar || vector),uiText(u"请先选择一个可着色的字段或空间对象。"));
    // Export actions open a request dialog; no file is written by the UI.
    enable("postProcessAnimationExportAction",spatial && selectedContext_.value("frames").toInt()>1,uiText(u"请选择包含时间帧的数据。"));
    enable("postProcessImageExportAction",spatial,uiText(u"请选择要保存的视图。"));
    for (const char *name : {"postProcessAnimationExportAction", "postProcessImageExportAction"}) {
        auto *action = findChild<QAction *>(QString::fromLatin1(name));
        if (action && action->isEnabled()) action->setToolTip(uiText(u"选择导出目标并提交请求；导出服务尚未接入，不会生成文件。"));
    }
    enable("postProcessProbeAction",spatial,uiText(u"请先导入并选择可查询的网格或对象。"));
    if (viewportHint_) viewportHint_->setText(spatial
        ? uiText(u"界面示例 · %1 · %2 · %3").arg(selectedContext_.value("topology").toString(),selectedName_,selectedContext_.value("field").toString())
        : uiText(u"未选择空间数据"));

}

void PostProcessWorkbenchWindow::openNewFeatureDialog(const QString &kind) {
    if (sourceDialog_) { sourceDialog_->raise(); sourceDialog_->activateWindow(); return; }
    if (parameterDialog_) { parameterDialog_->raise(); parameterDialog_->activateWindow(); return; }
    const QString title = kind == "contour" ? uiText(u"新建云图") : kind == "slice" ? uiText(u"新建切面")
        : kind == "isoline" ? uiText(u"新建等值线") : kind == "iso" ? uiText(u"新建等值面")
        : kind == "trace" ? uiText(u"新建迹线") : uiText(u"新建流线");
    auto *dialog = new QDialog(this);
    sourceDialog_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setWindowTitle(title);
    dialog->setObjectName(QStringLiteral("postProcessFeatureSourceDialog"));
    dialog->setStyleSheet(centralWidget()->styleSheet());
    dialog->resize(500, 230);
    auto *layout = new QVBoxLayout(dialog);
    layout->addWidget(sectionLabel(title, dialog));
    auto *form = new QFormLayout;
    auto *files = new QComboBox(dialog);
    files->setObjectName(QStringLiteral("postProcessFeatureFile"));
    auto *sources = new QComboBox(dialog);
    sources->setObjectName(QStringLiteral("postProcessFeatureSource"));
    auto *fields = new QComboBox(dialog);
    fields->setObjectName(QStringLiteral("postProcessFeatureField"));
    auto addSource = [sources, kind](const QString &label, const QString &id, const QVariantMap &context) {
        const int dimension = context.value("dimension").toInt();
        if (context.value("sourceId").toString().isEmpty()) return;
        if ((kind == "slice" || kind == "iso") && dimension != 3) return;
        if (kind == "isoline" && dimension != 2) return;
        if ((kind == "stream" || kind == "contour") && dimension < 2) return;
        if (kind == "trace" && (dimension != 3 || context.value("frames").toInt() < 2)) return;
        QVariantMap candidate = context;
        candidate["inputObjectId"] = id;
        sources->addItem(label, candidate);
    };
    for (QTreeWidgetItemIterator it(datasetTree_); *it; ++it) {
        auto *item = *it;
        const QString itemKind = item->data(0, Qt::UserRole).toString();
        if (itemKind == "volume" || itemKind == "surface") {
            QVariantMap context = item->data(0, Qt::UserRole+3).toMap();
            if (context.value("sourceName").toString().isEmpty()) {
                auto *root = item; while (root->parent()) root = root->parent();
                context["sourceName"] = root->text(0);
            }
            addSource(item->text(0), item->data(0, Qt::UserRole+1).toString(), context);
        }
    }
    for (int i = 0; i < sceneList_->count(); ++i) {
        auto *item = sceneList_->item(i);
        addSource(item->text(), item->data(Qt::UserRole+1).toString(), item->data(Qt::UserRole+3).toMap());
    }
    QVector<QPair<QString,QVariantMap>> candidates;
    for (int i=0; i<sources->count(); ++i) {
        const QVariantMap context = sources->itemData(i).toMap();
        candidates.append(qMakePair(sources->itemText(i),context));
        const QString sourceId = context.value("sourceId").toString();
        if (files->findData(sourceId) < 0)
            files->addItem(context.value("sourceName",sourceId).toString(),sourceId);
    }
    form->addRow(uiText(u"数据集"), files);
    form->addRow(kind == "slice" || kind == "iso" || kind == "trace" ? uiText(u"体区域") : kind == "isoline" ? uiText(u"物面 / 切面") : uiText(u"区域 / 对象"), sources);
    if (kind != "slice") form->addRow(kind == "stream" || kind == "trace" ? uiText(u"速度矢量") : kind == "isoline" || kind == "iso" ? uiText(u"等值标量") : uiText(u"着色字段"), fields);
    else fields->hide();
    const bool integration = kind == "stream" || kind == "trace";
    auto *u = new QComboBox(dialog); auto *v = new QComboBox(dialog); auto *w = new QComboBox(dialog);
    u->setObjectName(QStringLiteral("postProcessVectorU")); v->setObjectName(QStringLiteral("postProcessVectorV")); w->setObjectName(QStringLiteral("postProcessVectorW"));
    if (integration) { form->addRow(uiText(u"U 分量"), u); form->addRow(uiText(u"V 分量"), v); form->addRow(uiText(u"W 分量"), w); }
    else { u->hide(); v->hide(); w->hide(); }
    layout->addLayout(form);
    auto *hint = new QLabel(dialog);
    hint->setObjectName(QStringLiteral("postProcessFeatureSourceHint"));
    hint->setWordWrap(true); layout->addWidget(hint);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(uiText(u"下一步"));
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText(u"取消"));
    layout->addWidget(buttons);
    const auto validate = [sources, fields, u, v, w, buttons, hint, kind, integration] {
        const bool manual = integration && fields->currentData().toString() == QStringLiteral("__components__");
        u->setEnabled(manual); v->setEnabled(manual); w->setEnabled(manual);
        const bool distinct = u->currentIndex() >= 0 && v->currentIndex() >= 0 && w->currentIndex() >= 0 && u->currentText() != v->currentText() && u->currentText() != w->currentText() && v->currentText() != w->currentText();
        const bool valid = sources->currentIndex() >= 0 && (kind == "slice" || fields->count() > 0) && (!manual || distinct);
        buttons->button(QDialogButtonBox::Ok)->setEnabled(valid);
        hint->setText(sources->currentIndex() < 0 ? uiText(u"没有符合要求的数据源，请先导入或选择示例数据。")
                     : manual ? (distinct ? uiText(u"请确认三个分量属于同一数据位置；当前仅保存界面草稿。") : uiText(u"请选择三个不同的分量。"))
                     : !valid ? uiText(u"所选数据源缺少此功能所需的字段，请选择其他数据源。") : uiText(u"请确认输入区域及可用字段；未读取的文件不能用于分析。"));
    };
    const auto updateFields = [sources, fields, u, v, w, kind, integration, validate](int index) {
        const QSignalBlocker blockFields(fields), blockU(u), blockV(v), blockW(w);
        fields->clear(); u->clear(); v->clear(); w->clear();
        const QVariantMap source = sources->itemData(index).toMap();
        if (kind == "slice") { fields->addItem(uiText(u"仅切片网格"), QString()); validate(); return; }
        QStringList available = source.value(integration ? "vectors" : "scalars").toStringList();
        for (const QString &name : available) fields->addItem(name, name);
        const QStringList scalars = source.value("scalars").toStringList();
        if (integration && scalars.size() >= 3) fields->addItem(uiText(u"手动组合 U / V / W"), QStringLiteral("__components__"));
        u->addItems(scalars); v->addItems(scalars); w->addItems(scalars);
        if (scalars.size() >= 3) { u->setCurrentIndex(0); v->setCurrentIndex(1); w->setCurrentIndex(2); }
        const int preferred = fields->findData(source.value("field")); if (preferred >= 0) fields->setCurrentIndex(preferred);
        fields->setEnabled(index >= 0); validate();
    };
    connect(sources, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, updateFields);
    for (QComboBox *input : {fields, u, v, w}) connect(input, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, [validate](int) { validate(); });
    const auto updateSources = [files, sources, candidates, updateFields, this](int) {
        const QSignalBlocker blocker(sources);
        sources->clear();
        for (const auto &candidate : candidates)
            if (candidate.second.value("sourceId") == files->currentData())
                sources->addItem(candidate.first,candidate.second);
        for (int i=0; i<sources->count(); ++i)
            if (sources->itemData(i).toMap().value("inputObjectId").toString() == selectedId_) { sources->setCurrentIndex(i); break; }
        updateFields(sources->currentIndex());
    };
    connect(files, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, updateSources);
    const int preferredFile = files->findData(selectedContext_.value("sourceId"));
    if (preferredFile >= 0) files->setCurrentIndex(preferredFile);
    updateSources(files->currentIndex());
    int preferredSource = -1;
    for (int i = 0; i < sources->count(); ++i) {
        const QVariantMap candidate = sources->itemData(i).toMap();
        if (candidate.value("inputObjectId").toString() == selectedId_) { preferredSource = i; break; }
        if (preferredSource < 0 && candidate.value("sourceId").toString() == selectedContext_.value("sourceId").toString()) preferredSource = i;
    }
    if (preferredSource >= 0) sources->setCurrentIndex(preferredSource);
    updateFields(sources->currentIndex());
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, sources, fields, u, v, w, kind, title] {
        QVariantMap context = sources->currentData().toMap();
        if (context.isEmpty()) return;
        context["field"] = fields->currentData().toString();
        if (context.value("field").toString() == QStringLiteral("__components__")) {
            context["vectorComponents"] = QStringList{u->currentText(), v->currentText(), w->currentText()};
            context["field"] = uiText(u"组合矢量（%1，%2，%3）").arg(u->currentText(), v->currentText(), w->currentText());
            QStringList vectors = context.value("vectors").toStringList(); vectors.append(context.value("field").toString()); context["vectors"] = vectors;
        }
        context["components"] = context.value("field").toString().isEmpty() ? 0
            : (context.value("vectors").toStringList().contains(context.value("field").toString()) ? 3 : 1);
        selectedContext_ = context;
        selectedId_ = context.value("inputObjectId").toString();
        dialog->accept();
        addDerivedLayer(kind, title.mid(2), uiText(u"已创建对象草稿，请设置参数。"));
    });
    dialog->show();
}

void PostProcessWorkbenchWindow::addDerivedLayer(const QString &kind, const QString &name, const QString &message) {
    if (selectedContext_.isEmpty() || parameterDialog_) return;
    const auto sourceContext = selectedContext_;
    emit postProcessCommandRequested(kind);
    auto *item = new QListWidgetItem(commandIcon(kind == "isoline" ? "iso" : kind), name + uiText(u" · 草稿 · 可见"), sceneList_);
    item->setData(Qt::UserRole, kind);
    item->setData(Qt::UserRole+1, QStringLiteral("object-%1").arg(nextObjectId_++));
    item->setData(Qt::UserRole+2, name);
    auto context = sourceContext;
    context["draft"] = true;
    context["sourceDimension"] = sourceContext.value("dimension");
    context["inputObjectId"] = selectedId_;
    if (kind=="stream" || kind=="trace") {
        context["integrationField"] = context.value("field");
        context["field"] = context.value("scalars").toStringList().value(0);
        context["components"] = context.value("field").toString().isEmpty() ? 0 : 1;
    }
    if (kind=="slice" || kind=="iso") context["dimension"] = 2;
    if (kind=="stream" || kind=="trace" || kind=="isoline") {
        context["dimension"] = 1; context["vectors"] = QStringList();
    }
    item->setData(Qt::UserRole+3,context);
    showNavigationPanel(2);
    sceneList_->setCurrentItem(item);
    updateSelectionDetails();
    appendMessage(message);
    const QString newId = item->data(Qt::UserRole+1).toString();
    openObjectParameters();
    if (parameterDialog_) {
        connect(parameterDialog_, &QDialog::rejected, this, [this, newId] {
            if (parameterDialog_ && parameterDialog_->property("draftApplied").toBool()) return;
            for (int i = 0; i < sceneList_->count(); ++i) {
                if (sceneList_->item(i)->data(Qt::UserRole+1).toString() == newId) {
                    delete sceneList_->takeItem(i);
                    parameterDrafts_.remove(newId); styleDrafts_.remove(newId);
                    updateSelectionDetails();
                    break;
                }
            }
        });
    }
}
bool PostProcessWorkbenchWindow::insertWidget(WidgetSlot slot,QWidget *widget){if(slot!=WidgetSlot::Viewport||!widget||!viewportLayout_)return false;if(emptyViewport_){viewportLayout_->removeWidget(emptyViewport_);emptyViewport_->deleteLater();emptyViewport_=nullptr;}if(viewportLayout_->count()!=0)return false;viewportLayout_->addWidget(widget); if (auto *sphere = findChild<QWidget *>(QStringLiteral("postProcessOrientationControl"))) sphere->raise(); if (auto *ruler = findChild<QWidget *>(QStringLiteral("postProcessRulerPlaceholder"))) ruler->raise(); return true;}
void PostProcessWorkbenchWindow::bindServices(const ServiceBindings &services){if(bound_)return;services_=services;bound_=true;}
void PostProcessWorkbenchWindow::appendMessage(const QString &message){statusBar()->showMessage(message,5000);}
void PostProcessWorkbenchWindow::updateSelectionDetails() {
    auto *item = sceneList_->currentItem();
    selectedId_ = item ? item->data(Qt::UserRole+1).toString() : QString();
    selectedKind_ = item ? item->data(Qt::UserRole).toString() : QString();
    selectedName_ = item ? item->data(Qt::UserRole+2).toString() : QString();
    selectedContext_ = item ? item->data(Qt::UserRole+3).toMap() : QVariantMap();
    updateSelectionActions();
}

void PostProcessWorkbenchWindow::openSelectedSettings() {
    if (selectedKind_ == "volume" || selectedKind_ == "surface") openObjectStyle();
    else if (parameterAction_->isEnabled()) openObjectParameters();
}

void PostProcessWorkbenchWindow::openObjectStyle() {
    if (!styleAction_->isEnabled()) return;
    if (styleDialog_) { styleDialog_->raise(); styleDialog_->activateWindow(); return; }
    const QString id = selectedId_;
    const QString name = selectedName_;
    const QVariantMap draft = styleDrafts_.value(id);
    auto *dialog = new QDialog(this);
    styleDialog_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setObjectName(QStringLiteral("postProcessObjectStyleDialog"));
    dialog->setWindowTitle(uiText(u"对象样式 · ") + name);
    dialog->setStyleSheet(centralWidget()->styleSheet());
    dialog->resize(480, 280);
    auto *layout = new QVBoxLayout(dialog);
    layout->addWidget(sectionLabel(uiText(u"对象样式"), dialog));
    auto *context = new QLabel(uiText(u"当前对象：") + name, dialog);
    context->setWordWrap(true); layout->addWidget(context);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *visible = new QCheckBox(uiText(u"在当前 Frame 中显示"), dialog);
    visible->setObjectName(QStringLiteral("objectStyleVisible"));
    visible->setChecked(draft.value("visible", true).toBool());
    auto *representation = new QComboBox(dialog);
    representation->setObjectName(QStringLiteral("objectStyleRepresentation"));
    representation->addItems({uiText(u"表面"), uiText(u"线框"), uiText(u"表面和边线"), uiText(u"点")});
    representation->setCurrentIndex(draft.value("representation", 0).toInt());
    auto *color = new QComboBox(dialog);
    color->setObjectName(QStringLiteral("objectStyleColor"));
    color->addItems({uiText(u"继承图层"), uiText(u"蓝色"), uiText(u"青色"), uiText(u"橙色"), uiText(u"灰色")});
    color->setCurrentIndex(draft.value("color", 0).toInt());
    auto *opacity = new QSpinBox(dialog);
    opacity->setObjectName(QStringLiteral("objectStyleOpacity"));
    opacity->setRange(0,100); opacity->setSuffix(" %"); opacity->setValue(draft.value("opacity",100).toInt());
    form->addRow(visible); form->addRow(uiText(u"显示方式"), representation);
    form->addRow(uiText(u"颜色"), color); form->addRow(uiText(u"不透明度"), opacity);
    layout->addLayout(form);
    auto *hint = new QLabel(uiText(u"当前保存的是此对象的界面草稿。场景服务接入后才会更新实际渲染。"), dialog);
    hint->setObjectName(QStringLiteral("mutedText")); hint->setWordWrap(true);
    layout->addWidget(hint); layout->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(uiText(u"保存草稿"));
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText(u"取消")); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, id, name, visible, color, opacity, representation] {
        styleDrafts_[id] = {{"visible", visible->isChecked()}, {"representation", representation->currentIndex()},
                            {"color", color->currentIndex()}, {"opacity", opacity->value()}};
        for (int i = 0; i < sceneList_->count(); ++i) {
            auto *item = sceneList_->item(i);
            if (item->data(Qt::UserRole+1).toString() == id)
                item->setText(item->data(Qt::UserRole+2).toString() + uiText(u" · 草稿") + (visible->isChecked() ? uiText(u" · 可见") : uiText(u" · 隐藏")));
        }
        appendMessage(uiText(u"已保存“%1”的样式草稿；实际渲染尚未接入。").arg(name));
        dialog->accept();
    });
    dialog->show();
}

void PostProcessWorkbenchWindow::openObjectParameters(bool colorOnly) {
    if (!parameterAction_->isEnabled() && selectedContext_.isEmpty()) return;
    if (parameterDialog_) { parameterDialog_->raise(); parameterDialog_->activateWindow(); return; }
    const QString id = selectedId_;
    const QString kind = colorOnly || selectedKind_=="volume" || selectedKind_=="surface" ? QStringLiteral("field") : selectedKind_;
    const QString name = selectedName_;
    const QVariantMap draft = parameterDrafts_.value(id);
    auto *dialog = new QDialog(this);
    parameterDialog_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setObjectName(QStringLiteral("postProcessParameterDialog"));
    const QString title = kind == "slice" ? uiText(u"编辑切面") : kind == "iso" ? uiText(u"编辑等值面") : kind == "isoline" ? uiText(u"编辑等值线")
        : kind == "stream" ? uiText(u"编辑流线") : kind == "trace" ? uiText(u"编辑迹线")
        : kind == "curve" ? uiText(u"曲线设置") : (kind == "field" || kind == "contour") ? uiText(u"字段与色标") : uiText(u"对象参数");
    dialog->setWindowTitle(title + uiText(u" · ") + name);
    dialog->setStyleSheet(centralWidget()->styleSheet());
    dialog->resize(520, kind == "slice" || kind == "stream" || kind == "trace" ? 540 : 380);
    auto *layout = new QVBoxLayout(dialog);
    layout->addWidget(sectionLabel(title, dialog));
    auto *context = new QLabel(uiText(u"当前对象：") + name, dialog); context->setWordWrap(true); layout->addWidget(context);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    QMap<QString, QDoubleSpinBox *> numbers;
    QMap<QString, QComboBox *> choices;
    QMap<QString, QLineEdit *> texts;
    const auto number = [&](const QString &key, const QString &label, double value) {
        auto *input = new QDoubleSpinBox(dialog); input->setObjectName(key);
        input->setDecimals(4); input->setRange(-1.e9,1.e9); input->setValue(draft.value(key,value).toDouble());
        form->addRow(label,input); numbers.insert(key,input);
    };
    const auto choice = [&](const QString &key, const QString &label, const QStringList &items) {
        auto *input = new QComboBox(dialog); input->setObjectName(key); input->addItems(items);
        input->setCurrentIndex(draft.value(key,0).toInt()); form->addRow(label,input); choices.insert(key,input);
    };
    const auto textInput = [&](const QString &key, const QString &label, const QString &placeholder) {
        auto *input = new QLineEdit(draft.value(key).toString(), dialog);
        input->setObjectName(key); input->setPlaceholderText(placeholder);
        form->addRow(label, input); texts.insert(key, input);
    };
    if (kind == "slice") {
        choice("normal",uiText(u"切面定义"), {"X", "Y", "Z", uiText(u"原点和法向")});
        number("offset",uiText(u"位置"),0.0);
        number("originX",uiText(u"原点 X"),0.0); number("originY",uiText(u"原点 Y"),0.0); number("originZ",uiText(u"原点 Z"),0.0);
        number("normalX",uiText(u"法向 X"),1.0); number("normalY",uiText(u"法向 Y"),0.0); number("normalZ",uiText(u"法向 Z"),0.0);
        choice("sliceStyle",uiText(u"切面显示"),{uiText(u"表面"),uiText(u"网格线"),uiText(u"表面和网格线")});
    } else if (kind == "iso" || kind == "isoline") {
        if (kind == "isoline") choice("levelMode",uiText(u"等值设置"),{uiText(u"指定等值"),uiText(u"范围与数量")});
        number("isoValue",uiText(u"等值"),0.0);
        textInput("additionalValues",uiText(u"其他等值"),uiText(u"用逗号分隔，可留空"));
        if (kind == "isoline") {
            number("levelMin",uiText(u"最小等值"),0.0); number("levelMax",uiText(u"最大等值"),1.0);
            number("levelCount",uiText(u"等值数量"),10); numbers.value("levelCount")->setDecimals(0); numbers.value("levelCount")->setRange(2,5000);
        }
        choice("featureColor",uiText(u"颜色"),{uiText(u"按字段着色"),uiText(u"蓝色"),uiText(u"白色"),uiText(u"黑色")});
        choice("colorField",uiText(u"着色标量"),selectedContext_.value("scalars").toStringList());
        if (kind == "iso") { number("opacity",uiText(u"不透明度 / %"),100.0); numbers.value("opacity")->setRange(0,100); }
        else { number("lineWidth",uiText(u"线宽"),1.0); numbers.value("lineWidth")->setMinimum(0.1); }
    } else if (kind == "stream" || kind == "trace") {
        if (kind == "trace") choice("traceType",uiText(u"迹线类型"),{uiText(u"粒子路径"),uiText(u"连续释放迹线")});
        if (kind == "stream") {
            choice("streamType",uiText(u"流线类型"),{uiText(u"体流线"),uiText(u"表面流线")});
            if (!draft.contains("streamType") && selectedContext_.value("sourceDimension", selectedContext_.value("dimension")).toInt() == 2) choices.value("streamType")->setCurrentIndex(1);
        }
        choice("seedMode",uiText(u"种子方式"),{uiText(u"单点坐标"),uiText(u"线段")});
        number("seedX",uiText(u"种子 X"),0.0);
        number("seedY",uiText(u"种子 Y"),0.0);
        number("seedZ",uiText(u"种子 Z"),0.0);
        if (kind == "stream") choice("direction",uiText(u"积分方向"), {uiText(u"向前"),uiText(u"向后"),uiText(u"双向")});
        number("seedEndX",uiText(u"线种子终点 X"),0.0); number("seedEndY",uiText(u"线种子终点 Y"),0.0); number("seedEndZ",uiText(u"线种子终点 Z"),0.0);
        number("seedCount",uiText(u"种子数量"),2.0);
        numbers.value("seedCount")->setDecimals(0); numbers.value("seedCount")->setRange(2,1000000);
        number("stepSize",uiText(u"积分步长"),0.01); numbers.value("stepSize")->setMinimum(0.0001);
        if (kind == "trace") {
            number("releaseTime",uiText(u"开始释放时间"),0.0); number("endTime",uiText(u"跟踪结束时间"),1.0);
            number("releaseCount",uiText(u"每时间单位释放数量"),1.0); numbers.value("releaseCount")->setDecimals(0); numbers.value("releaseCount")->setMinimum(1);
        }
        number("maxLength",uiText(u"最大长度"),1.0); numbers.value("maxLength")->setMinimum(0.0001);
        number("maxSteps",uiText(u"最大步数"),1000.0); numbers.value("maxSteps")->setDecimals(0); numbers.value("maxSteps")->setMinimum(1);
        choice("streamColor",uiText(u"显示着色"),{uiText(u"单色"),uiText(u"按标量字段")});
        choice("colorField",uiText(u"着色标量"),selectedContext_.value("scalars").toStringList());
    }
    if (kind == "field" || kind == "contour") {
        QStringList fields = selectedContext_.value("scalars").toStringList();
        if (fields.isEmpty() && !selectedContext_.value("field").toString().isEmpty())
            fields.append(selectedContext_.value("field").toString());
        choice("displayField", uiText(u"字段"), fields);
        if (!draft.contains("displayField"))
            choices.value("displayField")->setCurrentIndex(qMax(0, fields.indexOf(selectedContext_.value("field").toString())));
        choice("palette",uiText(u"配色方案"),{uiText(u"蓝-青-黄-红"),uiText(u"灰度"),uiText(u"蓝-白-红")});
        choices.value("palette")->setToolTip(uiText(u"蓝青黄红：连续量变化；灰度：由暗到亮；蓝白红：突出两端差异，白色不保证对应零。"));
        choice("rangeMode",uiText(u"范围模式"),{uiText(u"自动：当前对象 / 字段"),uiText(u"手动：指定上下限")});
        number("rangeMin",uiText(u"手动最小值"),0.0); number("rangeMax",uiText(u"手动最大值"),1.0);
        choice("legendVisible",uiText(u"色标显示"),{uiText(u"显示"),uiText(u"隐藏")});
        choices.value("legendVisible")->setToolTip(uiText(u"显示或隐藏颜色与数值的对应图例，不改变对象显隐。"));
        auto *range = new QLabel(uiText(u"真实结果读取后计算"),dialog); form->addRow(uiText(u"数据实际值域"),range);
        range->setToolTip(uiText(u"数据自身的最小值和最大值，与手动指定的颜色映射范围分别显示。"));
        auto *rangeHint = new QLabel(dialog); rangeHint->setWordWrap(true); form->addRow(rangeHint);
        const auto updateRange = [choices, numbers, rangeHint](int index) {
            numbers.value("rangeMin")->setEnabled(index == 1); numbers.value("rangeMax")->setEnabled(index == 1);
            rangeHint->setText(index == 0
                ? uiText(u"自动范围使用当前对象、当前字段和当前时间步的实际最小 / 最大值。读取服务接入后计算。")
                : uiText(u"手动范围固定颜色映射上下限，便于对比不同对象或时间步；范围外数值使用端点颜色，不裁剪数据。"));
        };
        connect(choices.value("rangeMode"), QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, updateRange);
        updateRange(choices.value("rangeMode")->currentIndex());
    } else if (kind == "curve") {
        choice("xAxis",uiText(u"横轴"),{uiText(u"迭代步"),uiText(u"时间"),uiText(u"工况序号")});
    } else if (kind != "layer") {
        const bool integration = kind=="stream" || kind=="trace";
        QStringList fields = integration ? QStringList{selectedContext_.value("integrationField").toString()}
                                         : selectedContext_.value("scalars").toStringList();
        fields.removeAll(QString());
        if (kind == "slice") fields.prepend(uiText(u"仅切片网格"));
        choice("displayField", integration ? uiText(u"积分矢量") : uiText(u"字段"), fields);
        if (!draft.contains("displayField"))
            choices.value("displayField")->setCurrentIndex(qMax(0, fields.indexOf(selectedContext_.value(integration ? "integrationField" : "field").toString())));
    } else {
        auto *hint = new QLabel(uiText(u"通用对象尚未指定特征类型，请先创建切面、等值面或流线对象。"),dialog);
        hint->setWordWrap(true); form->addRow(hint);
    }
    if (kind == "isoline" || kind == "iso") {
        const auto updateFeatureColor = [form, choices](int mode) {
            auto *field = choices.value("colorField");
            field->setVisible(mode == 0); form->labelForField(field)->setVisible(mode == 0);
        };
        connect(choices.value("featureColor"), QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, updateFeatureColor);
        updateFeatureColor(choices.value("featureColor")->currentIndex());
    }
    if (kind == "isoline") {
        const auto updateLevels = [form, numbers, texts](int mode) {
            for (const QString &key : {QStringLiteral("levelMin"),QStringLiteral("levelMax"),QStringLiteral("levelCount")}) {
                numbers.value(key)->setVisible(mode == 1); form->labelForField(numbers.value(key))->setVisible(mode == 1);
            }
            numbers.value("isoValue")->setVisible(mode == 0); form->labelForField(numbers.value("isoValue"))->setVisible(mode == 0);
            texts.value("additionalValues")->setVisible(mode == 0); form->labelForField(texts.value("additionalValues"))->setVisible(mode == 0);
        };
        connect(choices.value("levelMode"), QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, updateLevels);
        updateLevels(choices.value("levelMode")->currentIndex());
    }
    if (kind == "trace") {
        const auto updateRelease = [form, numbers](int type) {
            numbers.value("releaseCount")->setVisible(type == 1); form->labelForField(numbers.value("releaseCount"))->setVisible(type == 1);
        };
        connect(choices.value("traceType"), QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, updateRelease);
        updateRelease(choices.value("traceType")->currentIndex());
    }
    if (kind == "slice") {
        const auto updateSlice = [form, numbers](int index) {
            const bool arbitrary = index == 3;
            numbers.value("offset")->setVisible(!arbitrary); form->labelForField(numbers.value("offset"))->setVisible(!arbitrary);
            for (const QString &key : {QStringLiteral("originX"),QStringLiteral("originY"),QStringLiteral("originZ"),QStringLiteral("normalX"),QStringLiteral("normalY"),QStringLiteral("normalZ")})
                { numbers.value(key)->setVisible(arbitrary); form->labelForField(numbers.value(key))->setVisible(arbitrary); }
        };
        connect(choices.value("normal"), QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, updateSlice);
        updateSlice(choices.value("normal")->currentIndex());
    }
    if (kind == "stream" || kind == "trace") {
        const auto updateSeed = [form, numbers](int index) {
            for (const QString &key : {QStringLiteral("seedX"),QStringLiteral("seedY"),QStringLiteral("seedZ")})
                { numbers.value(key)->setVisible(index != 2); form->labelForField(numbers.value(key))->setVisible(index != 2); }
            for (const QString &key : {QStringLiteral("seedEndX"),QStringLiteral("seedEndY"),QStringLiteral("seedEndZ"),QStringLiteral("seedCount")})
                { numbers.value(key)->setVisible(index == 1); form->labelForField(numbers.value(key))->setVisible(index == 1); }
        };
        connect(choices.value("seedMode"), QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, updateSeed);
        updateSeed(choices.value("seedMode")->currentIndex());
        const auto updateColor = [form, choices](int index) {
            auto *field = choices.value("colorField");
            field->setVisible(index == 1); form->labelForField(field)->setVisible(index == 1);
        };
        connect(choices.value("streamColor"), QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, updateColor);
        updateColor(choices.value("streamColor")->currentIndex());
    }
    auto *formWidget = new QWidget(dialog); formWidget->setLayout(form);
    layout->addWidget(scrollPanel(formWidget), 1);
    auto *hint = new QLabel(uiText(u"仅保存当前对象的参数草稿，不执行数据读取、特征计算或专业参数校验。"),dialog);
    hint->setWordWrap(true); hint->setObjectName(QStringLiteral("mutedText")); layout->addWidget(hint); layout->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(uiText(u"保存草稿"));
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText(u"取消")); layout->addWidget(buttons);
    auto *apply = buttons->addButton(uiText(u"应用（待接入）"), QDialogButtonBox::ApplyRole);
    apply->setObjectName(QStringLiteral("postProcessApplyDraft"));
    if (choices.contains("rangeMode")) {
        const auto validateRange = [choices, numbers, buttons, apply, hint] {
            const bool valid = choices.value("rangeMode")->currentIndex() == 0
                || numbers.value("rangeMin")->value() < numbers.value("rangeMax")->value();
            buttons->button(QDialogButtonBox::Ok)->setEnabled(valid); apply->setEnabled(valid);
            hint->setText(valid ? uiText(u"仅保存当前对象的参数草稿，实际着色待渲染服务接入。")
                                : uiText(u"手动最小值必须小于手动最大值。"));
        };
        connect(choices.value("rangeMode"), QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, [validateRange](int) { validateRange(); });
        for (const QString &key : {QStringLiteral("rangeMin"),QStringLiteral("rangeMax")})
            connect(numbers.value(key), QOverload<double>::of(&QDoubleSpinBox::valueChanged), dialog, [validateRange](double) { validateRange(); });
        validateRange();
    }
    if (kind == "slice" || kind == "iso" || kind == "isoline" || kind == "trace" || kind == "stream") {
        const auto validateInputs = [this, kind, choices, numbers, texts, buttons, apply, hint] {
            QString error;
            if (kind == "slice" && choices.value("normal")->currentIndex() == 3
                && numbers.value("normalX")->value() == 0 && numbers.value("normalY")->value() == 0
                && numbers.value("normalZ")->value() == 0)
                error = uiText(u"切面法向不能为零向量。");
            if (kind == "iso" || (kind == "isoline" && choices.value("levelMode")->currentIndex() == 0)) {
                const QString extra = texts.value("additionalValues")->text().trimmed();
                if (!extra.isEmpty()) {
                    const QStringList values = extra.split(',', QString::KeepEmptyParts);
                    if (kind == "iso" && values.size() > 2) error = uiText(u"等值面最多指定三个等值。");
                    for (const QString &value : values) {
                        bool ok = false; value.trimmed().toDouble(&ok);
                        if (!ok) { error = uiText(u"其他等值请用英文逗号分隔有效数字。"); break; }
                    }
                }
            }
            if ((kind == "iso" || kind == "isoline") && choices.value("featureColor")->currentIndex() == 0
                && choices.value("colorField")->count() == 0) error = uiText(u"当前来源没有可用于着色的标量字段。");
            if (kind == "isoline" && choices.value("levelMode")->currentIndex() == 1
                && numbers.value("levelMin")->value() >= numbers.value("levelMax")->value())
                error = uiText(u"最小等值必须小于最大等值。");
            if ((kind == "stream" || kind == "trace") && choices.value("seedMode")->currentIndex() == 1
                && numbers.value("seedX")->value() == numbers.value("seedEndX")->value()
                && numbers.value("seedY")->value() == numbers.value("seedEndY")->value()
                && numbers.value("seedZ")->value() == numbers.value("seedEndZ")->value())
                error = uiText(u"线段种子的起点和终点不能相同。");
            if (kind == "stream" && choices.value("streamType")->currentIndex() == 0 && selectedContext_.value("sourceDimension", selectedContext_.value("dimension")).toInt() != 3)
                error = uiText(u"体流线需要三维体区域，请选择表面流线。");
            if (kind == "trace" && numbers.value("endTime")->value() <= numbers.value("releaseTime")->value())
                error = uiText(u"跟踪结束时间须晚于释放时间。");
            if ((kind == "stream" || kind == "trace") && choices.value("streamColor")->currentIndex() == 1
                && choices.value("colorField")->count() == 0)
                error = uiText(u"当前来源没有可用于着色的标量字段。");
            buttons->button(QDialogButtonBox::Ok)->setEnabled(error.isEmpty()); apply->setEnabled(error.isEmpty());
            hint->setText(error.isEmpty() ? uiText(u"仅保存参数草稿，实际计算和渲染待接入。") : error);
        };
        for (auto *input : numbers) connect(input, QOverload<double>::of(&QDoubleSpinBox::valueChanged), dialog, [validateInputs](double) { validateInputs(); });
        for (auto *input : choices) connect(input, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, [validateInputs](int) { validateInputs(); });
        for (auto *input : texts) connect(input, &QLineEdit::textChanged, dialog, [validateInputs](const QString &) { validateInputs(); });
        validateInputs();
    }
    const auto updateDraftField = [this, id, kind, choices] {
        if (!choices.contains("displayField")) return;
        QString field = choices.value("displayField")->currentText();
        if (kind == "slice" && choices.value("displayField")->currentIndex() == 0) field.clear();
        for (int i=0; i<sceneList_->count(); ++i) {
            auto *item = sceneList_->item(i);
            if (item->data(Qt::UserRole+1).toString() != id) continue;
            QVariantMap context = item->data(Qt::UserRole+3).toMap();
            if (kind == "stream" || kind == "trace") context["integrationField"] = field;
            else {
                context["field"] = field;
                context["components"] = field.isEmpty() ? 0 : (context.value("vectors").toStringList().contains(field) ? 3 : 1);
            }
            item->setData(Qt::UserRole+3,context);
            if (selectedId_ == id) { selectedContext_ = context; updateSelectionActions(); }
            break;
        }
    };
    connect(apply, &QPushButton::clicked, dialog, [this, dialog, id, kind, numbers, choices, texts, updateDraftField] {
        QVariantMap saved = parameterDrafts_.value(id);
        for (auto it=numbers.cbegin(); it!=numbers.cend(); ++it) saved.insert(it.key(),it.value()->value());
        for (auto it=choices.cbegin(); it!=choices.cend(); ++it) saved.insert(it.key(),it.value()->currentIndex());
        for (auto it=texts.cbegin(); it!=texts.cend(); ++it) saved.insert(it.key(),it.value()->text());
        parameterDrafts_[id] = saved;
        updateDraftField();
        dialog->setProperty("draftApplied", true);
        emit postProcessCommandRequested(QStringLiteral("apply:") + kind);
        appendMessage(uiText(u"已提交应用请求；特征服务尚未接入，未生成结果。"));
    });
    connect(buttons, &QDialogButtonBox::rejected,dialog,&QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted,dialog,[this, dialog, id, name, numbers, choices, texts, updateDraftField] {
        QVariantMap saved = parameterDrafts_.value(id);
        for (auto it=numbers.cbegin();it!=numbers.cend();++it) saved.insert(it.key(),it.value()->value());
        for (auto it=choices.cbegin();it!=choices.cend();++it) saved.insert(it.key(),it.value()->currentIndex());
        for (auto it=texts.cbegin();it!=texts.cend();++it) saved.insert(it.key(),it.value()->text());
        parameterDrafts_[id]=saved;
        updateDraftField();
        appendMessage(uiText(u"已保存“%1”的参数草稿；尚未执行计算。").arg(name));
        dialog->accept();
    });
    dialog->show();
}

void PostProcessWorkbenchWindow::showProbePanel() {
    if (!probeDock_) {
        probeDock_ = new QDockWidget(uiText(u"探针结果"), this);
        probeDock_->setObjectName(QStringLiteral("postProcessProbeDock"));
        installDockTitle(probeDock_);
        probeResult_ = new QTextEdit(probeDock_);
        probeResult_->setObjectName(QStringLiteral("postProcessProbeResult"));
        probeResult_->setReadOnly(true);
        probeResult_->setPlainText(uiText(u"暂无取点结果。\n\n点击顶部“探针”启用取点工具，再在数据位置取点。当前 VTK 拾取服务尚未接入。"));
        probeResult_->setStyleSheet(centralWidget()->styleSheet());
        probeDock_->setWidget(probeResult_);
        addDockWidget(Qt::RightDockWidgetArea, probeDock_);
        resizeDocks({probeDock_}, {300}, Qt::Horizontal);
    }
    probeDock_->show(); probeDock_->raise();
}

void PostProcessWorkbenchWindow::displayProbeResult(const QString &text) {
    Q_ASSERT(QThread::currentThread() == thread());
    if (text.trimmed().isEmpty()) return;
    showProbePanel();
    probeResult_->setPlainText(text);
}

} // namespace aerosim
