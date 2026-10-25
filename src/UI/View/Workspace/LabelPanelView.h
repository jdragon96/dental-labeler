#pragma once

#include "Core/Types.h"

#include <QHash>
#include <QTimer>
#include <QWidget>

#include <memory>

namespace Ui
{
class LabelPanelView;
}

class QButtonGroup;
class QTreeWidgetItem;
class Observers;
class OdontogramWidget;
class SystemStatus;
class WorkspacePresenter;

// 오른쪽 패널 (DL-20 ⑭ ⑮ ⑯, 기록 탭)
class LabelPanelView : public QWidget
{
    Q_OBJECT

public:
    LabelPanelView(Observers* kpObservers, SystemStatus* kpSystem, WorkspacePresenter* kpPresenter,
                   QWidget* parent = nullptr);
    ~LabelPanelView() override;

public:
    // 결손으로 바꿀 때 정점이 있으면 먼저 묻는다
    void ChangeToothState(int label, EToothState state);

private:
    void RebuildTree();
    void ScheduleRefresh();
    void RefreshLabels();
    void RefreshAttributes();
    void RefreshHistory();
    void OnTreeItemClicked(QTreeWidgetItem* pItem, int column);
    void OnTreeItemDoubleClicked(QTreeWidgetItem* pItem, int column);
    void OnStateClicked(int state);
    void OnToothContextMenu(int label, const QPoint& globalPos);

private:
    std::unique_ptr<Ui::LabelPanelView> ui;
    Observers* m_kpObservers;
    SystemStatus* m_kpSystem;
    WorkspacePresenter* m_kpPresenter;
    OdontogramWidget* m_pOdontogram;
    QButtonGroup* m_pStateGroup;
    QHash<int, QTreeWidgetItem*> m_hashItems;
    QTimer m_timerRefresh;
    bool m_bUpdating;
};
