#include "aerosim/ui/MeshManagementPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
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
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <cstddef>

namespace aerosim {
namespace {
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
}

MeshManagementPage::MeshManagementPage(QWidget *viewportHost, QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("viewportPage"));
    setStyleSheet(QStringLiteral(
        "#meshManagementHeader { background:#edf3fa; border-bottom:1px solid #c8d5e4; }"
        "#meshManagementTitle { color:#1f3b5b; font-size:15px; font-weight:600; }"
        "#meshObjectPanel, #meshViewerPanel, #meshHistoryPanel { background:#ffffff; border:1px solid #cbd5df; }"
        "#meshViewerToolbar { background:#f5f7fa; border-bottom:1px solid #d5dde6; }"
        "#meshList { border:1px solid #d5dde6; background:#ffffff; alternate-background-color:#f7f9fc; }"
        "#meshCountLabel, #meshViewerStatus { color:#637083; }"
        "#axisIndicator { color:#2b579a; background:rgba(255,255,255,220); border:1px solid #b8c8da; padding:5px; font-weight:600; }"
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

    verticalSplitter_ = new QSplitter(Qt::Vertical, this);
    verticalSplitter_->setObjectName(QStringLiteral("meshWorkspaceVerticalSplitter"));
    mainSplitter_ = new QSplitter(Qt::Horizontal, verticalSplitter_);
    mainSplitter_->setObjectName(QStringLiteral("meshWorkspaceMainSplitter"));
    mainSplitter_->setChildrenCollapsible(false);

    auto *objectPanel = new QFrame(mainSplitter_);
    objectPanel->setObjectName(QStringLiteral("meshObjectPanel"));
    objectPanel->setMinimumWidth(245);
    objectPanel->setMaximumWidth(430);
    auto *objectLayout = new QVBoxLayout(objectPanel);
    objectLayout->setContentsMargins(10, 10, 10, 10);
    objectLayout->setSpacing(7);
    auto *listHeading = new QHBoxLayout;
    auto *listTitle = new QLabel(uiText(u"\u7f51\u683c\u5bf9\u8c61"), objectPanel);
    QFont listTitleFont = listTitle->font();
    listTitleFont.setBold(true);
    listTitle->setFont(listTitleFont);
    listHeading->addWidget(listTitle);
    listHeading->addStretch();
    meshCount_ = new QLabel(objectPanel);
    meshCount_->setObjectName(QStringLiteral("meshObjectCount"));
    listHeading->addWidget(meshCount_);
    objectLayout->addLayout(listHeading);

    meshList_ = new QTreeWidget(objectPanel);
    meshList_->setObjectName(QStringLiteral("meshList"));
    meshList_->setHeaderHidden(true);
    meshList_->setRootIsDecorated(false);
    meshList_->setAlternatingRowColors(true);
    meshList_->setSelectionMode(QAbstractItemView::SingleSelection);
    meshList_->setMinimumHeight(118);
    meshList_->header()->setStretchLastSection(true);
    objectLayout->addWidget(meshList_, 1);

    auto *listActions = new QHBoxLayout;
    addMeshButton_ = new QPushButton(uiText(u"+ \u6dfb\u52a0\u7f51\u683c"), objectPanel);
    addMeshButton_->setObjectName(QStringLiteral("addMeshButton"));
    addMeshButton_->setToolTip(uiText(u"\u6dfb\u52a0\u4e00\u4e2a\u524d\u7aef Mock \u7f51\u683c\uff0c\u6700\u591a 10 \u4e2a"));
    removeMeshButton_ = new QPushButton(uiText(u"- \u79fb\u9664"), objectPanel);
    removeMeshButton_->setObjectName(QStringLiteral("removeMeshButton"));
    listActions->addWidget(addMeshButton_);
    listActions->addWidget(removeMeshButton_);
    objectLayout->addLayout(listActions);

    auto *properties = new QGroupBox(uiText(u"\u663e\u793a\u5c5e\u6027\uff08Mock\uff09"), objectPanel);
    properties->setObjectName(QStringLiteral("meshDisplayProperties"));
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
    auto *propertyScroll = new QScrollArea(objectPanel);
    propertyScroll->setObjectName(QStringLiteral("meshPropertyScroll"));
    propertyScroll->setWidgetResizable(true);
    propertyScroll->setFrameShape(QFrame::NoFrame);
    propertyScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    propertyScroll->setMinimumHeight(145);
    propertyScroll->setWidget(properties);
    objectLayout->addWidget(propertyScroll, 2);

    auto *viewerPanel = new QFrame(mainSplitter_);
    viewerPanel->setObjectName(QStringLiteral("meshViewerPanel"));
    viewerPanel->setMinimumWidth(420);
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
    background->setToolTip(uiText(u"\u4ec5\u5207\u6362\u524d\u7aef\u5360\u4f4d\u80cc\u666f\uff0c\u4e0d\u8c03\u7528 VTK \u6e32\u67d3"));
    displayTools->addWidget(background);
    toolbarLayout->addLayout(displayTools);
    viewerLayout->addWidget(toolbar);

    auto *canvas = new QFrame(viewerPanel);
    canvas->setObjectName(QStringLiteral("meshViewerCanvas"));
    auto *canvasLayout = new QGridLayout(canvas);
    canvasLayout->setContentsMargins(8, 8, 8, 8);
    viewportHost->setObjectName(QStringLiteral("viewportSlot"));
    canvasLayout->addWidget(viewportHost, 0, 0);
    axisIndicator_ = new QLabel(uiText(u"Z \u2191\n  \u2514\u2500 X\nY \u2299"), canvas);
    axisIndicator_->setObjectName(QStringLiteral("axisIndicator"));
    axisIndicator_->setAlignment(Qt::AlignCenter);
    axisIndicator_->setFixedSize(66, 64);
    canvasLayout->addWidget(axisIndicator_, 0, 0, Qt::AlignRight | Qt::AlignBottom);
    viewerLayout->addWidget(canvas, 1);
    viewerStatus_ = new QLabel(uiText(u"VTKViewer \u63d2\u69fd\u5df2\u4fdd\u7559 \u00b7 \u5f53\u524d\u4ec5\u663e\u793a\u524d\u7aef Mock \u72b6\u6001"), viewerPanel);
    viewerStatus_->setObjectName(QStringLiteral("meshViewerStatus"));
    viewerStatus_->setContentsMargins(10, 5, 10, 5);
    viewerLayout->addWidget(viewerStatus_);
    mainSplitter_->addWidget(objectPanel);
    mainSplitter_->addWidget(viewerPanel);
    mainSplitter_->setStretchFactor(0, 0);
    mainSplitter_->setStretchFactor(1, 1);
    mainSplitter_->setSizes({290, 850});

    auto *historyPanel = new QFrame(verticalSplitter_);
    historyPanel->setObjectName(QStringLiteral("meshHistoryPanel"));
    auto *historyLayout = new QVBoxLayout(historyPanel);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    historyLayout->setSpacing(0);
    auto *historyHeader = new QWidget(historyPanel);
    auto *historyHeaderLayout = new QHBoxLayout(historyHeader);
    historyHeaderLayout->setContentsMargins(9, 3, 7, 3);
    historyHeaderLayout->addWidget(new QLabel(uiText(u"\u7f51\u683c\u64cd\u4f5c\u8f85\u52a9\u533a"), historyHeader));
    historyHeaderLayout->addStretch();
    historyCollapseButton_ = new QPushButton(uiText(u"\u6536\u8d77"), historyHeader);
    historyCollapseButton_->setObjectName(QStringLiteral("meshHistoryCollapseButton"));
    historyCollapseButton_->setMaximumWidth(58);
    historyHeaderLayout->addWidget(historyCollapseButton_);
    historyLayout->addWidget(historyHeader);
    historyTabs_ = new QTabWidget(historyPanel);
    historyTabs_->setObjectName(QStringLiteral("meshHistoryTabs"));
    historyList_ = new QListWidget(historyTabs_);
    historyList_->setObjectName(QStringLiteral("meshOperationHistory"));
    historyList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
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
    verticalSplitter_->addWidget(mainSplitter_);
    verticalSplitter_->addWidget(historyPanel);
    verticalSplitter_->setStretchFactor(0, 1);
    verticalSplitter_->setStretchFactor(1, 0);
    verticalSplitter_->setSizes({620, expandedHistoryHeight_});
    pageLayout->addWidget(verticalSplitter_, 1);

    connect(addMeshButton_, &QPushButton::clicked, this, &MeshManagementPage::addMesh);
    connect(removeMeshButton_, &QPushButton::clicked, this, &MeshManagementPage::removeCurrentMesh);
    connect(meshList_, &QTreeWidget::currentItemChanged, this,
        [this](QTreeWidgetItem *item) { if (item) selectMesh(meshList_->indexOfTopLevelItem(item)); });
    connect(meshList_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item) {
        if (updatingEditor_ || !item) return;
        const int row = meshList_->indexOfTopLevelItem(item);
        if (row < 0 || row >= meshes_.size()) return;
        meshes_[row].visible = item->checkState(0) == Qt::Checked;
        if (row == meshList_->indexOfTopLevelItem(meshList_->currentItem())) refreshEditor();
        appendHistory((meshes_[row].visible ? uiText(u"\u663e\u793a ") : uiText(u"\u9690\u85cf ")) + meshes_[row].name);
    });
    connect(nameEditor_, &QLineEdit::editingFinished, this, &MeshManagementPage::updateCurrentMesh);
    connect(visibleEditor_, &QCheckBox::toggled, this, &MeshManagementPage::updateCurrentMesh);
    connect(displayModeEditor_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshManagementPage::updateCurrentMesh);
    connect(opacityEditor_, &QSlider::valueChanged, this, &MeshManagementPage::updateCurrentMesh);
    connect(scaleEditor_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MeshManagementPage::updateCurrentMesh);
    connect(unitEditor_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshManagementPage::updateCurrentMesh);
    connect(colorButton_, &QPushButton::clicked, this, [this] {
        const int row = meshList_->indexOfTopLevelItem(meshList_->currentItem());
        if (row < 0 || row >= meshes_.size()) return;
        const QVector<QColor> colors = {QColor("#3b82c4"), QColor("#e58b2a"), QColor("#4d9d69"), QColor("#8c63b8")};
        int index = colors.indexOf(meshes_[row].color);
        meshes_[row].color = colors[(index + 1) % colors.size()];
        refreshEditor();
        appendHistory(uiText(u"\u4fee\u6539 %1 \u663e\u793a\u989c\u8272\uff08Mock\uff09").arg(meshes_[row].name));
    });
    connect(surfaceButton, &QToolButton::clicked, this, [this] {
        displayModeEditor_->setCurrentText(uiText(u"\u8868\u9762"));
    });
    connect(wireframeButton, &QToolButton::clicked, this, [this] {
        displayModeEditor_->setCurrentText(uiText(u"\u7ebf\u6846"));
    });
    connect(axisButton, &QToolButton::toggled, axisIndicator_, &QWidget::setVisible);
    connect(background, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, canvas, background](int index) {
        const QString color = index == 1 ? QStringLiteral("#273443") : index == 2
            ? QStringLiteral("qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #dce9f7,stop:1 #6f849b)")
            : QStringLiteral("#edf2f7");
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
        const QSignalBlocker blocker(meshList_);
        auto *item = new QTreeWidgetItem(meshList_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        refreshListItem(i);
    }
    updateCount();
    appendHistory(uiText(u"\u521d\u59cb\u5316 3 \u4e2a Mock \u7f51\u683c\u5bf9\u8c61"));
    meshList_->setCurrentItem(meshList_->topLevelItem(0));
    setHistoryCollapsed(true);
}

void MeshManagementPage::addMesh()
{
    if (meshes_.size() >= 10) return;
    MeshUiState state;
    state.name = QStringLiteral("Mesh_%1").arg(meshes_.size() + 1, 3, 10, QLatin1Char('0'));
    state.displayMode = uiText(u"\u8868\u9762");
    state.color = QColor("#3b82c4");
    state.unit = QStringLiteral("m");
    meshes_.append(state);
    QTreeWidgetItem *item = nullptr;
    {
        const QSignalBlocker blocker(meshList_);
        item = new QTreeWidgetItem(meshList_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        refreshListItem(meshes_.size() - 1);
    }
    meshList_->setCurrentItem(item);
    appendHistory(uiText(u"\u6dfb\u52a0\u7f51\u683c %1\uff08Mock\uff09").arg(state.name));
    updateCount();
}

void MeshManagementPage::removeCurrentMesh()
{
    const int row = meshList_->indexOfTopLevelItem(meshList_->currentItem());
    if (row < 0 || row >= meshes_.size()) return;
    const QString name = meshes_[row].name;
    meshes_.remove(row);
    delete meshList_->takeTopLevelItem(row);
    if (!meshes_.isEmpty()) meshList_->setCurrentItem(meshList_->topLevelItem(qMin(row, meshes_.size() - 1)));
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

void MeshManagementPage::refreshListItem(int row)
{
    if (row < 0 || row >= meshes_.size()) return;
    auto *item = meshList_->topLevelItem(row);
    if (!item) return;
    updatingEditor_ = true;
    item->setText(0, meshes_[row].name);
    item->setCheckState(0, meshes_[row].visible ? Qt::Checked : Qt::Unchecked);
    QPixmap marker(12, 12);
    marker.fill(meshes_[row].color);
    item->setIcon(0, QIcon(marker));
    item->setToolTip(0, meshes_[row].visible
        ? uiText(u"\u524d\u7aef\u72b6\u6001\uff1a\u53ef\u89c1\uff08\u672a\u8fde\u63a5 VTK \u5bf9\u8c61\uff09")
        : uiText(u"\u524d\u7aef\u72b6\u6001\uff1a\u9690\u85cf\uff08\u672a\u8fde\u63a5 VTK \u5bf9\u8c61\uff09"));
    updatingEditor_ = false;
}

void MeshManagementPage::refreshEditor()
{
    const int row = meshList_->indexOfTopLevelItem(meshList_->currentItem());
    const bool valid = row >= 0 && row < meshes_.size();
    const QVector<QWidget *> editors = {nameEditor_, visibleEditor_, displayModeEditor_, colorButton_,
                                        opacityEditor_, scaleEditor_, unitEditor_};
    for (auto *widget : editors) widget->setEnabled(valid);
    removeMeshButton_->setEnabled(valid);
    updatingEditor_ = true;
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
        viewerStatus_->setText(uiText(u"\u5f53\u524d\u7f16\u8f91\uff1a%1 \u00b7 %2 \u00b7 \u524d\u7aef Mock \u72b6\u6001")
            .arg(state.name, state.visible ? uiText(u"\u53ef\u89c1") : uiText(u"\u9690\u85cf")));
    } else {
        nameEditor_->clear();
        opacityValue_->clear();
        colorButton_->setText(uiText(u"\u65e0\u9009\u62e9"));
        viewerStatus_->setText(uiText(u"\u5c1a\u672a\u9009\u62e9\u7f51\u683c\u5bf9\u8c61 \u00b7 VTKViewer \u63d2\u69fd\u4fdd\u6301\u4e0d\u53d8"));
    }
    updatingEditor_ = false;
}

void MeshManagementPage::updateCurrentMesh()
{
    if (updatingEditor_) return;
    const int row = meshList_->indexOfTopLevelItem(meshList_->currentItem());
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
    refreshListItem(row);
    refreshEditor();
    appendHistory(uiText(u"\u66f4\u65b0 %1 \u663e\u793a\u5c5e\u6027\uff08Mock\uff09").arg(state.name));
}

void MeshManagementPage::updateCount()
{
    meshCount_->setText(uiText(u"%1 / 10 \u4e2a").arg(meshes_.size()));
    addMeshButton_->setEnabled(meshes_.size() < 10);
    addMeshButton_->setToolTip(meshes_.size() < 10
        ? uiText(u"\u6dfb\u52a0\u4e00\u4e2a\u524d\u7aef Mock \u7f51\u683c\uff0c\u6700\u591a 10 \u4e2a")
        : uiText(u"\u5df2\u8fbe\u5230 10 \u4e2a\u7f51\u683c\u5bf9\u8c61\u7684\u754c\u9762\u4e0a\u9650"));
}

void MeshManagementPage::appendHistory(const QString &message)
{
    historyList_->addItem(QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message));
    historyList_->scrollToBottom();
}

void MeshManagementPage::setHistoryCollapsed(bool collapsed)
{
    historyCollapsed_ = collapsed;
    if (collapsed) {
        const QList<int> sizes = verticalSplitter_->sizes();
        if (sizes.size() == 2 && sizes[1] > 35) expandedHistoryHeight_ = sizes[1];
        historyTabs_->hide();
        historyCollapseButton_->setText(uiText(u"\u5c55\u5f00"));
        verticalSplitter_->setSizes({qMax(500, height()), 32});
    } else {
        historyTabs_->show();
        historyCollapseButton_->setText(uiText(u"\u6536\u8d77"));
        verticalSplitter_->setSizes({qMax(300, height() - expandedHistoryHeight_), expandedHistoryHeight_});
    }
}

QVariantMap MeshManagementPage::layoutState() const
{
    const QList<int> horizontal = mainSplitter_->sizes();
    const int horizontalTotal = horizontal.value(0) + horizontal.value(1);
    const QList<int> vertical = verticalSplitter_->sizes();
    const int verticalTotal = vertical.value(0) + vertical.value(1);
    QVariantMap state;
    state.insert(QStringLiteral("objectPanelRatio"), horizontalTotal > 0
        ? double(horizontal.value(0)) / horizontalTotal : 0.255);
    state.insert(QStringLiteral("historyRatio"), verticalTotal > 0 && !historyCollapsed_
        ? double(vertical.value(1)) / verticalTotal
        : double(expandedHistoryHeight_) / qMax(1, height()));
    state.insert(QStringLiteral("historyCollapsed"), historyCollapsed_);
    return state;
}

void MeshManagementPage::restoreLayoutState(const QVariantMap &state)
{
    const double objectRatio = qBound(0.20, state.value(QStringLiteral("objectPanelRatio"), 0.255).toDouble(), 0.42);
    const double historyRatio = qBound(0.10, state.value(QStringLiteral("historyRatio"), 0.20).toDouble(), 0.45);
    mainSplitter_->setSizes({qRound(objectRatio * 1000), qRound((1.0 - objectRatio) * 1000)});
    expandedHistoryHeight_ = qBound(90, qRound(qMax(450, height()) * historyRatio), qMax(120, height() / 2));
    setHistoryCollapsed(state.value(QStringLiteral("historyCollapsed"), true).toBool());
}

void MeshManagementPage::restoreDefaultLayout()
{
    mainSplitter_->setSizes({290, 850});
    expandedHistoryHeight_ = 145;
    setHistoryCollapsed(true);
}

} // namespace aerosim
