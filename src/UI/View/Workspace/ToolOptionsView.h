#pragma once

#include "Core/Types.h"

#include <QVector>
#include <QWidget>

#include <memory>

namespace Ui
{
class ToolOptionsView;
}

class QButtonGroup;
class QToolButton;
class Observers;
class SystemStatus;
class WorkspacePresenter;

// 도구 옵션과 활성 라벨 카드 (DL-20 ⑧ ⑨). 고른 도구의 옵션만 보인다
class ToolOptionsView : public QWidget
{
    Q_OBJECT

public:
    ToolOptionsView(Observers* kpObservers, SystemStatus* kpSystem, WorkspacePresenter* kpPresenter,
                    QWidget* parent = nullptr);
    ~ToolOptionsView() override;

private:
    void Refresh();
    void RefreshActiveLabel();
    void CommitSettings();
    ETool GetSettingsTool() const;

private:
    std::unique_ptr<Ui::ToolOptionsView> ui;
    Observers* m_kpObservers;
    SystemStatus* m_kpSystem;
    WorkspacePresenter* m_kpPresenter;
    QButtonGroup* m_pReachGroup;
    QButtonGroup* m_pOverwriteGroup;
    QVector<QToolButton*> m_vecRecentButtons;
    bool m_bUpdating;
};
