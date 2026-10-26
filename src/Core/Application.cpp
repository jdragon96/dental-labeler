#include "Core/Application.h"

#include "Core/Observers.h"
#include "Core/SystemStatus.h"
#include "Presenter/Workspace/WorkspacePresenter.h"
#include "UI/Skin/Default/MainWindow.h"
#include "UI/View/Workspace/Viewport3DWidget.h"

#include <QApplication>
#include <QFile>
#include <QMouseEvent>
#include <QPalette>
#include <QPixmap>
#include <QStyleFactory>
#include <QTimer>

#include <cmath>

Application::Application()
    : m_upObservers(std::make_unique<Observers>())
    , m_upSystemStatus(std::make_unique<SystemStatus>(m_upObservers.get()))
    , m_upWorkspacePresenter(std::make_unique<WorkspacePresenter>(m_upObservers.get(), m_upSystemStatus.get()))
    , m_upMainWindow()
{
    ApplyTheme();
    m_upMainWindow = std::make_unique<MainWindow>(m_upObservers.get(), m_upSystemStatus.get(), m_upWorkspacePresenter.get());
}

Application::~Application()
{
    // 창이 Presenter 시그널에 연결되어 있으므로 창부터 닫는다
    m_upMainWindow.reset();
}

void Application::Show()
{
    m_upMainWindow->resize(1440, 900);
    m_upMainWindow->show();
}

void Application::OpenFile(const QString& path)
{
    m_upMainWindow->OpenFile(path);
}

void Application::ScheduleScreenshot(const QString& path, bool demoStroke)
{
    MainWindow* pWindow = m_upMainWindow.get();
    auto saveAndQuit = [pWindow, path](int exitCode) {
        QTimer::singleShot(600, pWindow, [pWindow, path, exitCode] {
            const bool bSaved = pWindow->grab().save(path);
            qApp->exit(bSaved ? exitCode : 3);
        });
    };

    QObject::connect(m_upWorkspacePresenter.get(), &WorkspacePresenter::CaseOpened, pWindow, [this, pWindow, saveAndQuit, demoStroke] {
        QTimer::singleShot(700, pWindow, [this, saveAndQuit, demoStroke] {
            if (demoStroke)
            {
                RunDemoStrokes();
            }
            saveAndQuit(0);
        });
    });
    QObject::connect(m_upWorkspacePresenter.get(), &WorkspacePresenter::LoadFailed, pWindow, [saveAndQuit] { saveAndQuit(2); });
    // 무엇에도 걸리지 않으면 30초 뒤 끝낸다
    QTimer::singleShot(30000, pWindow, [] { qApp->exit(4); });
}

void Application::ApplyTheme()
{
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x1A, 0x1E, 0x23));
    palette.setColor(QPalette::WindowText, QColor(0xD6, 0xDC, 0xE2));
    palette.setColor(QPalette::Base, QColor(0x16, 0x19, 0x1D));
    palette.setColor(QPalette::AlternateBase, QColor(0x22, 0x27, 0x2D));
    palette.setColor(QPalette::ToolTipBase, QColor(0x0C, 0x0E, 0x11));
    palette.setColor(QPalette::ToolTipText, QColor(0xD6, 0xDC, 0xE2));
    palette.setColor(QPalette::Text, QColor(0xD6, 0xDC, 0xE2));
    palette.setColor(QPalette::Button, QColor(0x2A, 0x30, 0x37));
    palette.setColor(QPalette::ButtonText, QColor(0xD6, 0xDC, 0xE2));
    palette.setColor(QPalette::Highlight, QColor(0x5B, 0x8C, 0xFF));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, QColor(0x8D, 0xB2, 0xFF));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x5A, 0x63, 0x6D));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x5A, 0x63, 0x6D));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x5A, 0x63, 0x6D));
    QApplication::setPalette(palette);

    QFile file(QStringLiteral(":/skin/default/Default.qss"));
    if (file.open(QIODevice::ReadOnly))
    {
        qApp->setStyleSheet(QString::fromUtf8(file.readAll()));
    }
}

void Application::RunDemoStrokes()
{
    // 실제 마우스 입력 경로(뷰포트 → Presenter)를 그대로 거친다
    Viewport3DWidget* pViewport = m_upMainWindow->GetViewport();
    m_upSystemStatus->SetActiveTool(ETool::BRUSH);
    PaintSettings settings = m_upSystemStatus->GetPaintSettings(ETool::BRUSH);
    settings.m_fRadius = 2.5f;
    settings.m_eOverwrite = EOverwriteRule::UNLABELED_AND_GINGIVA;
    m_upSystemStatus->SetPaintSettings(ETool::BRUSH, settings);

    const struct
    {
        int iLabel;
        double dY;
    } arrStrokes[] = { { 0, 0.30 }, { 36, 0.48 }, { 46, 0.56 }, { 33, 0.70 } };
    const int iWidth = pViewport->width();
    const int iHeight = pViewport->height();
    for (const auto& stroke : arrStrokes)
    {
        m_upWorkspacePresenter->SelectFdi(stroke.iLabel);
        const int iY = static_cast<int>(iHeight * stroke.dY);
        const QPoint ptStart(static_cast<int>(iWidth * 0.15), iY);
        QMouseEvent press(QEvent::MouseButtonPress, ptStart, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(pViewport, &press);
        for (int i = 1; i <= 60; ++i)
        {
            const QPoint ptMove(static_cast<int>(iWidth * (0.15 + 0.7 * i / 60.0)), iY + static_cast<int>(6.0 * std::sin(i * 0.3)));
            QMouseEvent move(QEvent::MouseMove, ptMove, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(pViewport, &move);
        }
        QMouseEvent release(QEvent::MouseButtonRelease, ptStart, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(pViewport, &release);
    }
}
