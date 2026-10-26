#pragma once

#include "Core/Types.h"

#include <array>
#include <vector>

class Observers;

// 앱이 들고 있는 작업 상태. 값이 바뀌면 Observers 로 알린다 (docs/Qt.md 2.1.2)
class SystemStatus
{
public:
    explicit SystemStatus(Observers* kpObservers);

public:
    int GetActiveLabel() const;
    void SetActiveLabel(int label);
    const std::vector<int>& GetRecentLabels() const;

    ETool GetActiveTool() const;
    void SetActiveTool(ETool tool);

    EDisplayMode GetDisplayMode() const;
    void SetDisplayMode(EDisplayMode mode);

    float GetLabelOpacity() const;
    void SetLabelOpacity(float opacity);

    // 지우개는 브러시 설정을 같이 쓴다
    const PaintSettings& GetPaintSettings(ETool tool) const;
    void SetPaintSettings(ETool tool, const PaintSettings& settings);

    EJaw GetJaw() const;
    void SetJaw(EJaw jaw);

    // 숫자 두 개 · 치식도로 FDI 자리를 고를 때 붙일 클래스. 기본은 치아
    int GetObjectClass() const;
    void SetObjectClass(int classValue);

private:
    static size_t GetSettingsSlot(ETool tool);

private:
    Observers* m_kpObservers;
    int m_iActiveLabel;
    std::vector<int> m_vecRecentLabels;
    ETool m_eActiveTool;
    EDisplayMode m_eDisplayMode;
    float m_fLabelOpacity;
    std::array<PaintSettings, 3> m_arrPaintSettings;
    EJaw m_eJaw;
    int m_iObjectClass;
};
