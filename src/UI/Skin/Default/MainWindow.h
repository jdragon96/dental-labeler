#pragma once

#include "Presenter/Workspace/WorkspaceTypes.h"

#include <QKeySequence>
#include <QMainWindow>
#include <QTimer>

#include <functional>
#include <memory>

namespace Ui
{
class MainWindow;
}

class QAction;
class QActionGroup;
class QLabel;
class QMenu;
class QProgressBar;
class QProgressDialog;
class QSlider;
class LabelKeyInput;
class LabelPanelView;
class Observers;
class SystemStatus;
class ToolOptionsView;
class ToolRailView;
class Viewport3DWidget;
class WorkspacePresenter;

// 기본 시안의 작업 창. 배치는 MainWindow.ui, 뷰는 UI/View 에서 가져와 끼운다 (docs/Qt.md 3.3)
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Observers* kpObservers, SystemStatus* kpSystem, WorkspacePresenter* kpPresenter, QWidget* parent = nullptr);
    ~MainWindow() override;

public:
    // 저장 안 된 변경이 있으면 먼저 묻는다
    void OpenFile(const QString& path);
    Viewport3DWidget* GetViewport() const;

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void CreateViews();
    void CreateMenusAndActions();
    void CreateToolBar();
    void CreateStatusBar();
    QAction* AddAction(QMenu* pMenu, const QString& text, const QKeySequence& shortcut, const std::function<void()>& func);

    bool ConfirmDiscardOrSave(const QString& question);
    void OnOpenTriggered();
    void OnLoadStarted(const QString& fileName);
    void OnLoadProgress(const QString& step, int percent);
    void OnLoadFailed(const QString& message);
    void OnCaseOpened();
    void OnRecoveryAvailable(const QDateTime& time, int editCount);
    void OnHoverChanged(const PickResult& pick);
    void ShowStatusMessage(const QString& text, int durationMs);
    void AdjustBrushSize(float factor);
    void AdjustOpacity(float delta);
    void ShowShortcutHelp();
    void ShowAbout();
    void UpdateTitle();
    void UpdateStatus();
    void UpdateActions();

private:
    std::unique_ptr<Ui::MainWindow> ui;
    Observers* m_kpObservers;
    SystemStatus* m_kpSystem;
    WorkspacePresenter* m_kpPresenter;

    Viewport3DWidget* m_pViewport;
    ToolRailView* m_pToolRail;
    ToolOptionsView* m_pToolOptions;
    LabelPanelView* m_pLabelPanel;
    LabelKeyInput* m_pKeyInput;
    QProgressDialog* m_pProgress;
    QString m_strLoadingName;

    QAction* m_pActionOpen;
    QAction* m_pActionSave;
    QAction* m_pActionUndo;
    QAction* m_pActionRedo;
    QActionGroup* m_pDisplayGroup;
    QAction* m_pActionShaded;
    QAction* m_pActionLabel;
    QSlider* m_pOpacitySlider;
    QLabel* m_pOpacityLabel;

    QProgressBar* m_pStatusProgress;
    QLabel* m_pStatusProgressText;
    QLabel* m_pStatusUnlabeled;
    QLabel* m_pStatusHover;
    QLabel* m_pStatusTool;
    QLabel* m_pStatusAutosave;
    QLabel* m_pStatusMessage;
    QTimer m_timerStatus;
    QTimer m_timerMessage;
};
