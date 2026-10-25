#include "Core/SystemStatus.h"

#include "Core/Observers.h"

#include <algorithm>

namespace
{
constexpr size_t kRecentLabelCount = 4;
constexpr size_t kSlotBrush = 0;
constexpr size_t kSlotLasso = 1;
constexpr size_t kSlotFill = 2;
}

SystemStatus::SystemStatus(Observers* kpObservers)
    : m_kpObservers(kpObservers)
    , m_iActiveLabel(kGingiva)
    , m_vecRecentLabels()
    , m_eActiveTool(ETool::BRUSH)
    , m_eDisplayMode(EDisplayMode::LABEL)
    , m_fLabelOpacity(0.7f)
    , m_arrPaintSettings()
    , m_eJaw(EJaw::UNKNOWN)
{
    // 채우기는 연결된 같은 라벨 전체를 바꾸므로, 치은 전체가 한 번에 치아로 바뀌지 않게 미분류만 바꾸는 것이 기본이다
    m_arrPaintSettings[kSlotFill].m_eOverwrite = EOverwriteRule::UNLABELED_ONLY;
    m_vecRecentLabels.push_back(m_iActiveLabel);
}

int SystemStatus::GetActiveLabel() const
{
    return m_iActiveLabel;
}

void SystemStatus::SetActiveLabel(int label)
{
    auto it = std::find(m_vecRecentLabels.begin(), m_vecRecentLabels.end(), label);
    if (it != m_vecRecentLabels.end())
    {
        m_vecRecentLabels.erase(it);
    }
    m_vecRecentLabels.insert(m_vecRecentLabels.begin(), label);
    if (m_vecRecentLabels.size() > kRecentLabelCount)
    {
        m_vecRecentLabels.resize(kRecentLabelCount);
    }

    m_iActiveLabel = label;
    emit m_kpObservers->ActiveLabelChanged(label);
}

const std::vector<int>& SystemStatus::GetRecentLabels() const
{
    return m_vecRecentLabels;
}

ETool SystemStatus::GetActiveTool() const
{
    return m_eActiveTool;
}

void SystemStatus::SetActiveTool(ETool tool)
{
    if (m_eActiveTool == tool)
    {
        return;
    }
    m_eActiveTool = tool;
    emit m_kpObservers->ActiveToolChanged(tool);
}

EDisplayMode SystemStatus::GetDisplayMode() const
{
    return m_eDisplayMode;
}

void SystemStatus::SetDisplayMode(EDisplayMode mode)
{
    if (m_eDisplayMode == mode)
    {
        return;
    }
    m_eDisplayMode = mode;
    emit m_kpObservers->DisplayModeChanged(mode);
}

float SystemStatus::GetLabelOpacity() const
{
    return m_fLabelOpacity;
}

void SystemStatus::SetLabelOpacity(float opacity)
{
    const float fOpacity = std::clamp(opacity, 0.0f, 1.0f);
    if (qFuzzyCompare(m_fLabelOpacity, fOpacity))
    {
        return;
    }
    m_fLabelOpacity = fOpacity;
    emit m_kpObservers->LabelOpacityChanged(fOpacity);
}

const PaintSettings& SystemStatus::GetPaintSettings(ETool tool) const
{
    return m_arrPaintSettings[GetSettingsSlot(tool)];
}

void SystemStatus::SetPaintSettings(ETool tool, const PaintSettings& settings)
{
    PaintSettings& paintSettings = m_arrPaintSettings[GetSettingsSlot(tool)];
    paintSettings = settings;
    paintSettings.m_fRadius = std::clamp(paintSettings.m_fRadius, 0.2f, 10.0f);
    paintSettings.m_fSensitivity = std::clamp(paintSettings.m_fSensitivity, 0.0f, 1.0f);
    emit m_kpObservers->PaintSettingsChanged(tool);
}

EJaw SystemStatus::GetJaw() const
{
    return m_eJaw;
}

void SystemStatus::SetJaw(EJaw jaw)
{
    if (m_eJaw == jaw)
    {
        return;
    }
    m_eJaw = jaw;
    emit m_kpObservers->JawChanged(jaw);
}

size_t SystemStatus::GetSettingsSlot(ETool tool)
{
    switch (tool)
    {
    case ETool::LASSO:
        return kSlotLasso;
    case ETool::FILL:
        return kSlotFill;
    case ETool::NAVIGATE:
    case ETool::BRUSH:
    case ETool::ERASER:
        break;
    }
    return kSlotBrush;
}
