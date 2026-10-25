#pragma once

#include "Core/Types.h"

#include <QHash>
#include <QWidget>

class QButtonGroup;
class QToolButton;
class Observers;
class SystemStatus;

// 도구 레일 (DL-20 ⑦). 버튼 아래 글자가 단축키다
class ToolRailView : public QWidget
{
    Q_OBJECT

public:
    ToolRailView(Observers* kpObservers, SystemStatus* kpSystem, QWidget* parent = nullptr);

private:
    void AddTool(ETool tool, const QString& name, const QString& key);
    void OnToolChanged(ETool tool);

private:
    Observers* m_kpObservers;
    SystemStatus* m_kpSystem;
    QButtonGroup* m_pGroup;
    QHash<int, QToolButton*> m_hashButtons;
};
