#pragma once

#include <QString>

#include <memory>

class MainWindow;
class Observers;
class SystemStatus;
class WorkspacePresenter;

// 앱 조립 지점 (docs/Qt.md 2). 주요 모듈을 만들어 들고 창에 넘긴다
class Application
{
public:
    Application();
    ~Application();

public:
    void Show();
    void OpenFile(const QString& path);
    // 케이스가 열리면 창을 그림 파일로 저장하고 끝낸다 (화면 확인용). demoStroke 면 브러시 획을 몇 번 그어 본다
    void ScheduleScreenshot(const QString& path, bool demoStroke);

private:
    void ApplyTheme();
    void RunDemoStrokes();

private:
    std::unique_ptr<Observers> m_upObservers;
    std::unique_ptr<SystemStatus> m_upSystemStatus;
    std::unique_ptr<WorkspacePresenter> m_upWorkspacePresenter;
    std::unique_ptr<MainWindow> m_upMainWindow;
};
