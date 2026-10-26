#include "UI/View/Workspace/LabelPanelView.h"

#include "ui_LabelPanelView.h"

#include "Core/LabelTable.h"
#include "Core/Observers.h"
#include "Core/SystemStatus.h"
#include "Presenter/Workspace/WorkspacePresenter.h"
#include "UI/Common/Control/OdontogramWidget.h"
#include "UI/Common/Theme/IconFactory.h"

#include <QButtonGroup>
#include <QHeaderView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace
{
enum EColumn
{
    COLUMN_LABEL = 0,
    COLUMN_NAME = 1,
    COLUMN_COUNT = 2,
    COLUMN_EYE = 3,
    COLUMN_LOCK = 4,
};

constexpr int kRoleLabel = Qt::UserRole + 1;
const QColor kActiveRow(0x2B, 0x3A, 0x5C);

const std::array<EToothState, 6> kStates = { {
    EToothState::NORMAL, EToothState::MISSING, EToothState::IMPLANT,
    EToothState::CROWN, EToothState::PARTIAL_ERUPTION, EToothState::SUPERNUMERARY,
} };

QIcon MakeSwatch(int label)
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    if (label == kUnlabeled)
    {
        painter.setPen(QPen(QColor(0x4A, 0x53, 0x5D), 2.0));
        painter.setBrush(QBrush(QColor(0x4A, 0x53, 0x5D), Qt::BDiagPattern));
    }
    else
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(LabelTable::GetColor(label));
    }
    painter.drawRoundedRect(QRectF(2, 2, 20, 20), 5, 5);
    return QIcon(pixmap);
}

QString GetQuadrantTitle(int quadrant)
{
    static const std::array<const char*, 5> arrNames = { { "", "상악 우측", "상악 좌측", "하악 좌측", "하악 우측" } };
    return QStringLiteral("%1사분면 · %2").arg(quadrant).arg(QString::fromUtf8(arrNames[static_cast<size_t>(quadrant)]));
}

int GetToothCode(int fdi)
{
    return LabelTable::MakeCode(LabelTable::kClassTooth, fdi);
}
}

LabelPanelView::LabelPanelView(Observers* kpObservers, SystemStatus* kpSystem, WorkspacePresenter* kpPresenter,
                               QWidget* parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::LabelPanelView>())
    , m_kpObservers(kpObservers)
    , m_kpSystem(kpSystem)
    , m_kpPresenter(kpPresenter)
    , m_pOdontogram(nullptr)
    , m_pStateGroup(new QButtonGroup(this))
    , m_pClassGroup(new QButtonGroup(this))
    , m_hashItems()
    , m_vecRowCodes()
    , m_timerRefresh()
    , m_bUpdating(false)
{
    ui->setupUi(this);
    setObjectName(QStringLiteral("labelPanel"));

    auto* pOdontogramLayout = new QVBoxLayout(ui->odontogramHost);
    pOdontogramLayout->setContentsMargins(0, 0, 0, 0);
    m_pOdontogram = new OdontogramWidget(ui->odontogramHost);
    pOdontogramLayout->addWidget(m_pOdontogram);

    // 클래스 버튼: 버튼 글자의 숫자가 파일의 label 값이다
    const std::vector<LabelTable::ClassInfo>& vecClasses = LabelTable::GetClasses();
    m_pClassGroup->setExclusive(true);
    for (size_t i = 0; i < vecClasses.size(); ++i)
    {
        const LabelTable::ClassInfo& info = vecClasses[i];
        auto* pButton = new QPushButton(QStringLiteral("%1 %2").arg(QString::fromUtf8(info.m_kpName)).arg(info.m_iValue), ui->classHost);
        pButton->setCheckable(true);
        pButton->setToolTip(QStringLiteral("label %1 · Alt+%1").arg(info.m_iValue));
        pButton->setProperty("segment", i == 0 ? QStringLiteral("left") : (i + 1 == vecClasses.size() ? QStringLiteral("right") : QStringLiteral("middle")));
        ui->layoutClasses->addWidget(pButton);
        m_pClassGroup->addButton(pButton, info.m_iValue);
    }

    QHeaderView* pHeader = ui->treeLabels->header();
    pHeader->setStretchLastSection(false);
    pHeader->setSectionResizeMode(COLUMN_LABEL, QHeaderView::Fixed);
    pHeader->setSectionResizeMode(COLUMN_NAME, QHeaderView::Stretch);
    pHeader->setSectionResizeMode(COLUMN_COUNT, QHeaderView::Fixed);
    pHeader->setSectionResizeMode(COLUMN_EYE, QHeaderView::Fixed);
    pHeader->setSectionResizeMode(COLUMN_LOCK, QHeaderView::Fixed);
    pHeader->resizeSection(COLUMN_LABEL, 62);
    pHeader->resizeSection(COLUMN_COUNT, 66);
    pHeader->resizeSection(COLUMN_EYE, 26);
    pHeader->resizeSection(COLUMN_LOCK, 26);
    ui->treeLabels->setSelectionMode(QAbstractItemView::NoSelection);
    ui->treeLabels->setFocusPolicy(Qt::NoFocus);
    ui->treeLabels->setIconSize(QSize(12, 12));

    m_pStateGroup->setExclusive(true);
    for (size_t i = 0; i < kStates.size(); ++i)
    {
        auto* pButton = new QPushButton(LabelTable::GetToothStateName(kStates[i]), ui->stateHost);
        pButton->setObjectName(QStringLiteral("stateButton"));
        pButton->setCheckable(true);
        ui->layoutStates->addWidget(pButton, static_cast<int>(i / 3), static_cast<int>(i % 3));
        m_pStateGroup->addButton(pButton, static_cast<int>(kStates[i]));
    }

    m_timerRefresh.setSingleShot(true);
    m_timerRefresh.setInterval(60);
    connect(&m_timerRefresh, &QTimer::timeout, this, &LabelPanelView::RefreshLabels);

    connect(m_kpPresenter, &WorkspacePresenter::CaseOpened, this, [this] {
        RebuildTree();
        RefreshLabels();
    });
    connect(m_kpPresenter, &WorkspacePresenter::LabelsChanged, this, &LabelPanelView::ScheduleRefresh);
    connect(m_kpPresenter, &WorkspacePresenter::LabelStateChanged, this, &LabelPanelView::ScheduleRefresh);
    connect(m_kpPresenter, &WorkspacePresenter::HistoryChanged, this, &LabelPanelView::RefreshHistory);
    connect(m_kpObservers, &Observers::ActiveLabelChanged, this, &LabelPanelView::RefreshLabels);
    connect(m_kpObservers, &Observers::ObjectClassChanged, this, &LabelPanelView::RefreshClassButtons);
    connect(m_kpObservers, &Observers::JawChanged, this, [this] {
        RebuildTree();
        RefreshLabels();
    });

    connect(ui->treeLabels, &QTreeWidget::itemClicked, this, &LabelPanelView::OnTreeItemClicked);
    connect(ui->treeLabels, &QTreeWidget::itemDoubleClicked, this, &LabelPanelView::OnTreeItemDoubleClicked);
    connect(m_pOdontogram, &OdontogramWidget::ToothClicked, m_kpPresenter, &WorkspacePresenter::SelectFdi);
    connect(m_pOdontogram, &OdontogramWidget::ToothDoubleClicked, this,
            [this](int fdi) { emit m_kpObservers->FocusLabelRequested(LabelTable::MakeCode(m_kpSystem->GetObjectClass(), fdi)); });
    connect(m_pOdontogram, &OdontogramWidget::ToothContextMenuRequested, this, &LabelPanelView::OnToothContextMenu);
    connect(m_pClassGroup, &QButtonGroup::idClicked, m_kpPresenter, &WorkspacePresenter::SelectClass);
    connect(ui->buttonEraser, &QPushButton::clicked, this, [this] { m_kpSystem->SetActiveTool(ETool::ERASER); });
    connect(m_pStateGroup, &QButtonGroup::idClicked, this, &LabelPanelView::OnStateClicked);
    connect(ui->editNote, &QLineEdit::editingFinished, this, [this] {
        if (!m_bUpdating)
        {
            m_kpPresenter->SetToothNote(LabelTable::GetFdiOf(m_kpSystem->GetActiveLabel()), ui->editNote->text().trimmed());
        }
    });
    connect(ui->listHistory, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* pItem) { m_kpPresenter->JumpToHistory(ui->listHistory->row(pItem)); });

    RebuildTree();
    RefreshLabels();
    RefreshHistory();
}

LabelPanelView::~LabelPanelView() = default;

void LabelPanelView::RebuildTree()
{
    ui->treeLabels->clear();
    m_hashItems.clear();
    m_vecRowCodes.clear();

    auto addGroup = [this](const QString& title) {
        auto* pItem = new QTreeWidgetItem(ui->treeLabels);
        pItem->setText(COLUMN_LABEL, title);
        pItem->setFirstColumnSpanned(true);
        pItem->setFlags(Qt::ItemIsEnabled);
        pItem->setForeground(COLUMN_LABEL, QColor(0x8B, 0x96, 0xA1));
        QFont font = pItem->font(COLUMN_LABEL);
        font.setPointSizeF(font.pointSizeF() * 0.9);
        font.setBold(true);
        pItem->setFont(COLUMN_LABEL, font);
    };
    auto addRow = [this](int label) {
        auto* pItem = new QTreeWidgetItem(ui->treeLabels);
        pItem->setData(COLUMN_LABEL, kRoleLabel, label);
        pItem->setIcon(COLUMN_LABEL, MakeSwatch(label));
        pItem->setText(COLUMN_LABEL, LabelTable::GetShortText(label));
        pItem->setTextAlignment(COLUMN_COUNT, Qt::AlignRight | Qt::AlignVCenter);
        pItem->setFlags(Qt::ItemIsEnabled);
        m_hashItems.insert(label, pItem);
        m_vecRowCodes.append(label);
    };

    const QVector<LabelRowData> vecRows = m_kpPresenter->GetLabelRows();
    addGroup(QStringLiteral("기타"));
    addRow(kGingiva);
    addRow(kUnlabeled);

    // 치아: 사분면마다 1 … 8
    for (int iQuadrant = 1; iQuadrant <= 4; ++iQuadrant)
    {
        bool bGroupAdded = false;
        for (int iPosition = 1; iPosition <= 8; ++iPosition)
        {
            const int iCode = GetToothCode(iQuadrant * 10 + iPosition);
            const bool bListed = std::any_of(vecRows.begin(), vecRows.end(), [iCode](const LabelRowData& row) { return row.m_iLabel == iCode; });
            if (!bListed)
            {
                continue;
            }
            if (!bGroupAdded)
            {
                addGroup(GetQuadrantTitle(iQuadrant));
                bGroupAdded = true;
            }
            addRow(iCode);
        }
    }

    // 그 밖의 클래스 (스캔바디 …): 클래스마다 묶는다
    int iLastClass = -1;
    for (const LabelRowData& row : vecRows)
    {
        const int iClass = LabelTable::GetClassOf(row.m_iLabel);
        if (iClass <= LabelTable::kClassTooth)
        {
            continue;
        }
        if (iClass != iLastClass)
        {
            addGroup(QStringLiteral("%1 · label %2").arg(LabelTable::GetClassName(iClass)).arg(iClass));
            iLastClass = iClass;
        }
        addRow(row.m_iLabel);
    }
    m_pOdontogram->SetJaw(m_kpSystem->GetJaw());
}

void LabelPanelView::ScheduleRefresh()
{
    if (!m_timerRefresh.isActive())
    {
        m_timerRefresh.start();
    }
}

void LabelPanelView::RefreshLabels()
{
    m_timerRefresh.stop();

    // 스캔바디처럼 칠한 대상이 늘거나 줄면 목록을 다시 만든다
    QVector<int> vecWanted;
    vecWanted.append(kGingiva);
    vecWanted.append(kUnlabeled);
    for (const LabelRowData& row : m_kpPresenter->GetLabelRows())
    {
        if (row.m_iLabel != kGingiva && row.m_iLabel != kUnlabeled)
        {
            vecWanted.append(row.m_iLabel);
        }
    }
    QVector<int> vecSortedWanted = vecWanted;
    QVector<int> vecSortedCurrent = m_vecRowCodes;
    std::sort(vecSortedWanted.begin(), vecSortedWanted.end());
    std::sort(vecSortedCurrent.begin(), vecSortedCurrent.end());
    if (vecSortedWanted != vecSortedCurrent)
    {
        RebuildTree();
    }

    const int iActive = m_kpSystem->GetActiveLabel();
    const QIcon iconEye = IconFactory::Create(EIcon::EYE, QColor(0x8B, 0x96, 0xA1));
    const QIcon iconEyeOff = IconFactory::Create(EIcon::EYE_OFF, QColor(0x5A, 0x63, 0x6D));
    const QIcon iconLock = IconFactory::Create(EIcon::LOCK, QColor(0xF2, 0xC1, 0x4E));
    const QIcon iconUnlock = IconFactory::Create(EIcon::UNLOCK, QColor(0x5A, 0x63, 0x6D));

    // 치식도: 치아 정점 · 상태 · 그 자리 스캔바디
    QHash<int, OdontogramWidget::ToothCell> hashCells;
    int iDone = 0;
    int iTeeth = 0;
    const EJaw eJaw = m_kpSystem->GetJaw();
    for (int iFdi : LabelTable::GetArchOrder(EJaw::UNKNOWN))
    {
        const LabelRowData row = m_kpPresenter->GetLabelRow(GetToothCode(iFdi));
        OdontogramWidget::ToothCell cell;
        cell.m_iCount = row.m_iCount;
        cell.m_eState = row.m_eState;
        cell.m_iScanbodyCount = m_kpPresenter->GetLabelRow(LabelTable::MakeCode(LabelTable::kClassScanbody, iFdi)).m_iCount;
        hashCells.insert(iFdi, cell);
        if (eJaw == EJaw::UNKNOWN || LabelTable::GetJawOfFdi(iFdi) == eJaw)
        {
            ++iTeeth;
            if (cell.m_iCount > 0 || cell.m_iScanbodyCount > 0 || cell.m_eState == EToothState::MISSING)
            {
                ++iDone;
            }
        }
    }
    m_pOdontogram->SetCells(hashCells);
    const LabelTable::ClassInfo* kpActiveClass = LabelTable::FindClass(LabelTable::GetClassOf(iActive));
    const bool bPositioned = kpActiveClass != nullptr && kpActiveClass->m_eFdiUse != LabelTable::EFdiUse::NONE;
    m_pOdontogram->SetActiveLabel(bPositioned ? LabelTable::GetFdiOf(iActive) : 0);
    ui->labelOdontogramSummary->setText(m_kpPresenter->HasCase()
                                            ? QStringLiteral("%1 %2 / %3").arg(LabelTable::GetJawName(eJaw)).arg(iDone).arg(iTeeth)
                                            : QString());

    for (auto it = m_hashItems.begin(); it != m_hashItems.end(); ++it)
    {
        const LabelRowData row = m_kpPresenter->GetLabelRow(it.key());
        QTreeWidgetItem* pItem = it.value();
        const bool bMissing = LabelTable::IsToothCode(it.key()) && row.m_eState == EToothState::MISSING;
        QString strName = row.m_strName;
        if (LabelTable::IsToothCode(it.key()) && row.m_eState != EToothState::NORMAL)
        {
            strName += QStringLiteral(" · %1").arg(LabelTable::GetToothStateName(row.m_eState));
        }
        pItem->setText(COLUMN_NAME, strName);
        pItem->setText(COLUMN_COUNT, bMissing && row.m_iCount == 0 ? QStringLiteral("결손") : QStringLiteral("%L1").arg(row.m_iCount));
        pItem->setIcon(COLUMN_EYE, row.m_bHidden ? iconEyeOff : iconEye);
        pItem->setToolTip(COLUMN_EYE, row.m_bHidden ? QStringLiteral("다시 보이기") : QStringLiteral("숨기기 (H)"));
        pItem->setIcon(COLUMN_LOCK, row.m_bLocked ? iconLock : iconUnlock);
        pItem->setToolTip(COLUMN_LOCK, row.m_bLocked ? QStringLiteral("잠금 풀기") : QStringLiteral("잠그기: 어떤 도구로도 바뀌지 않는다 (Ctrl+L)"));

        const QColor colText = bMissing || row.m_bHidden ? QColor(0x6C, 0x76, 0x82) : QColor(0xD6, 0xDC, 0xE2);
        const QBrush brushBackground = it.key() == iActive ? QBrush(kActiveRow) : QBrush();
        for (int iColumn = COLUMN_LABEL; iColumn <= COLUMN_LOCK; ++iColumn)
        {
            pItem->setForeground(iColumn, colText);
            pItem->setBackground(iColumn, brushBackground);
        }
    }
    if (QTreeWidgetItem* pActive = m_hashItems.value(iActive, nullptr))
    {
        ui->treeLabels->scrollToItem(pActive);
    }
    RefreshClassButtons();
    RefreshAttributes();
}

void LabelPanelView::RefreshClassButtons()
{
    // 치은 · 미분류처럼 자리가 없는 라벨이면 그 클래스, 아니면 숫자 입력에 붙는 클래스를 켠다
    const int iActive = m_kpSystem->GetActiveLabel();
    const int iActiveClass = LabelTable::GetClassOf(iActive);
    const LabelTable::ClassInfo* kpClass = LabelTable::FindClass(iActiveClass);
    const int iShown = kpClass != nullptr && kpClass->m_eFdiUse == LabelTable::EFdiUse::NONE ? iActiveClass : m_kpSystem->GetObjectClass();
    if (QAbstractButton* pButton = m_pClassGroup->button(iShown))
    {
        pButton->setChecked(true);
    }
}

void LabelPanelView::RefreshAttributes()
{
    m_bUpdating = true;
    const int iActive = m_kpSystem->GetActiveLabel();
    const int iFdi = LabelTable::GetFdiOf(iActive);
    const LabelRowData row = m_kpPresenter->GetLabelRow(iActive);
    const bool bPositioned = LabelTable::IsFdi(iFdi) && m_kpPresenter->HasCase();

    ui->labelToothTitle->setText(QStringLiteral("%1 · %2").arg(LabelTable::GetShortText(iActive), LabelTable::GetName(iActive)));
    ui->labelToothCount->setText(m_kpPresenter->HasCase() ? QStringLiteral("%L1 정점").arg(row.m_iCount) : QString());
    ui->stateHost->setEnabled(bPositioned);
    ui->editNote->setEnabled(bPositioned);
    if (QAbstractButton* pButton = m_pStateGroup->button(static_cast<int>(row.m_eState)))
    {
        pButton->setChecked(true);
    }
    if (!ui->editNote->hasFocus())
    {
        ui->editNote->setText(bPositioned ? row.m_strNote : QString());
    }
    m_bUpdating = false;
}

void LabelPanelView::RefreshHistory()
{
    ui->listHistory->clear();
    auto* pFirst = new QListWidgetItem(QStringLiteral("처음 상태 (열었을 때)"), ui->listHistory);
    pFirst->setForeground(QColor(0x8B, 0x96, 0xA1));

    const QVector<HistoryRowData> vecRows = m_kpPresenter->GetHistoryRows();
    const int iApplied = m_kpPresenter->GetHistoryAppliedCount();
    for (int i = 0; i < vecRows.size(); ++i)
    {
        const HistoryRowData& row = vecRows[i];
        auto* pItem = new QListWidgetItem(QStringLiteral("%1   %2   %L3").arg(row.m_strTime, row.m_strName).arg(row.m_iVertexCount),
                                          ui->listHistory);
        pItem->setForeground(row.m_bApplied ? QColor(0xD6, 0xDC, 0xE2) : QColor(0x5A, 0x63, 0x6D));
        if (i + 1 == iApplied)
        {
            pItem->setBackground(kActiveRow);
        }
    }
    if (iApplied == 0)
    {
        pFirst->setBackground(kActiveRow);
    }
    ui->listHistory->scrollToBottom();
}

void LabelPanelView::OnTreeItemClicked(QTreeWidgetItem* pItem, int column)
{
    const QVariant varLabel = pItem->data(COLUMN_LABEL, kRoleLabel);
    if (!varLabel.isValid())
    {
        return;
    }
    const int iLabel = varLabel.toInt();
    if (column == COLUMN_EYE)
    {
        m_kpPresenter->ToggleHidden(iLabel);
        return;
    }
    if (column == COLUMN_LOCK)
    {
        m_kpPresenter->ToggleLocked(iLabel);
        return;
    }
    if (iLabel == kUnlabeled)
    {
        m_kpSystem->SetActiveTool(ETool::ERASER);
        return;
    }
    m_kpPresenter->SelectLabel(iLabel);
}

void LabelPanelView::OnTreeItemDoubleClicked(QTreeWidgetItem* pItem, int column)
{
    const QVariant varLabel = pItem->data(COLUMN_LABEL, kRoleLabel);
    if (!varLabel.isValid() || column == COLUMN_EYE || column == COLUMN_LOCK)
    {
        return;
    }
    emit m_kpObservers->FocusLabelRequested(varLabel.toInt());
}

void LabelPanelView::OnStateClicked(int state)
{
    ChangeToothState(LabelTable::GetFdiOf(m_kpSystem->GetActiveLabel()), static_cast<EToothState>(state));
}

void LabelPanelView::OnToothContextMenu(int fdi, const QPoint& globalPos)
{
    const EToothState eCurrent = m_kpPresenter->GetLabelRow(GetToothCode(fdi)).m_eState;
    QMenu menu(this);
    menu.addSection(QStringLiteral("%1 · %2").arg(fdi).arg(LabelTable::GetFdiName(fdi)));
    for (EToothState eState : kStates)
    {
        QAction* pAction = menu.addAction(LabelTable::GetToothStateName(eState));
        pAction->setCheckable(true);
        pAction->setChecked(eState == eCurrent);
        pAction->setData(static_cast<int>(eState));
    }
    QAction* pChosen = menu.exec(globalPos);
    if (pChosen != nullptr)
    {
        ChangeToothState(fdi, static_cast<EToothState>(pChosen->data().toInt()));
    }
}

void LabelPanelView::ChangeToothState(int fdi, EToothState state)
{
    if (!LabelTable::IsFdi(fdi) || !m_kpPresenter->HasCase())
    {
        return;
    }
    if (state == EToothState::MISSING)
    {
        const int iCount = m_kpPresenter->GetLabelRow(GetToothCode(fdi)).m_iCount;
        if (iCount > 0)
        {
            const QMessageBox::StandardButton eAnswer = QMessageBox::question(
                this, QStringLiteral("결손으로 표시"),
                QStringLiteral("%1 치아에 정점 %L2개가 있다. 미분류로 되돌리고 결손으로 표시할까?").arg(fdi).arg(iCount),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (eAnswer != QMessageBox::Yes)
            {
                RefreshAttributes();
                return;
            }
            m_kpPresenter->ClearLabelVertices(GetToothCode(fdi));
        }
    }
    m_kpPresenter->SetToothState(fdi, state);
}
