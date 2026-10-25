#include "UI/View/Workspace/ToolRailView.h"

#include "Core/Observers.h"
#include "Core/SystemStatus.h"
#include "UI/Common/Theme/IconFactory.h"

#include <QButtonGroup>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
EIcon GetToolIcon(ETool tool)
{
    switch (tool)
    {
    case ETool::NAVIGATE:
        return EIcon::NAVIGATE;
    case ETool::BRUSH:
        return EIcon::BRUSH;
    case ETool::LASSO:
        return EIcon::LASSO;
    case ETool::FILL:
        return EIcon::FILL;
    case ETool::ERASER:
        return EIcon::ERASER;
    }
    return EIcon::BRUSH;
}
}

ToolRailView::ToolRailView(Observers* kpObservers, SystemStatus* kpSystem, QWidget* parent)
    : QWidget(parent)
    , m_kpObservers(kpObservers)
    , m_kpSystem(kpSystem)
    , m_pGroup(new QButtonGroup(this))
    , m_hashButtons()
{
    setObjectName(QStringLiteral("toolRail"));
    auto* pLayout = new QVBoxLayout(this);
    pLayout->setContentsMargins(7, 10, 7, 10);
    pLayout->setSpacing(4);
    m_pGroup->setExclusive(true);

    AddTool(ETool::NAVIGATE, QStringLiteral("이동"), QStringLiteral("V"));
    AddTool(ETool::BRUSH, QStringLiteral("브러시"), QStringLiteral("B"));
    AddTool(ETool::LASSO, QStringLiteral("올가미"), QStringLiteral("L"));
    AddTool(ETool::FILL, QStringLiteral("채우기"), QStringLiteral("F"));
    AddTool(ETool::ERASER, QStringLiteral("지우개"), QStringLiteral("E"));
    pLayout->addStretch(1);

    connect(m_pGroup, &QButtonGroup::idClicked, this,
            [this](int id) { m_kpSystem->SetActiveTool(static_cast<ETool>(id)); });
    connect(m_kpObservers, &Observers::ActiveToolChanged, this, &ToolRailView::OnToolChanged);
    OnToolChanged(m_kpSystem->GetActiveTool());
}

void ToolRailView::AddTool(ETool tool, const QString& name, const QString& key)
{
    auto* pButton = new QToolButton(this);
    pButton->setObjectName(QStringLiteral("toolButton"));
    pButton->setCheckable(true);
    pButton->setIcon(IconFactory::Create(GetToolIcon(tool), QColor(0x8B, 0x96, 0xA1), QColor(Qt::white)));
    pButton->setIconSize(QSize(20, 20));
    pButton->setText(key);
    pButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    pButton->setToolTip(QStringLiteral("%1 (%2)").arg(name, key));
    pButton->setFixedSize(38, 44);
    m_pGroup->addButton(pButton, static_cast<int>(tool));
    m_hashButtons.insert(static_cast<int>(tool), pButton);
    layout()->addWidget(pButton);
}

void ToolRailView::OnToolChanged(ETool tool)
{
    QToolButton* pButton = m_hashButtons.value(static_cast<int>(tool), nullptr);
    if (pButton != nullptr)
    {
        pButton->setChecked(true);
    }
}
