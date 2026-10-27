#include "UI/View/Workspace/ToolOptionsView.h"

#include "ui_ToolOptionsView.h"

#include "Core/LabelTable.h"
#include "Core/Observers.h"
#include "Core/SystemStatus.h"
#include "Presenter/Workspace/WorkspacePresenter.h"

#include <QButtonGroup>
#include <QToolButton>

#include <cmath>

namespace
{
constexpr int kRecentButtonCount = 4;

QString GetToolName(ETool tool)
{
    switch (tool)
    {
    case ETool::NAVIGATE:
        return QStringLiteral("이동");
    case ETool::BRUSH:
        return QStringLiteral("브러시");
    case ETool::LASSO:
        return QStringLiteral("올가미");
    case ETool::FILL:
        return QStringLiteral("채우기");
    case ETool::ERASER:
        return QStringLiteral("지우개");
    }
    return QString();
}

QString GetToolKey(ETool tool)
{
    switch (tool)
    {
    case ETool::NAVIGATE:
        return QStringLiteral("V");
    case ETool::BRUSH:
        return QStringLiteral("B");
    case ETool::LASSO:
        return QStringLiteral("L");
    case ETool::FILL:
        return QStringLiteral("F");
    case ETool::ERASER:
        return QStringLiteral("E");
    }
    return QString();
}

QString GetToolHint(ETool tool)
{
    switch (tool)
    {
    case ETool::NAVIGATE:
        return QStringLiteral("칠하지 않고 둘러본다. 왼쪽 드래그로 회전하고, 더블클릭한 곳이 회전 중심이 된다.");
    case ETool::FILL:
        return QStringLiteral("클릭한 정점과 같은 라벨로 이어진 영역 전체를 바꾼다. 경계로 둘러싼 미분류 구멍을 한 번에 메울 때 쓴다.");
    case ETool::LASSO:
        return QStringLiteral("화면에 닫힌 곡선을 그려 안쪽을 칠한다.");
    case ETool::ERASER:
        return QStringLiteral("미분류로 되돌린다. 잠근 라벨은 지워지지 않는다.");
    case ETool::BRUSH:
        break;
    }
    return QString();
}
}

ToolOptionsView::ToolOptionsView(Observers* kpObservers, SystemStatus* kpSystem, WorkspacePresenter* kpPresenter,
                                 QWidget* parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::ToolOptionsView>())
    , m_kpObservers(kpObservers)
    , m_kpSystem(kpSystem)
    , m_kpPresenter(kpPresenter)
    , m_pReachGroup(new QButtonGroup(this))
    , m_pOverwriteGroup(new QButtonGroup(this))
    , m_vecRecentButtons()
    , m_bUpdating(false)
{
    ui->setupUi(this);
    setObjectName(QStringLiteral("toolOptions"));

    m_pReachGroup->addButton(ui->buttonVisibleOnly, static_cast<int>(EPaintReach::VISIBLE_ONLY));
    m_pReachGroup->addButton(ui->buttonThrough, static_cast<int>(EPaintReach::THROUGH));
    m_pOverwriteGroup->addButton(ui->radioOverwriteAll, static_cast<int>(EOverwriteRule::ALL));
    m_pOverwriteGroup->addButton(ui->radioOverwriteUnlabeledGingiva, static_cast<int>(EOverwriteRule::UNLABELED_AND_GINGIVA));
    m_pOverwriteGroup->addButton(ui->radioOverwriteUnlabeledOnly, static_cast<int>(EOverwriteRule::UNLABELED_ONLY));

    for (int i = 0; i < kRecentButtonCount; ++i)
    {
        auto* pButton = new QToolButton(ui->recentHost);
        pButton->setObjectName(QStringLiteral("recentLabelButton"));
        pButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        ui->layoutRecent->addWidget(pButton);
        connect(pButton, &QToolButton::clicked, this, [this, pButton] {
            m_kpPresenter->SelectLabel(pButton->property("label").toInt());
        });
        m_vecRecentButtons.append(pButton);
    }
    ui->layoutRecent->addStretch(1);

    connect(ui->sliderSize, &QSlider::valueChanged, this, &ToolOptionsView::CommitSettings);
    connect(ui->sliderSensitivity, &QSlider::valueChanged, this, &ToolOptionsView::CommitSettings);
    connect(ui->checkCurvatureStop, &QCheckBox::toggled, this, &ToolOptionsView::CommitSettings);
    connect(m_pReachGroup, &QButtonGroup::idClicked, this, [this] { CommitSettings(); });
    connect(m_pOverwriteGroup, &QButtonGroup::idClicked, this, [this] { CommitSettings(); });

    connect(m_kpObservers, &Observers::ActiveToolChanged, this, &ToolOptionsView::Refresh);
    connect(m_kpObservers, &Observers::PaintSettingsChanged, this, &ToolOptionsView::Refresh);
    connect(m_kpObservers, &Observers::ActiveLabelChanged, this, &ToolOptionsView::RefreshActiveLabel);
    connect(m_kpObservers, &Observers::LabelColorsChanged, this, &ToolOptionsView::RefreshActiveLabel);

    Refresh();
    RefreshActiveLabel();
}

ToolOptionsView::~ToolOptionsView() = default;

void ToolOptionsView::Refresh()
{
    m_bUpdating = true;
    const ETool eTool = m_kpSystem->GetActiveTool();
    const PaintSettings& settings = m_kpSystem->GetPaintSettings(GetSettingsTool());
    const bool bBrush = eTool == ETool::BRUSH || eTool == ETool::ERASER;

    ui->labelToolName->setText(GetToolName(eTool));
    ui->labelToolKey->setText(GetToolKey(eTool));
    ui->groupSize->setVisible(bBrush);
    ui->groupReach->setVisible(bBrush || eTool == ETool::LASSO);
    ui->groupOverwrite->setVisible(eTool == ETool::BRUSH || eTool == ETool::LASSO || eTool == ETool::FILL);
    ui->groupCurvature->setVisible(bBrush);
    ui->labelToolHint->setText(GetToolHint(eTool));
    ui->labelToolHint->setVisible(!ui->labelToolHint->text().isEmpty());

    ui->sliderSize->setValue(static_cast<int>(std::lround(settings.m_fRadius * 10.0f)));
    ui->labelSizeValue->setText(QStringLiteral("%1 mm").arg(static_cast<double>(settings.m_fRadius), 0, 'f', 1));
    if (QAbstractButton* pReach = m_pReachGroup->button(static_cast<int>(settings.m_eReach)))
    {
        pReach->setChecked(true);
    }
    if (QAbstractButton* pOverwrite = m_pOverwriteGroup->button(static_cast<int>(settings.m_eOverwrite)))
    {
        pOverwrite->setChecked(true);
    }
    ui->checkCurvatureStop->setChecked(settings.m_bCurvatureStop);
    ui->sliderSensitivity->setValue(static_cast<int>(std::lround(settings.m_fSensitivity * 100.0f)));
    ui->sliderSensitivity->setEnabled(settings.m_bCurvatureStop);
    ui->labelSensitivityValue->setText(QString::number(static_cast<double>(settings.m_fSensitivity), 'f', 2));
    m_bUpdating = false;
}

void ToolOptionsView::RefreshActiveLabel()
{
    const int iLabel = m_kpSystem->GetActiveLabel();
    const QColor colLabel = LabelTable::GetColor(iLabel);
    const QString strShort = LabelTable::GetShortText(iLabel);
    ui->labelActiveSwatch->setText(strShort);
    ui->labelActiveSwatch->setStyleSheet(QStringLiteral("background:%1;color:#10161C;border-radius:8px;font-weight:600;font-size:%2px;")
                                             .arg(colLabel.name())
                                             .arg(strShort.size() > 2 ? 11 : 15));
    ui->labelActiveName->setText(LabelTable::GetName(iLabel));

    const std::vector<int>& vecRecent = m_kpSystem->GetRecentLabels();
    for (int i = 0; i < m_vecRecentButtons.size(); ++i)
    {
        QToolButton* pButton = m_vecRecentButtons[i];
        if (i >= static_cast<int>(vecRecent.size()))
        {
            pButton->hide();
            continue;
        }
        const int iRecent = vecRecent[static_cast<size_t>(i)];
        QPixmap pixmap(10, 10);
        pixmap.fill(LabelTable::GetColor(iRecent));
        pButton->setIcon(QIcon(pixmap));
        pButton->setText(LabelTable::GetShortText(iRecent));
        pButton->setToolTip(LabelTable::GetName(iRecent));
        pButton->setProperty("label", iRecent);
        pButton->show();
    }
}

void ToolOptionsView::CommitSettings()
{
    if (m_bUpdating)
    {
        return;
    }
    PaintSettings settings = m_kpSystem->GetPaintSettings(GetSettingsTool());
    settings.m_fRadius = static_cast<float>(ui->sliderSize->value()) / 10.0f;
    settings.m_eReach = static_cast<EPaintReach>(m_pReachGroup->checkedId());
    if (m_pOverwriteGroup->checkedId() >= 0)
    {
        settings.m_eOverwrite = static_cast<EOverwriteRule>(m_pOverwriteGroup->checkedId());
    }
    settings.m_bCurvatureStop = ui->checkCurvatureStop->isChecked();
    settings.m_fSensitivity = static_cast<float>(ui->sliderSensitivity->value()) / 100.0f;
    m_kpSystem->SetPaintSettings(GetSettingsTool(), settings);
}

ETool ToolOptionsView::GetSettingsTool() const
{
    const ETool eTool = m_kpSystem->GetActiveTool();
    return eTool == ETool::NAVIGATE ? ETool::BRUSH : eTool;
}
