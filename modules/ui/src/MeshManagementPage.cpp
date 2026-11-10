#include "aerosim/ui/MeshManagementPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QIcon>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>
#include <cstddef>

namespace aerosim {
namespace {
// Raises the page's update guard for the enclosing scope and restores the
// previous depth on exit, so nested refresh helpers never clear it early.
class EditorUpdateScope {
public:
    explicit EditorUpdateScope(int &depth) : depth_(depth) { ++depth_; }
    ~EditorUpdateScope() { --depth_; }
    EditorUpdateScope(const EditorUpdateScope &) = delete;
    EditorUpdateScope &operator=(const EditorUpdateScope &) = delete;
private:
    int &depth_;
};

template <std::size_t N>
QString uiText(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}

QToolButton *addViewButton(QHBoxLayout *layout, QWidget *parent, const QString &text,
                           const QString &toolTip, bool enabled = false)
{
    auto *button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(toolTip);
    button->setEnabled(enabled);
    button->setAutoRaise(true);
    layout->addWidget(button);
    return button;
}

// Colour axis triad drawn like the reference viewer's corner indicator: three
// labelled axes with ball ends rather than text arrows.
class AxisTriad final : public QWidget {
public:
    explicit AxisTriad(QWidget *parent = nullptr) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setToolTip(uiText(u"\u4e16\u754c\u5750\u6807\u7cfb\u6307\u793a"));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPointF origin(width() * 0.5, height() * 0.70);
        const qreal radius = qMin(width(), height()) * 0.40;

        struct Axis { QPointF dir; QColor color; QString label; };
        const Axis axes[3] = {
            {QPointF(0.0, -1.0), QColor(QStringLiteral("#e8d33c")), QStringLiteral("Z")},
            {QPointF(0.94, 0.34), QColor(QStringLiteral("#d93a34")), QStringLiteral("X")},
            {QPointF(-0.94, 0.34), QColor(QStringLiteral("#3aa34a")), QStringLiteral("Y")}};

        for (const auto &axis : axes) {
            const QPointF tip = origin + axis.dir * radius;
            painter.setPen(QPen(axis.color, 2.2, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(origin, tip);
            painter.setPen(Qt::NoPen);
            painter.setBrush(axis.color);
            painter.drawEllipse(tip, 4.2, 4.2);
            QFont labelFont = font();
            labelFont.setBold(true);
            labelFont.setPointSizeF(qMax(6.0, labelFont.pointSizeF() * 0.85));
            painter.setFont(labelFont);
            painter.setPen(axis.color.darker(120));
            const QPointF labelPos = origin + axis.dir * (radius + 9.0);
            painter.drawText(QRectF(labelPos.x() - 8, labelPos.y() - 8, 16, 16),
                             Qt::AlignCenter, axis.label);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#7d8797")));
        painter.drawEllipse(origin, 2.4, 2.4);
    }
};
}

MeshManagementPage::MeshManagementPage(QWidget *viewportHost, QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("viewportPage"));
    setStyleSheet(QStringLiteral(
        "#meshManagementHeader { background:#edf3fa; border-bottom:1px solid #c8d5e4; }"
        "#meshManagementTitle { color:#1f3b5b; font-size:15px; font-weight:600; }"
        "#meshInfoPanel { background:#ffffff; border:1px solid #cbd5df; }"
        "#meshInfoTitle { color:#1f4e9c; font-size:14px; font-weight:600; }"
        "#meshHistoryPanel { background:#ffffff; border:1px solid #cbd5df; }"
        "#meshHistoryHeader { background:#e8eef7; border-bottom:1px solid #c8d5e4; }"
        "#meshHistoryHeader QLabel { color:#1f3b5b; }"
        "QSplitter::handle { background:#dde5ee; }"
        "QSplitter::handle:horizontal { width:5px; }"
        "QSplitter::handle:vertical { height:5px; }"
        "QSplitter::handle:hover { background:#b9cde4; }"
        "#meshViewerPanel { background:#ffffff; border:1px solid #cbd5df; }"
        "#meshViewerToolbar { background:#f5f7fa; border-bottom:1px solid #d5dde6; }"
        "#meshCanvasBadge { color:#5d6b7a; background:rgba(255,255,255,150); "
        "border:1px solid #b8c8da; border-radius:3px; padding:2px 7px; margin:6px; }"
        "#meshCanvasOverlay { background:transparent; }"
        "#meshTable { border:1px solid #d5dde6; background:#ffffff; gridline-color:#e4eaf1; "
        "alternate-background-color:#f7f9fc; selection-background-color:#dceafd; selection-color:#25364c; }"
        "#meshTable QHeaderView::section { background:#eef3f9; color:#25364c; border:0; "
        "border-right:1px solid #d5dde6; border-bottom:1px solid #d5dde6; padding:4px 6px; }"
        "#meshCountLabel, #meshViewerStatus { color:#637083; }"
        "#meshViewerStatus { background:#f5f7fa; border-top:1px solid #d5dde6; }"
        // The reference tab bar is a blue-grey strip with the active tab in
        // white, and it sits flush above the viewport with no pane border.
        // The widget background fills the strip to the right of the last tab;
        // the pane repaints the content area white.
        "#meshWorkspaceTabs { background:#8ea6c0; }"
        "#meshWorkspaceTabs::pane { border:0; border-top:1px solid #9fb4cc; background:#ffffff; }"
        "#meshWorkspaceTabs QTabBar { background:#8ea6c0; }"
        "#meshWorkspaceTabs QTabBar::tab { background:#8ea6c0; color:#f2f6fb; "
        "padding:5px 22px; margin:0; border:1px solid #7b94af; border-bottom:none; }"
        "#meshWorkspaceTabs QTabBar::tab:hover:!selected { background:#9db4cc; color:#ffffff; }"
        "#meshWorkspaceTabs QTabBar::tab:selected { background:#ffffff; color:#1f3b5b; "
        "border:1px solid #9fb4cc; border-bottom:1px solid #ffffff; font-weight:600; }"
        "QGroupBox { font-weight:600; margin-top:8px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 3px; }"));

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(8, 8, 8, 8);
    pageLayout->setSpacing(6);

    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("meshManagementHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 7, 12, 7);
    auto *title = new QLabel(uiText(u"\u7f51\u683c\u7ba1\u7406"), header);
    title->setObjectName(QStringLiteral("meshManagementTitle"));
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    auto *scope = new QLabel(uiText(u"\u524d\u7aef Mock \u4ea4\u4e92 \u00b7 \u672a\u8fde\u63a5\u771f\u5b9e\u7f51\u683c\u6570\u636e"), header);
    scope->setObjectName(QStringLiteral("meshCountLabel"));
    headerLayout->addWidget(scope);
    pageLayout->addWidget(header);

    // Reference layout: the settings column stacks the mesh-information panel
    // above a log panel, and the right-hand tabbed workspace fills the rest.
    // Both dividers are user-draggable splitters.
    mainSplitter_ = new QSplitter(Qt::Horizontal, this);
    mainSplitter_->setObjectName(QStringLiteral("meshWorkspaceMainSplitter"));
    mainSplitter_->setChildrenCollapsible(false);
    settingsSplitter_ = new QSplitter(Qt::Vertical, mainSplitter_);
    settingsSplitter_->setObjectName(QStringLiteral("meshSettingsSplitter"));
    settingsSplitter_->setChildrenCollapsible(false);

    // Reference "网格信息" panel: coordinate system, mesh table, import/delete.
    auto *objectPanel = new QFrame(settingsSplitter_);
    objectPanel->setObjectName(QStringLiteral("meshInfoPanel"));
    objectPanel->setMinimumWidth(245);
    objectPanel->setMaximumWidth(430);
    auto *objectLayout = new QVBoxLayout(objectPanel);
    objectLayout->setContentsMargins(10, 8, 10, 10);
    objectLayout->setSpacing(7);

    auto *infoTitle = new QLabel(uiText(u"\u7f51\u683c\u4fe1\u606f"), objectPanel);
    infoTitle->setObjectName(QStringLiteral("meshInfoTitle"));
    infoTitle->setAlignment(Qt::AlignCenter);
    objectLayout->addWidget(infoTitle);

    auto *coordinateRow = new QWidget(objectPanel);
    auto *coordinateLayout = new QHBoxLayout(coordinateRow);
    coordinateLayout->setContentsMargins(0, 0, 0, 0);
    coordinateLayout->setSpacing(8);
    auto *coordinateLabel = new QLabel(uiText(u"\u7f51\u683c\u5750\u6807\u7cfb\uff1a"), coordinateRow);
    solidModelRadio_ = new QRadioButton(uiText(u"\u4e09\u7ef4\u6a21\u578b"), coordinateRow);
    solidModelRadio_->setObjectName(QStringLiteral("meshCoordinateSystemSolid"));
    solidModelRadio_->setChecked(true);
    solidModelRadio_->setToolTip(uiText(u"\u7f51\u683c\u6309\u4e09\u7ef4\u6a21\u578b\u8fdb\u884c\u8fb9\u754c\u63d0\u53d6\u4e0e\u663e\u793a"));
    planarModelRadio_ = new QRadioButton(uiText(u"\u4e8c\u7ef4\u6a21\u578b"), coordinateRow);
    planarModelRadio_->setObjectName(QStringLiteral("meshCoordinateSystemPlanar"));
    planarModelRadio_->setToolTip(uiText(u"\u4e8c\u7ef4\u6a21\u578b\u76ee\u524d\u4ec5\u4fdd\u7559\u9009\u9879\uff0c\u4e0d\u6267\u884c\u5e73\u9762\u63d0\u53d6"));
    coordinateLayout->addWidget(coordinateLabel);
    coordinateLayout->addWidget(solidModelRadio_);
    coordinateLayout->addWidget(planarModelRadio_);
    coordinateLayout->addStretch();
    objectLayout->addWidget(coordinateRow);

    meshTable_ = new QTableWidget(objectPanel);
    meshTable_->setObjectName(QStringLiteral("meshTable"));
    meshTable_->setColumnCount(3);
    meshTable_->setHorizontalHeaderLabels({uiText(u"\u5e8f\u53f7"), uiText(u"\u7f51\u683c\u540d\u79f0"),
                                           uiText(u"\u662f\u5426\u53ef\u89c1")});
    meshTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    meshTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    meshTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    meshTable_->verticalHeader()->hide();
    meshTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    meshTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    meshTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    meshTable_->setAlternatingRowColors(true);
    meshTable_->setMinimumHeight(118);
    objectLayout->addWidget(meshTable_, 1);

    auto *listActions = new QHBoxLayout;
    meshCount_ = new QLabel(objectPanel);
    meshCount_->setObjectName(QStringLiteral("meshObjectCount"));
    listActions->addWidget(meshCount_);
    listActions->addStretch();
    importMeshButton_ = new QPushButton(uiText(u"\u5bfc\u5165"), objectPanel);
    importMeshButton_->setObjectName(QStringLiteral("importMeshButton"));
    importMeshButton_->setToolTip(uiText(u"\u5bfc\u5165\u4e00\u4e2a\u524d\u7aef Mock \u7f51\u683c\uff0c\u6700\u591a 10 \u4e2a"));
    deleteMeshButton_ = new QPushButton(uiText(u"\u5220\u9664"), objectPanel);
    deleteMeshButton_->setObjectName(QStringLiteral("deleteMeshButton"));
    deleteMeshButton_->setToolTip(uiText(u"\u5220\u9664\u5f53\u524d\u9009\u4e2d\u7684\u7f51\u683c\u5bf9\u8c61"));
    listActions->addWidget(importMeshButton_);
    listActions->addWidget(deleteMeshButton_);
    objectLayout->addLayout(listActions);

    // The reference settings panel contains only 网格信息. Keep the display
    // properties available but collapsed by default so the panel matches that
    // compact proportion; expanding it exposes the per-object style editors.
    auto *properties = new QGroupBox(uiText(u"\u663e\u793a\u5c5e\u6027"), objectPanel);
    properties->setObjectName(QStringLiteral("meshDisplayProperties"));
    properties->setCheckable(true);
    properties->setChecked(false);
    properties->setToolTip(uiText(u"\u5c55\u5f00\u540e\u53ef\u7f16\u8f91\u5f53\u524d\u7f51\u683c\u7684\u663e\u793a\u5c5e\u6027"));
    auto *form = new QFormLayout(properties);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    nameEditor_ = new QLineEdit(properties);
    nameEditor_->setObjectName(QStringLiteral("meshNameEditor"));
    visibleEditor_ = new QCheckBox(uiText(u"\u53ef\u89c1\uff08Mock\uff09"), properties);
    visibleEditor_->setObjectName(QStringLiteral("meshVisibleEditor"));
    displayModeEditor_ = new QComboBox(properties);
    displayModeEditor_->setObjectName(QStringLiteral("meshDisplayModeEditor"));
    displayModeEditor_->addItems({uiText(u"\u8868\u9762"), uiText(u"\u7ebf\u6846"), uiText(u"\u8868\u9762 + \u8fb9\u7ebf")});
    colorButton_ = new QPushButton(properties);
    colorButton_->setObjectName(QStringLiteral("meshColorButton"));
    colorButton_->setToolTip(uiText(u"\u70b9\u51fb\u5faa\u73af\u5207\u6362 Mock \u663e\u793a\u989c\u8272"));
    auto *opacityRow = new QWidget(properties);
    auto *opacityLayout = new QHBoxLayout(opacityRow);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    opacityEditor_ = new QSlider(Qt::Horizontal, opacityRow);
    opacityEditor_->setObjectName(QStringLiteral("meshOpacityEditor"));
    opacityEditor_->setRange(0, 100);
    opacityValue_ = new QLabel(opacityRow);
    opacityValue_->setMinimumWidth(36);
    opacityLayout->addWidget(opacityEditor_, 1);
    opacityLayout->addWidget(opacityValue_);
    scaleEditor_ = new QDoubleSpinBox(properties);
    scaleEditor_->setObjectName(QStringLiteral("meshScaleEditor"));
    scaleEditor_->setRange(0.001, 1000.0);
    scaleEditor_->setDecimals(3);
    scaleEditor_->setSingleStep(0.1);
    unitEditor_ = new QComboBox(properties);
    unitEditor_->setObjectName(QStringLiteral("meshUnitEditor"));
    unitEditor_->addItems({QStringLiteral("m"), QStringLiteral("mm"), QStringLiteral("cm"), QStringLiteral("in")});
    form->addRow(uiText(u"\u7f51\u683c\u540d\u79f0"), nameEditor_);
    form->addRow(uiText(u"\u53ef\u89c1\u6027"), visibleEditor_);
    form->addRow(uiText(u"\u663e\u793a\u6a21\u5f0f"), displayModeEditor_);
    form->addRow(uiText(u"\u663e\u793a\u989c\u8272"), colorButton_);
    form->addRow(uiText(u"\u900f\u660e\u5ea6"), opacityRow);
    form->addRow(uiText(u"\u6bd4\u4f8b\u56e0\u5b50"), scaleEditor_);
    form->addRow(uiText(u"\u5355\u4f4d\u6807\u8bb0"), unitEditor_);
    objectLayout->addWidget(properties);
    // Checkable group boxes hide only their own children; apply the same
    // collapse to the scroll wrapper so the panel really shrinks.
    auto *propertyScroll = new QScrollArea(objectPanel);
    propertyScroll->setObjectName(QStringLiteral("meshPropertyScroll"));
    propertyScroll->setWidgetResizable(true);
    propertyScroll->setFrameShape(QFrame::NoFrame);
    propertyScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    propertyScroll->setMinimumHeight(120);
    propertyScroll->setWidget(properties);
    objectLayout->addWidget(propertyScroll, 2);
    propertyScroll->setVisible(properties->isChecked());

    // Reference tab strip: 网格视图 / 单工况 / 后处理.
    workspaceTabs_ = new QTabWidget(mainSplitter_);
    workspaceTabs_->setObjectName(QStringLiteral("meshWorkspaceTabs"));
    workspaceTabs_->setDocumentMode(false);

    auto *viewerPanel = new QFrame(workspaceTabs_);
    viewerPanel->setObjectName(QStringLiteral("meshViewerPanel"));
    auto *viewerLayout = new QVBoxLayout(viewerPanel);
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    viewerLayout->setSpacing(0);
    auto *toolbar = new QWidget(viewerPanel);
    toolbar->setObjectName(QStringLiteral("meshViewerToolbar"));
    auto *toolbarLayout = new QVBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 3, 8, 3);
    toolbarLayout->setSpacing(1);
    auto *cameraTools = new QHBoxLayout;
    cameraTools->setSpacing(2);
    const QString pending = uiText(u"\u7b49\u5f85 VTKViewer \u89c6\u56fe\u63a7\u5236\u63a5\u53e3\u63a5\u5165");
    addViewButton(cameraTools, toolbar, uiText(u"\u9002\u5e94"), pending);
    addViewButton(cameraTools, toolbar, uiText(u"\u653e\u5927"), pending);
    addViewButton(cameraTools, toolbar, uiText(u"\u7f29\u5c0f"), pending);
    addViewButton(cameraTools, toolbar, uiText(u"\u91cd\u7f6e"), pending);
    cameraTools->addSpacing(6);
    addViewButton(cameraTools, toolbar, uiText(u"\u6b63\u89c6 X"), pending);
    addViewButton(cameraTools, toolbar, uiText(u"\u4fa7\u89c6 Y"), pending);
    addViewButton(cameraTools, toolbar, uiText(u"\u9876\u89c6 Z"), pending);
    const QStringList cameraCommands = {"fit", "in", "out", "reset", "front", "right", "top"};
    const auto cameraButtons = toolbar->findChildren<QToolButton *>();
    for (int i=0; i<cameraCommands.size(); ++i) {
        auto *button=cameraButtons.at(i);
        button->setProperty("boundaryCommand", cameraCommands.at(i));
        button->setObjectName(QStringLiteral("boundaryView_%1").arg(cameraCommands.at(i)));
        connect(button, &QToolButton::clicked, this, [this, i, cameraCommands] {
            if (boundaryMode_) emit boundaryViewRequested(cameraCommands.at(i));
        });
    }
    cameraTools->addStretch();
    toolbarLayout->addLayout(cameraTools);
    auto *displayTools = new QHBoxLayout;
    displayTools->setSpacing(2);
    auto *surfaceButton = addViewButton(displayTools, toolbar, uiText(u"\u8868\u9762"),
        uiText(u"\u4ec5\u66f4\u65b0\u5f53\u524d Mock \u5bf9\u8c61\u7684\u663e\u793a\u6a21\u5f0f"), true);
    auto *wireframeButton = addViewButton(displayTools, toolbar, uiText(u"\u7ebf\u6846"),
        uiText(u"\u4ec5\u66f4\u65b0\u5f53\u524d Mock \u5bf9\u8c61\u7684\u663e\u793a\u6a21\u5f0f"), true);
    auto *axisButton = addViewButton(displayTools, toolbar, uiText(u"\u5750\u6807\u8f74"),
        uiText(u"\u663e\u793a\u6216\u9690\u85cf\u754c\u9762\u5750\u6807\u6307\u793a"), true);
    axisButton->setCheckable(true);
    axisButton->setChecked(true);
    addViewButton(displayTools, toolbar, uiText(u"\u65cb\u8f6c\u4e2d\u5fc3"), pending);
    displayTools->addStretch();
    auto *background = new QComboBox(toolbar);
    background->setObjectName(QStringLiteral("meshBackgroundMode"));
    background->addItems({uiText(u"\u6d45\u8272\u80cc\u666f"), uiText(u"\u6df1\u8272\u80cc\u666f"), uiText(u"\u53cc\u8272\u6e10\u53d8\uff08\u9884\u7559\uff09")});
    background->setToolTip(uiText(u"\u8c03\u6574\u4e09\u7ef4\u89c6\u53e3\u80cc\u666f\uff1b\u672a\u63a5\u5165 VTK \u65f6\u4ec5\u5207\u6362\u5360\u4f4d\u5e95\u8272"));
    displayTools->addWidget(background);
    toolbarLayout->addLayout(displayTools);
    viewerLayout->addWidget(toolbar);

    // Reference canvas: a light blue diagonal gradient with a colour axis
    // triad bottom-left and a small viewer badge bottom-right.
    auto *canvas = new QFrame(viewerPanel);
    canvas->setObjectName(QStringLiteral("meshViewerCanvas"));
    canvas->setStyleSheet(QStringLiteral(
        "#meshViewerCanvas { background:qlineargradient(x1:0,y1:0,x2:0.62,y2:1,"
        "stop:0 #c9d2f2, stop:0.45 #ccd8f5, stop:1 #e8eefb); }"));
    auto *canvasLayout = new QGridLayout(canvas);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    viewportHost->setObjectName(QStringLiteral("viewportSlot"));
    canvasLayout->addWidget(viewportHost, 0, 0);

    // The injected VTK widget paints its own opaque background, so the axis
    // triad and badge are raised above it instead of relying on the canvas
    // gradient showing through.
    auto *overlay = new QWidget(canvas);
    overlay->setObjectName(QStringLiteral("meshCanvasOverlay"));
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *overlayLayout = new QGridLayout(overlay);
    overlayLayout->setContentsMargins(10, 10, 10, 10);

    axisIndicator_ = new AxisTriad(overlay);
    axisIndicator_->setObjectName(QStringLiteral("axisIndicator"));
    axisIndicator_->setFixedSize(96, 92);
    overlayLayout->addWidget(axisIndicator_, 0, 0, Qt::AlignLeft | Qt::AlignBottom);

    auto *canvasBadge = new QLabel(uiText(u"\u25c9  195/1205MB"), overlay);
    canvasBadge->setObjectName(QStringLiteral("meshCanvasBadge"));
    canvasBadge->setAlignment(Qt::AlignCenter);
    canvasBadge->setToolTip(uiText(u"\u89c6\u53e3\u72b6\u6001\u793a\u610f\uff1b\u672a\u63a5\u5165\u771f\u5b9e\u6e32\u67d3\u8ba1\u91cf"));
    overlayLayout->addWidget(canvasBadge, 0, 0, Qt::AlignRight | Qt::AlignBottom);
    canvasLayout->addWidget(overlay, 0, 0);
    overlay->raise();

    viewerLayout->addWidget(canvas, 1);
    // Keep the status line as the reference-style slim footer strip.
    viewerStatus_ = new QLabel(uiText(u"VTKViewer \u63d2\u69fd\u5df2\u4fdd\u7559 \u00b7 \u5f53\u524d\u4ec5\u663e\u793a\u524d\u7aef Mock \u72b6\u6001"), viewerPanel);
    viewerStatus_->setObjectName(QStringLiteral("meshViewerStatus"));
    viewerStatus_->setContentsMargins(10, 4, 10, 4);
    viewerLayout->addWidget(viewerStatus_);
    workspaceTabs_->addTab(viewerPanel, uiText(u"\u7f51\u683c\u89c6\u56fe"));

    const auto createReservedTab = [this](const QString &heading, const QString &note) {
        auto *page = new QWidget(workspaceTabs_);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(24, 20, 24, 20);
        auto *titleLabel = new QLabel(heading, page);
        QFont headingFont = titleLabel->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        titleLabel->setFont(headingFont);
        layout->addWidget(titleLabel);
        auto *noteLabel = new QLabel(note, page);
        noteLabel->setWordWrap(true);
        layout->addWidget(noteLabel);
        layout->addStretch();
        return page;
    };
    workspaceTabs_->addTab(createReservedTab(
        uiText(u"\u5355\u5de5\u51b5"),
        uiText(u"\u5355\u5de5\u51b5\u89c6\u56fe\u7528\u4e8e\u663e\u793a\u4e00\u4e2a\u8ba1\u7b97\u5de5\u51b5\u7684\u53c2\u6570\u4e0e\u7ed3\u679c\u3002"
                  u"\u5f53\u524d\u5de5\u4f5c\u533a\u6ca1\u6709\u53ef\u5c55\u793a\u7684\u771f\u5b9e\u5de5\u51b5\u6570\u636e\uff0c\u8be5\u9875\u4ec5\u4fdd\u7559\u524d\u7aef\u5165\u53e3\u3002")),
        uiText(u"\u5355\u5de5\u51b5"));
    auto *postprocessTab = createReservedTab(
        uiText(u"\u540e\u5904\u7406"),
        uiText(u"\u540e\u5904\u7406\u5206\u6790\u5728\u72ec\u7acb\u7684\u540e\u5904\u7406\u5de5\u4f5c\u53f0\u4e2d\u5b8c\u6210\u3002"
                  u"\u53ef\u4ece\u6b64\u5904\u6253\u5f00\u5de5\u4f5c\u53f0\uff1b\u5f53\u524d\u9875\u4e0d\u521b\u5efa\u7b2c\u4e8c\u5957\u540e\u5904\u7406\u89c6\u56fe\u3002"));
    auto *postprocessLayout = qobject_cast<QVBoxLayout *>(postprocessTab->layout());
    auto *openPostprocess = new QPushButton(uiText(u"\u6253\u5f00\u540e\u5904\u7406\u5de5\u4f5c\u53f0"), postprocessTab);
    openPostprocess->setObjectName(QStringLiteral("meshOpenPostprocessButton"));
    openPostprocess->setToolTip(uiText(u"\u6253\u5f00\u72ec\u7acb\u7684\u540e\u5904\u7406\u5de5\u4f5c\u53f0\u7a97\u53e3"));
    postprocessLayout->addWidget(openPostprocess, 0, Qt::AlignLeft);
    connect(openPostprocess, &QPushButton::clicked, this, &MeshManagementPage::postProcessRequested);
    workspaceTabs_->addTab(postprocessTab, uiText(u"\u540e\u5904\u7406"));

    settingsSplitter_->addWidget(objectPanel);
    settingsSplitter_->setStretchFactor(0, 3);

    // Reference "日志" panel sits directly below the settings panel.
    auto *historyPanel = new QFrame(settingsSplitter_);
    historyPanel->setObjectName(QStringLiteral("meshHistoryPanel"));
    auto *historyLayout = new QVBoxLayout(historyPanel);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    historyLayout->setSpacing(0);
    historyHeader_ = new QWidget(historyPanel);
    historyHeader_->setObjectName(QStringLiteral("meshHistoryHeader"));
    auto *historyHeaderLayout = new QHBoxLayout(historyHeader_);
    historyHeaderLayout->setContentsMargins(9, 3, 7, 3);
    auto *historyTitle = new QLabel(uiText(u"\u65e5\u5fd7"), historyHeader_);
    QFont historyTitleFont = historyTitle->font();
    historyTitleFont.setBold(true);
    historyTitle->setFont(historyTitleFont);
    historyHeaderLayout->addWidget(historyTitle);
    historyHeaderLayout->addStretch();
    historyCollapseButton_ = new QPushButton(uiText(u"\u6536\u8d77"), historyHeader_);
    historyCollapseButton_->setObjectName(QStringLiteral("meshHistoryCollapseButton"));
    historyCollapseButton_->setMaximumWidth(58);
    historyHeaderLayout->addWidget(historyCollapseButton_);
    historyLayout->addWidget(historyHeader_);
    historyTabs_ = new QTabWidget(historyPanel);
    historyTabs_->setObjectName(QStringLiteral("meshHistoryTabs"));
    historyList_ = new QListWidget(historyTabs_);
    historyList_->setObjectName(QStringLiteral("meshOperationHistory"));
    historyList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // The reference log uses the regular UI font and wraps its lines instead
    // of scrolling horizontally, so long extraction summaries stay readable at
    // the narrow settings-column width.
    historyList_->setWordWrap(true);
    historyList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    historyList_->setTextElideMode(Qt::ElideNone);
    historyTabs_->addTab(historyList_, uiText(u"\u64cd\u4f5c\u8bb0\u5f55"));
    auto *scriptPage = new QWidget(historyTabs_);
    auto *scriptLayout = new QVBoxLayout(scriptPage);
    auto *scriptNote = new QLabel(uiText(u"\u811a\u672c\u63a7\u5236\u4e3a\u540e\u7eed\u7248\u672c\u9884\u7559\uff1b\u672c\u671f\u4e0d\u542f\u52a8 Python \u89e3\u91ca\u5668\u6216\u5916\u90e8\u8fdb\u7a0b\u3002"), scriptPage);
    scriptNote->setWordWrap(true);
    auto *scriptInput = new QPlainTextEdit(scriptPage);
    scriptInput->setObjectName(QStringLiteral("meshScriptPlaceholder"));
    scriptInput->setPlaceholderText(uiText(u"# \u540e\u7eed\u7248\u672c\u652f\u6301\u7f51\u683c\u811a\u672c\u63a7\u5236"));
    scriptInput->setEnabled(false);
    scriptLayout->addWidget(scriptNote);
    scriptLayout->addWidget(scriptInput, 1);
    historyTabs_->addTab(scriptPage, uiText(u"\u811a\u672c\u63a7\u5236\uff08\u9884\u7559\uff09"));
    historyLayout->addWidget(historyTabs_, 1);
    settingsSplitter_->addWidget(historyPanel);
    settingsSplitter_->setStretchFactor(1, 2);
    settingsSplitter_->setSizes({470, 180});

    mainSplitter_->addWidget(settingsSplitter_);
    mainSplitter_->addWidget(workspaceTabs_);
    mainSplitter_->setStretchFactor(0, 0);
    mainSplitter_->setStretchFactor(1, 1);
    mainSplitter_->setSizes({330, 810});
    pageLayout->addWidget(mainSplitter_, 1);

    connect(importMeshButton_, &QPushButton::clicked, this, &MeshManagementPage::addMesh);
    connect(deleteMeshButton_, &QPushButton::clicked, this, &MeshManagementPage::removeCurrentMesh);
    connect(properties, &QGroupBox::toggled, this, [propertyScroll](bool expanded) {
        propertyScroll->setVisible(expanded);
    });
    connect(meshTable_, &QTableWidget::currentCellChanged, this,
        [this](int row, int, int, int) { selectMesh(row); });
    connect(meshTable_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (isUpdatingEditor() || !item || item->column() != 2) return;
        const int row = item->row();
        if (row < 0 || row >= meshes_.size()) return;
        meshes_[row].visible = item->checkState() == Qt::Checked;
        emitBoundaryStyle(row);
        if (row == selectedRow()) refreshEditor();
        appendHistory((meshes_[row].visible ? uiText(u"\u663e\u793a ") : uiText(u"\u9690\u85c5 ")) + meshes_[row].name);
    });
    connect(nameEditor_, &QLineEdit::editingFinished, this, &MeshManagementPage::updateCurrentMesh);
    connect(visibleEditor_, &QCheckBox::toggled, this, &MeshManagementPage::updateCurrentMesh);
    connect(displayModeEditor_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshManagementPage::updateCurrentMesh);
    connect(opacityEditor_, &QSlider::valueChanged, this, &MeshManagementPage::updateCurrentMesh);
    connect(scaleEditor_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MeshManagementPage::updateCurrentMesh);
    connect(unitEditor_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshManagementPage::updateCurrentMesh);
    connect(colorButton_, &QPushButton::clicked, this, [this] {
        const int row = selectedRow();
        if (row < 0 || row >= meshes_.size()) return;
        const QVector<QColor> colors = {QColor("#3b82c4"), QColor("#e58b2a"), QColor("#4d9d69"), QColor("#8c63b8")};
        int index = colors.indexOf(meshes_[row].color);
        meshes_[row].color = colors[(index + 1) % colors.size()];
        emitBoundaryStyle(row);
        refreshEditor();
        appendHistory(uiText(u"\u4fee\u6539 %1 \u663e\u793a\u989c\u8272\uff08Mock\uff09").arg(meshes_[row].name));
    });
    connect(surfaceButton, &QToolButton::clicked, this, [this] {
        displayModeEditor_->setCurrentText(uiText(u"\u8868\u9762"));
    });
    connect(wireframeButton, &QToolButton::clicked, this, [this] {
        displayModeEditor_->setCurrentText(uiText(u"\u7ebf\u6846"));
    });
    axisButton->setObjectName(QStringLiteral("meshAxisButton"));
    connect(axisButton, &QToolButton::toggled, axisIndicator_, &QWidget::setVisible);
    connect(background, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, canvas, background](int index) {
        if (boundaryMode_) { emit boundaryBackgroundRequested(index); return; }
        // Keep the reference gradient as the default light viewport.
        const QString color = index == 1
            ? QStringLiteral("qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #2f3b49,stop:1 #4a5a6d)")
            : index == 2
            ? QStringLiteral("qlineargradient(x1:0,y1:0,x2:0.62,y2:1,stop:0 #dce9f7,stop:1 #6f849b)")
            : QStringLiteral("qlineargradient(x1:0,y1:0,x2:0.62,y2:1,stop:0 #c9d2f2,stop:0.45 #ccd8f5,stop:1 #e8eefb)");
        canvas->setStyleSheet(QStringLiteral("#meshViewerCanvas { background:%1; }").arg(color));
        viewerStatus_->setText(uiText(u"\u5360\u4f4d\u80cc\u666f\uff1a%1 \u00b7 \u672a\u8c03\u7528 VTK \u6e32\u67d3").arg(background->currentText()));
        appendHistory(uiText(u"\u5207\u6362\u89c6\u53e3\u5360\u4f4d\u80cc\u666f\uff1a%1").arg(background->currentText()));
    });
    connect(historyCollapseButton_, &QPushButton::clicked, this,
        [this] { setHistoryCollapsed(!historyCollapsed_); });

    const QVector<QColor> colors = {QColor("#3b82c4"), QColor("#e58b2a"), QColor("#4d9d69")};
    for (int i = 0; i < 3; ++i) {
        MeshUiState state;
        state.name = QStringLiteral("Mesh_%1").arg(i + 1, 3, 10, QLatin1Char('0'));
        state.visible = i != 2;
        state.displayMode = i == 1 ? uiText(u"\u7ebf\u6846") : uiText(u"\u8868\u9762");
        state.color = colors[i];
        state.opacity = i == 2 ? 65 : 100;
        state.scale = 1.0;
        state.unit = QStringLiteral("m");
        meshes_.append(state);
        meshTable_->insertRow(meshTable_->rowCount());
        {
            // Assigning a checkable item emits itemChanged while its check
            // state is still unset; without this guard that would be read as
            // the user hiding the mesh.
            EditorUpdateScope updateGuard(editorUpdateDepth_);
            auto *indexItem = new QTableWidgetItem;
            indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
            auto *nameItem = new QTableWidgetItem;
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
            auto *visibleItem = new QTableWidgetItem;
            visibleItem->setFlags(visibleItem->flags() | Qt::ItemIsUserCheckable);
            visibleItem->setTextAlignment(Qt::AlignCenter);
            meshTable_->setItem(i, 0, indexItem);
            meshTable_->setItem(i, 1, nameItem);
            meshTable_->setItem(i, 2, visibleItem);
            
        }
        refreshTableRow(i);
    }
    updateCount();
    appendHistory(uiText(u"\u521d\u59cb\u5316 3 \u4e2a Mock \u7f51\u683c\u5bf9\u8c61"));
    meshTable_->setCurrentCell(0, 1);
    setHistoryCollapsed(false);
}

void MeshManagementPage::addMesh()
{
    if (boundaryMode_) {
        const auto path=QFileDialog::getOpenFileName(this, QString::fromUtf8("导入网格边界"), QString(),
            QString::fromUtf8("VTK 网格 (*.vtk *.vtu *.vti *.vtp)"));
        if (!path.isEmpty()) emit boundaryImportRequested(path);
        return;
    }
    if (meshes_.size() >= 10) return;
    MeshUiState state;
    state.name = QStringLiteral("Mesh_%1").arg(meshes_.size() + 1, 3, 10, QLatin1Char('0'));
    state.displayMode = uiText(u"\u8868\u9762");
    state.color = QColor("#3b82c4");
    state.unit = QStringLiteral("m");
    meshes_.append(state);
    const int row = meshTable_->rowCount();
    meshTable_->insertRow(row);
    {
        // Building a checkable cell emits itemChanged before its initial check
        // state is assigned. Do not interpret that as a user hiding the mesh.
        EditorUpdateScope updateGuard(editorUpdateDepth_);
        auto *indexItem = new QTableWidgetItem;
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        auto *nameItem = new QTableWidgetItem;
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        auto *visibleItem = new QTableWidgetItem;
        visibleItem->setFlags(visibleItem->flags() | Qt::ItemIsUserCheckable);
        visibleItem->setTextAlignment(Qt::AlignCenter);
        meshTable_->setItem(row, 0, indexItem);
        meshTable_->setItem(row, 1, nameItem);
        meshTable_->setItem(row, 2, visibleItem);
        
    }
    refreshTableRow(row);
    meshTable_->setCurrentCell(row, 1);
    appendHistory(uiText(u"\u6dfb\u52a0\u7f51\u683c %1\uff08Mock\uff09").arg(state.name));
    updateCount();
}

void MeshManagementPage::removeCurrentMesh()
{
    const int row = selectedRow();
    if (row < 0 || row >= meshes_.size()) return;
    const QString name = meshes_[row].name;
    if (boundaryMode_) emit boundaryRemoveRequested(meshes_[row].id);
    meshes_.remove(row);
    meshTable_->removeRow(row);
    for (int index = 0; index < meshes_.size(); ++index) refreshTableRow(index);
    if (!meshes_.isEmpty()) meshTable_->setCurrentCell(qMin(row, meshes_.size() - 1), 1);
    else refreshEditor();
    appendHistory(uiText(u"\u79fb\u9664\u7f51\u683c %1\uff08\u4ec5\u5f71\u54cd\u5f53\u524d\u754c\u9762\uff09").arg(name));
    updateCount();
}

void MeshManagementPage::selectMesh(int row)
{
    if (row < 0 || row >= meshes_.size()) return;
    refreshEditor();
    appendHistory(uiText(u"\u9009\u62e9\u7f51\u683c %1").arg(meshes_[row].name));
}

void MeshManagementPage::refreshTableRow(int row)
{
    if (row < 0 || row >= meshes_.size()) return;
    auto *indexItem = meshTable_->item(row, 0);
    auto *nameItem = meshTable_->item(row, 1);
    auto *visibleItem = meshTable_->item(row, 2);
    if (!indexItem || !nameItem || !visibleItem) return;
    EditorUpdateScope updateGuard(editorUpdateDepth_);
    indexItem->setText(QString::number(row + 1));
    indexItem->setTextAlignment(Qt::AlignCenter);
    nameItem->setText(meshes_[row].name + (meshes_[row].ready ? QString() : QString::fromUtf8(" · 加载中")));
    if (!meshes_[row].id.isEmpty()) nameItem->setData(Qt::UserRole, meshes_[row].id);
    visibleItem->setCheckState(meshes_[row].visible ? Qt::Checked : Qt::Unchecked);
    visibleItem->setTextAlignment(Qt::AlignCenter);
    
}

void MeshManagementPage::refreshEditor()
{
    const int row = selectedRow();
    const bool valid = row >= 0 && row < meshes_.size();
    const QVector<QWidget *> editors = {nameEditor_, visibleEditor_, displayModeEditor_, colorButton_,
                                        opacityEditor_, scaleEditor_, unitEditor_};
    for (auto *widget : editors) widget->setEnabled(valid && meshes_[row].ready);
    deleteMeshButton_->setEnabled(valid);
    EditorUpdateScope updateGuard(editorUpdateDepth_);
    if (valid) {
        const auto &state = meshes_[row];
        nameEditor_->setText(state.name);
        visibleEditor_->setChecked(state.visible);
        displayModeEditor_->setCurrentText(state.displayMode);
        opacityEditor_->setValue(state.opacity);
        opacityValue_->setText(QStringLiteral("%1%").arg(state.opacity));
        scaleEditor_->setValue(state.scale);
        unitEditor_->setCurrentText(state.unit);
        colorButton_->setText(state.color.name().toUpper());
        colorButton_->setStyleSheet(QStringLiteral("QPushButton { border-left:16px solid %1; }").arg(state.color.name()));
        viewerStatus_->setText(uiText(u"\u5f53\u524d\u7f16\u8f91\uff1a%1 · %2 · 前端 Mock 状态")
            .arg(state.name, state.visible ? uiText(u"\u53ef\u89c1") : uiText(u"\u9690\u85c5")));
    } else {
        nameEditor_->clear();
        opacityValue_->clear();
        colorButton_->setText(uiText(u"\u65e0\u9009\u62e9"));
        viewerStatus_->setText(uiText(u"\u5c1a\u672a\u9009\u62e9\u7f51\u683c\u5bf9\u8c61 · VTKViewer \u63d2\u69fd\u4fdd\u6301\u4e0d\u53d8"));
    }
    if (boundaryMode_) viewerStatus_->setText(valid ? meshes_[row].summary
        : QString::fromUtf8("请选择网格文件以显示边界。"));
    
}

void MeshManagementPage::updateCurrentMesh()
{
    if (isUpdatingEditor()) return;
    const int row = selectedRow();
    if (row < 0 || row >= meshes_.size()) return;
    auto &state = meshes_[row];
    const QString oldName = state.name;
    const QString requestedName = nameEditor_->text().trimmed();
    state.name = requestedName.isEmpty() ? oldName : requestedName;
    state.visible = visibleEditor_->isChecked();
    state.displayMode = displayModeEditor_->currentText();
    state.opacity = opacityEditor_->value();
    state.scale = scaleEditor_->value();
    state.unit = unitEditor_->currentText();
    emitBoundaryStyle(row);
    refreshTableRow(row);
    refreshEditor();
    appendHistory(uiText(u"\u66f4\u65b0 %1 \u663e\u793a\u5c5e\u6027\uff08Mock\uff09").arg(state.name));
}

void MeshManagementPage::updateCount()
{
    if (boundaryMode_) {
        meshCount_->setText(QString::fromUtf8("%1 个").arg(meshes_.size()));
        importMeshButton_->setEnabled(!boundaryBusy_);
        importMeshButton_->setToolTip(QString::fromUtf8("读取网格并提取全部拓扑边界面"));
        return;
    }
    meshCount_->setText(uiText(u"%1 / 10 个").arg(meshes_.size()));
    importMeshButton_->setEnabled(meshes_.size() < 10);
    importMeshButton_->setToolTip(meshes_.size() < 10
        ? uiText(u"\u6dfb\u52a0\u4e00\u4e2a\u524d\u7aef Mock \u7f51\u683c\uff0c\u6700\u591a 10 \u4e2a")
        : uiText(u"\u5df2\u8fbe\u5230 10 \u4e2a\u7f51\u683c\u5bf9\u8c61\u7684\u754c\u9762\u4e0a\u9650"));
}

void MeshManagementPage::appendHistory(const QString &message)
{
    QString displayMessage=message;
    if (boundaryMode_) {
        displayMessage.remove(QString::fromUtf8("（Mock）"));
        displayMessage.remove(QString::fromUtf8("（仅影响当前界面）"));
    }
    historyList_->addItem(QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), displayMessage));
    historyList_->scrollToBottom();
}

void MeshManagementPage::setHistoryCollapsed(bool collapsed)
{
    historyCollapsed_ = collapsed;
    if (collapsed) {
        const QList<int> sizes = settingsSplitter_->sizes();
        if (sizes.size() == 2 && sizes[1] > 60) expandedHistoryHeight_ = sizes[1];
        historyTabs_->hide();
        historyCollapseButton_->setText(uiText(u"\u5c55\u5f00"));
        const int headerHeight = historyHeader_->sizeHint().height() + 2;
        settingsSplitter_->setSizes({qMax(200, height()), headerHeight});
    } else {
        historyTabs_->show();
        historyCollapseButton_->setText(uiText(u"\u6536\u8d77"));
        settingsSplitter_->setSizes({qMax(220, height() - expandedHistoryHeight_), expandedHistoryHeight_});
    }
}

QVariantMap MeshManagementPage::layoutState() const
{
    const QList<int> horizontal = mainSplitter_->sizes();
    const int horizontalTotal = horizontal.value(0) + horizontal.value(1);
    const QList<int> vertical = settingsSplitter_->sizes();
    const int verticalTotal = vertical.value(0) + vertical.value(1);
    QVariantMap state;
    state.insert(QStringLiteral("objectPanelRatio"), horizontalTotal > 0
        ? double(horizontal.value(0)) / horizontalTotal : 0.29);
    state.insert(QStringLiteral("historyRatio"), verticalTotal > 0 && !historyCollapsed_
        ? double(vertical.value(1)) / verticalTotal
        : double(expandedHistoryHeight_) / qMax(1, height()));
    state.insert(QStringLiteral("historyCollapsed"), historyCollapsed_);
    return state;
}

void MeshManagementPage::restoreLayoutState(const QVariantMap &state)
{
    const double objectRatio = qBound(0.18, state.value(QStringLiteral("objectPanelRatio"), 0.29).toDouble(), 0.45);
    const double historyRatio = qBound(0.10, state.value(QStringLiteral("historyRatio"), 0.24).toDouble(), 0.55);
    mainSplitter_->setSizes({qRound(objectRatio * 1000), qRound((1.0 - objectRatio) * 1000)});
    expandedHistoryHeight_ = qBound(90, qRound(qMax(450, height()) * historyRatio), qMax(120, height() / 2));
    setHistoryCollapsed(state.value(QStringLiteral("historyCollapsed"), false).toBool());
}

void MeshManagementPage::restoreDefaultLayout()
{
    mainSplitter_->setSizes({330, 810});
    expandedHistoryHeight_ = 150;
    setHistoryCollapsed(false);
}

void MeshManagementPage::enableBoundaryMode()
{
    if (boundaryMode_) return;
    boundaryMode_=true;
    {
        QSignalBlocker block(meshTable_);
        meshTable_->setRowCount(0);
        meshes_.clear();
    }
    historyList_->clear();
    meshCount_->setText(QString::fromUtf8("B-Rep · 网格边界显示"));
    auto *displayGroup = findChild<QGroupBox *>("meshDisplayProperties");
    displayGroup->setTitle(QString::fromUtf8("显示属性"));
    displayGroup->setChecked(true);
    findChild<QScrollArea *>("meshPropertyScroll")->setVisible(true);
    visibleEditor_->setText(QString::fromUtf8("可见"));
    colorButton_->setToolTip(QString::fromUtf8("切换当前对象颜色"));
    auto *opacityRow=opacityEditor_->parentWidget();
    if (auto *form=qobject_cast<QFormLayout *>(opacityRow->parentWidget()->layout())) {
        if (auto *label=qobject_cast<QLabel *>(form->labelForField(opacityRow)))
            label->setText(QString::fromUtf8("不透明度"));
    }
    scaleEditor_->setToolTip(QString::fromUtf8("仅缩放显示，不修改原始网格坐标"));
    unitEditor_->setToolTip(QString::fromUtf8("单位标记，不执行坐标或物理量换算"));
    // The reference viewer keeps its colour axis triad visible in boundary
    // mode, so only gate the toolbar toggle rather than hiding the triad.
    findChild<QToolButton *>("meshAxisButton")->setEnabled(true);
    for (auto *button:findChildren<QToolButton *>()) {
        if (button->toolTip().contains(QStringLiteral("Mock")))
            button->setToolTip(QString::fromUtf8("修改当前对象的显示模式"));
        const auto cmd=button->property("boundaryCommand").toString();
        if (!cmd.isEmpty()) {button->setEnabled(true);button->setToolTip(QString::fromUtf8("调整三维视图"));}
        if(cmd=="front") button->setText(QString::fromUtf8("正视（−Y）"));
        if(cmd=="right") button->setText(QString::fromUtf8("侧视（+X）"));
    }
    auto *background=findChild<QComboBox *>("meshBackgroundMode");
    background->setItemText(2,QString::fromUtf8("渐变背景")); background->setToolTip(QString::fromUtf8("设置三维视口背景"));
    emit boundaryBackgroundRequested(background->currentIndex());
    viewerStatus_->setWordWrap(true);
    refreshEditor(); updateCount();
    viewerStatus_->setText(QString::fromUtf8("请选择 VTK 网格文件；显示全部拓扑边界，原体数据保留用于分析。"));
}
void MeshManagementPage::beginBoundary(const QString &id,const QString &path)
{
    MeshUiState state; state.id=id; state.ready=false; state.name=QFileInfo(path).fileName();
    state.summary=QString::fromUtf8("正在后台读取并提取边界…");
    state.displayMode=displayModeEditor_->itemText(2); state.color=QColor("#3b82c4"); state.unit="m";
    meshes_.append(state);
    const int row = meshTable_->rowCount();
    meshTable_->insertRow(row);
    {
        // Building a checkable cell emits itemChanged before its initial check
        // state is assigned. Do not interpret that as a user hiding the mesh.
        QSignalBlocker block(meshTable_);
        EditorUpdateScope updateGuard(editorUpdateDepth_);
        auto *indexItem = new QTableWidgetItem;
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        auto *nameItem = new QTableWidgetItem;
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        auto *visibleItem = new QTableWidgetItem;
        visibleItem->setFlags(visibleItem->flags() | Qt::ItemIsUserCheckable);
        visibleItem->setTextAlignment(Qt::AlignCenter);
        meshTable_->setItem(row, 0, indexItem);
        meshTable_->setItem(row, 1, nameItem);
        meshTable_->setItem(row, 2, visibleItem);
        
    }
    refreshTableRow(row);
    meshTable_->setCurrentCell(row, 1); refreshEditor(); updateCount();
}
void MeshManagementPage::finishBoundary(const QString &id,const QString &summary)
{
    for(int i=0;i<meshes_.size();++i) if(meshes_[i].id==id) {
        meshes_[i].ready=true; meshes_[i].summary=summary;
        refreshTableRow(i); refreshEditor(); emitBoundaryStyle(i); appendHistory(summary); return;
    }
}
void MeshManagementPage::failBoundary(const QString &id,const QString &message)
{
    for(int i=0;i<meshes_.size();++i) if(meshes_[i].id==id) {
        meshes_.remove(i); meshTable_->removeRow(i); break;
    }
    for(int i=0;i<meshes_.size();++i) refreshTableRow(i);
    refreshEditor(); updateCount(); viewerStatus_->setText(message); appendHistory(message);
}
void MeshManagementPage::setBoundaryBusy(bool busy) {boundaryBusy_=busy;updateCount();}
void MeshManagementPage::emitBoundaryStyle(int row)
{
    if(!boundaryMode_ || row<0 || row>=meshes_.size() || !meshes_[row].ready) return;
    const auto &s=meshes_[row];
    emit boundaryStyleChanged(s.id,{{"visible",s.visible},{"color",s.color},{"opacity",s.opacity},
        {"scale",s.scale},{"mode",displayModeEditor_->findText(s.displayMode)}});
}

int MeshManagementPage::selectedRow() const
{
    if (!meshTable_) return -1;
    const int row = meshTable_->currentRow();
    return row >= 0 && row < meshes_.size() ? row : -1;
}

} // namespace aerosim
