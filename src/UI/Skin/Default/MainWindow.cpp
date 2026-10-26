#include "UI/Skin/Default/MainWindow.h"

#include "ui_MainWindow.h"

#include "Core/LabelTable.h"
#include "Core/Observers.h"
#include "Core/SystemStatus.h"
#include "Document/CaseData.h"
#include "Presenter/Workspace/WorkspacePresenter.h"
#include "UI/Common/LabelKeyInput.h"
#include "UI/Common/Theme/IconFactory.h"
#include "UI/View/Workspace/LabelPanelView.h"
#include "UI/View/Workspace/ToolOptionsView.h"
#include "UI/View/Workspace/ToolRailView.h"
#include "UI/View/Workspace/Viewport3DWidget.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QSlider>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

#include <cmath>

namespace
{
const QColor kIconColor(0xC4, 0xCC, 0xD4);

void PlaceInto(QWidget* pHost, QWidget* pChild)
{
    auto* pLayout = new QVBoxLayout(pHost);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(0);
    pLayout->addWidget(pChild);
}

QString FormatAge(const QDateTime& time)
{
    if (!time.isValid())
    {
        return QStringLiteral("자동 저장 전");
    }
    const qint64 llSeconds = time.secsTo(QDateTime::currentDateTime());
    if (llSeconds < 60)
    {
        return QStringLiteral("자동 저장 방금");
    }
    return QStringLiteral("자동 저장 %1분 전").arg(llSeconds / 60);
}
}

MainWindow::MainWindow(Observers* kpObservers, SystemStatus* kpSystem, WorkspacePresenter* kpPresenter, QWidget* parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::MainWindow>())
    , m_kpObservers(kpObservers)
    , m_kpSystem(kpSystem)
    , m_kpPresenter(kpPresenter)
    , m_pViewport(nullptr)
    , m_pToolRail(nullptr)
    , m_pToolOptions(nullptr)
    , m_pLabelPanel(nullptr)
    , m_pKeyInput(nullptr)
    , m_pProgress(nullptr)
    , m_strLoadingName()
    , m_pActionOpen(nullptr)
    , m_pActionSave(nullptr)
    , m_pActionUndo(nullptr)
    , m_pActionRedo(nullptr)
    , m_pDisplayGroup(nullptr)
    , m_pActionShaded(nullptr)
    , m_pActionLabel(nullptr)
    , m_pOpacitySlider(nullptr)
    , m_pOpacityLabel(nullptr)
    , m_pStatusProgress(nullptr)
    , m_pStatusProgressText(nullptr)
    , m_pStatusUnlabeled(nullptr)
    , m_pStatusHover(nullptr)
    , m_pStatusTool(nullptr)
    , m_pStatusAutosave(nullptr)
    , m_pStatusMessage(nullptr)
    , m_timerStatus()
    , m_timerMessage()
{
    ui->setupUi(this);
    CreateViews();
    CreateMenusAndActions();
    CreateToolBar();
    CreateStatusBar();

    m_pKeyInput = new LabelKeyInput(this);
    qApp->installEventFilter(m_pKeyInput);
    connect(m_pKeyInput, &LabelKeyInput::LabelEntered, m_kpPresenter, &WorkspacePresenter::SelectFdi);
    connect(m_pKeyInput, &LabelKeyInput::PendingChanged, m_kpObservers, &Observers::PendingLabelDigitsChanged);

    connect(m_kpPresenter, &WorkspacePresenter::LoadStarted, this, &MainWindow::OnLoadStarted);
    connect(m_kpPresenter, &WorkspacePresenter::LoadProgress, this, &MainWindow::OnLoadProgress);
    connect(m_kpPresenter, &WorkspacePresenter::LoadFailed, this, &MainWindow::OnLoadFailed);
    connect(m_kpPresenter, &WorkspacePresenter::CaseOpened, this, &MainWindow::OnCaseOpened);
    connect(m_kpPresenter, &WorkspacePresenter::RecoveryAvailable, this, &MainWindow::OnRecoveryAvailable);
    connect(m_kpPresenter, &WorkspacePresenter::DirtyChanged, this, &MainWindow::UpdateTitle);
    connect(m_kpPresenter, &WorkspacePresenter::HistoryChanged, this, &MainWindow::UpdateActions);
    connect(m_kpPresenter, &WorkspacePresenter::LabelsChanged, this, [this] {
        if (!m_timerStatus.isActive())
        {
            m_timerStatus.start();
        }
    });
    connect(m_kpPresenter, &WorkspacePresenter::Saved, this, [this](const QString& path) {
        ShowStatusMessage(QStringLiteral("저장했다: %1").arg(QFileInfo(path).fileName()), 4000);
    });
    connect(m_kpPresenter, &WorkspacePresenter::SaveFailed, this, [this](const QString& message) {
        QMessageBox::critical(this, QStringLiteral("저장하지 못했다"), message);
    });
    connect(m_kpPresenter, &WorkspacePresenter::AutosaveWritten, this, &MainWindow::UpdateStatus);
    connect(m_kpPresenter, &WorkspacePresenter::Message, this, [this](const QString& text) { ShowStatusMessage(text, 4000); });

    connect(m_kpObservers, &Observers::ActiveToolChanged, this, &MainWindow::UpdateStatus);
    connect(m_kpObservers, &Observers::PaintSettingsChanged, this, &MainWindow::UpdateStatus);
    connect(m_kpObservers, &Observers::DisplayModeChanged, this, [this](EDisplayMode mode) {
        (mode == EDisplayMode::LABEL ? m_pActionLabel : m_pActionShaded)->setChecked(true);
    });
    connect(m_kpObservers, &Observers::LabelOpacityChanged, this, [this](float opacity) {
        const int iPercent = static_cast<int>(std::lround(opacity * 100.0f));
        if (m_pOpacitySlider->value() != iPercent)
        {
            m_pOpacitySlider->setValue(iPercent);
        }
        m_pOpacityLabel->setText(QStringLiteral("%1%").arg(iPercent));
    });

    m_timerStatus.setSingleShot(true);
    m_timerStatus.setInterval(100);
    connect(&m_timerStatus, &QTimer::timeout, this, &MainWindow::UpdateStatus);
    auto* pClock = new QTimer(this);
    pClock->setInterval(30000);
    connect(pClock, &QTimer::timeout, this, &MainWindow::UpdateStatus);
    pClock->start();

    UpdateTitle();
    UpdateActions();
    UpdateStatus();
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(m_pKeyInput);
}

void MainWindow::OpenFile(const QString& path)
{
    if (m_kpPresenter->IsDirty()
        && !ConfirmDiscardOrSave(QStringLiteral("%1 에 저장하지 않은 변경이 있다. 다른 파일을 열기 전에 저장할까?").arg(m_kpPresenter->GetCaseId())))
    {
        return;
    }
    m_kpPresenter->OpenFile(path);
}

Viewport3DWidget* MainWindow::GetViewport() const
{
    return m_pViewport;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_kpPresenter->IsDirty()
        && !ConfirmDiscardOrSave(QStringLiteral("%1 에 저장하지 않은 변경이 있다. 닫기 전에 저장할까?").arg(m_kpPresenter->GetCaseId())))
    {
        event->ignore();
        return;
    }
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1)
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QString strPath = event->mimeData()->urls().first().toLocalFile();
    if (!strPath.isEmpty())
    {
        OpenFile(strPath);
    }
}

void MainWindow::CreateViews()
{
    m_pToolRail = new ToolRailView(m_kpObservers, m_kpSystem, ui->toolRailHost);
    PlaceInto(ui->toolRailHost, m_pToolRail);
    m_pToolOptions = new ToolOptionsView(m_kpObservers, m_kpSystem, m_kpPresenter, ui->toolOptionsHost);
    PlaceInto(ui->toolOptionsHost, m_pToolOptions);
    m_pViewport = new Viewport3DWidget(m_kpObservers, m_kpSystem, m_kpPresenter, ui->viewportHost);
    PlaceInto(ui->viewportHost, m_pViewport);
    m_pLabelPanel = new LabelPanelView(m_kpObservers, m_kpSystem, m_kpPresenter, ui->rightPanelHost);
    PlaceInto(ui->rightPanelHost, m_pLabelPanel);
    connect(m_pViewport, &Viewport3DWidget::HoverChanged, this, &MainWindow::OnHoverChanged);
}

void MainWindow::CreateMenusAndActions()
{
    QMenuBar* pMenuBar = menuBar();

    QMenu* pFile = pMenuBar->addMenu(QStringLiteral("파일(&F)"));
    m_pActionOpen = AddAction(pFile, QStringLiteral("열기…"), QKeySequence::Open, [this] { OnOpenTriggered(); });
    m_pActionOpen->setIcon(IconFactory::Create(EIcon::OPEN, kIconColor));
    m_pActionSave = AddAction(pFile, QStringLiteral("저장"), QKeySequence::Save, [this] { m_kpPresenter->Save(); });
    m_pActionSave->setIcon(IconFactory::Create(EIcon::SAVE, kIconColor));
    pFile->addSeparator();
    AddAction(pFile, QStringLiteral("종료"), QKeySequence(Qt::CTRL + Qt::Key_Q), [this] { close(); });

    QMenu* pEdit = pMenuBar->addMenu(QStringLiteral("편집(&E)"));
    m_pActionUndo = AddAction(pEdit, QStringLiteral("실행 취소"), QKeySequence::Undo, [this] { m_kpPresenter->Undo(); });
    m_pActionUndo->setIcon(IconFactory::Create(EIcon::UNDO, kIconColor));
    m_pActionRedo = AddAction(pEdit, QStringLiteral("다시 실행"), QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Z),
                              [this] { m_kpPresenter->Redo(); });
    m_pActionRedo->setShortcuts({ QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Z), QKeySequence(Qt::CTRL + Qt::Key_Y) });
    m_pActionRedo->setIcon(IconFactory::Create(EIcon::REDO, kIconColor));

    QMenu* pView = pMenuBar->addMenu(QStringLiteral("보기(&V)"));
    m_pDisplayGroup = new QActionGroup(this);
    m_pActionShaded = AddAction(pView, QStringLiteral("셰이딩"), QKeySequence(), [this] { m_kpSystem->SetDisplayMode(EDisplayMode::SHADED); });
    m_pActionLabel = AddAction(pView, QStringLiteral("라벨"), QKeySequence(), [this] { m_kpSystem->SetDisplayMode(EDisplayMode::LABEL); });
    for (QAction* pAction : { m_pActionShaded, m_pActionLabel })
    {
        pAction->setCheckable(true);
        m_pDisplayGroup->addAction(pAction);
    }
    m_pActionLabel->setChecked(m_kpSystem->GetDisplayMode() == EDisplayMode::LABEL);
    m_pActionShaded->setChecked(m_kpSystem->GetDisplayMode() == EDisplayMode::SHADED);
    AddAction(pView, QStringLiteral("라벨 색 켜기 · 끄기"), QKeySequence(Qt::Key_T), [this] {
        m_kpSystem->SetDisplayMode(m_kpSystem->GetDisplayMode() == EDisplayMode::LABEL ? EDisplayMode::SHADED : EDisplayMode::LABEL);
    });
    AddAction(pView, QStringLiteral("라벨 불투명도 낮추기"), QKeySequence(Qt::Key_Comma), [this] { AdjustOpacity(-0.1f); });
    AddAction(pView, QStringLiteral("라벨 불투명도 높이기"), QKeySequence(Qt::Key_Period), [this] { AdjustOpacity(0.1f); });
    pView->addSeparator();
    const struct
    {
        const char* kpName;
        EViewPreset ePreset;
        int iKey;
    } arrPresets[] = {
        { "교합면", EViewPreset::OCCLUSAL, Qt::Key_1 }, { "정면 (순측)", EViewPreset::FRONT, Qt::Key_2 },
        { "좌측", EViewPreset::LEFT, Qt::Key_3 },        { "우측", EViewPreset::RIGHT, Qt::Key_4 },
        { "설측", EViewPreset::LINGUAL, Qt::Key_5 },
    };
    for (const auto& preset : arrPresets)
    {
        const EViewPreset ePreset = preset.ePreset;
        AddAction(pView, QString::fromUtf8(preset.kpName), QKeySequence(Qt::CTRL + preset.iKey),
                  [this, ePreset] { emit m_kpObservers->ViewPresetRequested(ePreset); });
    }
    AddAction(pView, QStringLiteral("전체 보기"), QKeySequence(Qt::Key_Home), [this] { emit m_kpObservers->FitViewRequested(); });
    AddAction(pView, QStringLiteral("활성 치아로 확대"), QKeySequence(Qt::Key_Z),
              [this] { emit m_kpObservers->FocusLabelRequested(m_kpSystem->GetActiveLabel()); });
    pView->addSeparator();
    AddAction(pView, QStringLiteral("활성 라벨 숨기기"), QKeySequence(Qt::Key_H),
              [this] { m_kpPresenter->ToggleHidden(m_kpSystem->GetActiveLabel()); });
    AddAction(pView, QStringLiteral("모두 보이기"), QKeySequence(Qt::SHIFT + Qt::Key_H), [this] { m_kpPresenter->ShowAllLabels(); });
    AddAction(pView, QStringLiteral("활성 라벨만 보기"), QKeySequence(Qt::Key_I),
              [this] { m_kpPresenter->IsolateLabel(m_kpSystem->GetActiveLabel()); });

    QMenu* pTools = pMenuBar->addMenu(QStringLiteral("도구(&T)"));
    const struct
    {
        const char* kpName;
        ETool eTool;
        int iKey;
    } arrTools[] = {
        { "이동", ETool::NAVIGATE, Qt::Key_V }, { "브러시", ETool::BRUSH, Qt::Key_B }, { "올가미", ETool::LASSO, Qt::Key_L },
        { "채우기", ETool::FILL, Qt::Key_F },   { "지우개", ETool::ERASER, Qt::Key_E },
    };
    for (const auto& tool : arrTools)
    {
        const ETool eTool = tool.eTool;
        AddAction(pTools, QString::fromUtf8(tool.kpName), QKeySequence(tool.iKey), [this, eTool] { m_kpSystem->SetActiveTool(eTool); });
    }
    pTools->addSeparator();
    AddAction(pTools, QStringLiteral("브러시 작게"), QKeySequence(Qt::Key_BracketLeft), [this] { AdjustBrushSize(1.0f / 1.1f); });
    AddAction(pTools, QStringLiteral("브러시 크게"), QKeySequence(Qt::Key_BracketRight), [this] { AdjustBrushSize(1.1f); });

    QMenu* pLabel = pMenuBar->addMenu(QStringLiteral("라벨(&L)"));
    AddAction(pLabel, QStringLiteral("치은"), QKeySequence(Qt::Key_G), [this] { m_kpPresenter->SelectLabel(kGingiva); });
    // 클래스 바꾸기: Alt + 클래스 값 (파일의 label 값과 같다)
    for (const LabelTable::ClassInfo& info : LabelTable::GetClasses())
    {
        const int iClass = info.m_iValue;
        AddAction(pLabel, QStringLiteral("클래스: %1 (label %2)").arg(QString::fromUtf8(info.m_kpName)).arg(iClass),
                  QKeySequence(Qt::ALT + Qt::Key_0 + iClass), [this, iClass] { m_kpPresenter->SelectClass(iClass); });
    }
    pLabel->addSeparator();
    AddAction(pLabel, QStringLiteral("다음 치아"), QKeySequence(Qt::Key_Tab), [this] { m_kpPresenter->SelectAdjacentTooth(1); });
    AddAction(pLabel, QStringLiteral("이전 치아"), QKeySequence(Qt::SHIFT + Qt::Key_Backtab), [this] { m_kpPresenter->SelectAdjacentTooth(-1); })
        ->setShortcuts({ QKeySequence(Qt::SHIFT + Qt::Key_Backtab), QKeySequence(Qt::SHIFT + Qt::Key_Tab) });
    AddAction(pLabel, QStringLiteral("활성 라벨 잠금 · 해제"), QKeySequence(Qt::CTRL + Qt::Key_L),
              [this] { m_kpPresenter->ToggleLocked(m_kpSystem->GetActiveLabel()); });
    AddAction(pLabel, QStringLiteral("활성 치아 결손 표시"), QKeySequence(Qt::SHIFT + Qt::Key_M),
              [this] { m_pLabelPanel->ChangeToothState(LabelTable::GetFdiOf(m_kpSystem->GetActiveLabel()), EToothState::MISSING); });

    QMenu* pHelp = pMenuBar->addMenu(QStringLiteral("도움말(&H)"));
    AddAction(pHelp, QStringLiteral("단축키"), QKeySequence(Qt::Key_Question), [this] { ShowShortcutHelp(); });
    AddAction(pHelp, QStringLiteral("DentalLabeler 정보"), QKeySequence(), [this] { ShowAbout(); });
    AddAction(pHelp, QStringLiteral("Qt 정보"), QKeySequence(), [this] { QMessageBox::aboutQt(this); });
}

void MainWindow::CreateToolBar()
{
    QToolBar* pToolBar = ui->mainToolBar;
    pToolBar->setIconSize(QSize(16, 16));
    pToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    pToolBar->addAction(m_pActionOpen);
    pToolBar->addAction(m_pActionSave);
    pToolBar->addSeparator();
    pToolBar->addAction(m_pActionUndo);
    pToolBar->addAction(m_pActionRedo);
    pToolBar->addSeparator();
    pToolBar->addAction(m_pActionShaded);
    pToolBar->addAction(m_pActionLabel);

    m_pOpacitySlider = new QSlider(Qt::Horizontal, pToolBar);
    m_pOpacitySlider->setRange(0, 100);
    m_pOpacitySlider->setValue(static_cast<int>(std::lround(m_kpSystem->GetLabelOpacity() * 100.0f)));
    m_pOpacitySlider->setFixedWidth(80);
    m_pOpacitySlider->setToolTip(QStringLiteral("라벨 색 불투명도 ( , . )"));
    m_pOpacityLabel = new QLabel(QStringLiteral("%1%").arg(m_pOpacitySlider->value()), pToolBar);
    m_pOpacityLabel->setProperty("muted", true);
    m_pOpacityLabel->setMinimumWidth(36);
    connect(m_pOpacitySlider, &QSlider::valueChanged, this,
            [this](int value) { m_kpSystem->SetLabelOpacity(static_cast<float>(value) / 100.0f); });
    pToolBar->addWidget(m_pOpacitySlider);
    pToolBar->addWidget(m_pOpacityLabel);
    pToolBar->addSeparator();

    const struct
    {
        const char* kpName;
        EViewPreset ePreset;
    } arrPresets[] = {
        { "교합면", EViewPreset::OCCLUSAL }, { "정면", EViewPreset::FRONT }, { "좌", EViewPreset::LEFT },
        { "우", EViewPreset::RIGHT },       { "설측", EViewPreset::LINGUAL },
    };
    for (const auto& preset : arrPresets)
    {
        const EViewPreset ePreset = preset.ePreset;
        QAction* pAction = pToolBar->addAction(QString::fromUtf8(preset.kpName));
        pAction->setToolTip(QStringLiteral("%1 시점 (Ctrl+%2)").arg(QString::fromUtf8(preset.kpName)).arg(static_cast<int>(ePreset) + 1));
        connect(pAction, &QAction::triggered, this, [this, ePreset] { emit m_kpObservers->ViewPresetRequested(ePreset); });
    }
    QAction* pFit = pToolBar->addAction(IconFactory::Create(EIcon::FIT, kIconColor), QStringLiteral("전체 보기"));
    pFit->setToolTip(QStringLiteral("전체 보기 (Home)"));
    connect(pFit, &QAction::triggered, this, [this] { emit m_kpObservers->FitViewRequested(); });
}

void MainWindow::CreateStatusBar()
{
    QStatusBar* pStatus = statusBar();
    pStatus->setSizeGripEnabled(false);

    m_pStatusProgress = new QProgressBar(pStatus);
    m_pStatusProgress->setRange(0, 1000);
    m_pStatusProgress->setTextVisible(false);
    m_pStatusProgress->setFixedWidth(90);
    m_pStatusProgressText = new QLabel(pStatus);
    m_pStatusUnlabeled = new QLabel(pStatus);
    m_pStatusHover = new QLabel(pStatus);
    m_pStatusTool = new QLabel(pStatus);
    m_pStatusAutosave = new QLabel(pStatus);
    auto* pNotation = new QLabel(QStringLiteral("FDI"), pStatus);

    pStatus->addWidget(m_pStatusProgress);
    pStatus->addWidget(m_pStatusProgressText);
    pStatus->addWidget(m_pStatusUnlabeled);
    pStatus->addWidget(m_pStatusHover);
    pStatus->addWidget(m_pStatusTool);
    // QStatusBar::showMessage 는 위 칸들 위에 겹쳐 그려지므로 알림은 따로 둔 칸에 쓴다
    m_pStatusMessage = new QLabel(pStatus);
    m_pStatusMessage->setStyleSheet(QStringLiteral("color: #C9D7FF;"));
    pStatus->addWidget(m_pStatusMessage, 1);
    pStatus->addPermanentWidget(m_pStatusAutosave);
    pStatus->addPermanentWidget(pNotation);

    m_timerMessage.setSingleShot(true);
    connect(&m_timerMessage, &QTimer::timeout, m_pStatusMessage, &QLabel::clear);
}

void MainWindow::ShowStatusMessage(const QString& text, int durationMs)
{
    m_pStatusMessage->setText(text);
    m_timerMessage.stop();
    if (durationMs > 0)
    {
        m_timerMessage.start(durationMs);
    }
}

QAction* MainWindow::AddAction(QMenu* pMenu, const QString& text, const QKeySequence& shortcut, const std::function<void()>& func)
{
    QAction* pAction = pMenu->addAction(text);
    if (!shortcut.isEmpty())
    {
        pAction->setShortcut(shortcut);
        pAction->setShortcutContext(Qt::WindowShortcut);
    }
    connect(pAction, &QAction::triggered, this, func);
    // 메뉴가 닫혀 있어도 창 단축키로 동작하게 창에도 붙인다
    addAction(pAction);
    return pAction;
}

bool MainWindow::ConfirmDiscardOrSave(const QString& question)
{
    QMessageBox box(QMessageBox::Question, QStringLiteral("저장하지 않은 변경"), question, QMessageBox::NoButton, this);
    QPushButton* pDiscard = box.addButton(QStringLiteral("저장 안 함"), QMessageBox::DestructiveRole);
    box.addButton(QStringLiteral("취소"), QMessageBox::RejectRole);
    QPushButton* pSave = box.addButton(QStringLiteral("저장"), QMessageBox::AcceptRole);
    box.setDefaultButton(pSave);
    box.exec();
    if (box.clickedButton() == pSave)
    {
        return m_kpPresenter->Save();
    }
    return box.clickedButton() == pDiscard;
}

void MainWindow::OnOpenTriggered()
{
    const QString strPath = QFileDialog::getOpenFileName(this, QStringLiteral("케이스 열기"), QString(),
                                                         QStringLiteral("구강 스캔 · 라벨 파일 (*.ply);;모든 파일 (*.*)"));
    if (!strPath.isEmpty())
    {
        OpenFile(strPath);
    }
}

void MainWindow::OnLoadStarted(const QString& fileName)
{
    m_strLoadingName = fileName;
    if (m_pProgress == nullptr)
    {
        m_pProgress = new QProgressDialog(this);
        m_pProgress->setWindowTitle(QStringLiteral("여는 중"));
        m_pProgress->setWindowModality(Qt::WindowModal);
        m_pProgress->setCancelButtonText(QStringLiteral("취소"));
        m_pProgress->setRange(0, 100);
        m_pProgress->setMinimumWidth(380);
        connect(m_pProgress, &QProgressDialog::canceled, m_kpPresenter, &WorkspacePresenter::CancelLoading);
    }
    // 1초 안에 끝나면 띄우지 않는다 (DL-90 ①)
    m_pProgress->setMinimumDuration(1000);
    m_pProgress->setLabelText(QStringLiteral("%1 여는 중").arg(fileName));
    m_pProgress->setValue(0);
    ShowStatusMessage(QStringLiteral("%1 여는 중…").arg(fileName), 0);
}

void MainWindow::OnLoadProgress(const QString& step, int percent)
{
    if (m_pProgress != nullptr && !m_pProgress->wasCanceled())
    {
        m_pProgress->setLabelText(QStringLiteral("%1 여는 중\n%2").arg(m_strLoadingName, step));
        m_pProgress->setValue(std::min(percent, 99));
    }
}

void MainWindow::OnLoadFailed(const QString& message)
{
    if (m_pProgress != nullptr)
    {
        m_pProgress->reset();
    }
    ShowStatusMessage(QString(), 0);
    if (message.contains(QStringLiteral("취소")))
    {
        return;
    }
    QMessageBox::critical(this, QStringLiteral("열 수 없다"), message);
}

void MainWindow::OnCaseOpened()
{
    if (m_pProgress != nullptr)
    {
        m_pProgress->reset();
    }
    const CaseData* kpCase = m_kpPresenter->GetCase();
    QString strMessage = QStringLiteral("%1 을 열었다").arg(kpCase->m_strCaseId);
    if (kpCase->m_bLabelFileLoaded)
    {
        strMessage += QStringLiteral(" · 라벨 파일을 불러왔다");
    }
    if (kpCase->m_eJaw == EJaw::UNKNOWN)
    {
        strMessage += QStringLiteral(" · 파일 이름으로 턱을 알 수 없어 상 · 하악 번호를 모두 보인다");
    }
    ShowStatusMessage(strMessage, 6000);
    UpdateTitle();
    UpdateActions();
    UpdateStatus();
    m_pViewport->setFocus();
}

void MainWindow::OnRecoveryAvailable(const QDateTime& time, int editCount)
{
    QMessageBox box(QMessageBox::Question, QStringLiteral("지난 작업 복구"),
                    QStringLiteral("마지막 저장보다 새 자동 저장본이 있다. %1 의 %2 자동 저장본에 저장하지 않은 변경 %3건이 있다.")
                        .arg(m_kpPresenter->GetCaseId(), time.toString(QStringLiteral("HH:mm")))
                        .arg(editCount),
                    QMessageBox::NoButton, this);
    QPushButton* pDiscard = box.addButton(QStringLiteral("자동 저장본 버리기"), QMessageBox::DestructiveRole);
    box.addButton(QStringLiteral("나중에"), QMessageBox::RejectRole);
    QPushButton* pRestore = box.addButton(QStringLiteral("복구"), QMessageBox::AcceptRole);
    box.setDefaultButton(pRestore);
    box.exec();
    if (box.clickedButton() == pRestore)
    {
        m_kpPresenter->ResolveRecovery(true);
    }
    else if (box.clickedButton() == pDiscard)
    {
        m_kpPresenter->ResolveRecovery(false);
    }
}

void MainWindow::OnHoverChanged(const PickResult& pick)
{
    if (!pick.m_bHit)
    {
        m_pStatusHover->clear();
        return;
    }
    m_pStatusHover->setText(QStringLiteral("커서 정점 #%L1 · %2 · 곡률 %3")
                                .arg(pick.m_iVertex)
                                .arg(pick.m_label == kUnlabeled ? QStringLiteral("미분류") : QString::number(pick.m_label))
                                .arg(static_cast<double>(pick.m_fCurvature), 0, 'f', 2));
}

void MainWindow::AdjustBrushSize(float factor)
{
    PaintSettings settings = m_kpSystem->GetPaintSettings(ETool::BRUSH);
    settings.m_fRadius *= factor;
    m_kpSystem->SetPaintSettings(ETool::BRUSH, settings);
}

void MainWindow::AdjustOpacity(float delta)
{
    m_kpSystem->SetLabelOpacity(m_kpSystem->GetLabelOpacity() + delta);
}

void MainWindow::ShowShortcutHelp()
{
    const QString strHtml = QStringLiteral(
        "<table cellspacing='4'>"
        "<tr><td colspan='2'><b>도구</b></td></tr>"
        "<tr><td>V B L F E</td><td>이동 · 브러시 · 올가미 · 채우기 · 지우개</td></tr>"
        "<tr><td>[ ]  ·  Shift+휠</td><td>브러시 크기</td></tr>"
        "<tr><td>Ctrl+드래그</td><td>반대 동작 (지우기)</td></tr>"
        "<tr><td>Alt+클릭</td><td>스포이드: 클릭한 정점의 라벨을 활성 라벨로</td></tr>"
        "<tr><td colspan='2'><b>라벨</b></td></tr>"
        "<tr><td>숫자 두 개</td><td>FDI 번호 (예: 3 다음 6 → 36, 0.8초 안)</td></tr>"
        "<tr><td>0 · G</td><td>치은</td></tr>"
        "<tr><td>Alt+0 · Alt+1 · Alt+2</td><td>클래스: 치은 · 치아 · 스캔바디 (예: Alt+2 다음 3 6 → 36 자리 스캔바디)</td></tr>"
        "<tr><td>Tab · Shift+Tab</td><td>아치 순서로 다음 · 이전 치아</td></tr>"
        "<tr><td>Ctrl+L · Shift+M</td><td>잠금 · 결손 표시</td></tr>"
        "<tr><td colspan='2'><b>보기</b></td></tr>"
        "<tr><td>오른쪽 드래그 · 가운데 드래그 · 휠</td><td>회전 · 이동 · 확대</td></tr>"
        "<tr><td>Space+왼쪽 드래그</td><td>임시 회전</td></tr>"
        "<tr><td>더블클릭</td><td>회전 중심 옮기기</td></tr>"
        "<tr><td>Ctrl+1 … 5 · Home · Z</td><td>교합면 · 정면 · 좌 · 우 · 설측 · 전체 · 활성 치아</td></tr>"
        "<tr><td>T · , .</td><td>라벨 색 켜기 · 불투명도</td></tr>"
        "<tr><td>H · Shift+H · I</td><td>숨기기 · 모두 보이기 · 활성 라벨만</td></tr>"
        "<tr><td colspan='2'><b>파일</b></td></tr>"
        "<tr><td>Ctrl+O · Ctrl+S</td><td>열기 · 저장</td></tr>"
        "<tr><td>Ctrl+Z · Ctrl+Shift+Z</td><td>실행 취소 · 다시 실행</td></tr>"
        "</table>");
    QMessageBox box(QMessageBox::NoIcon, QStringLiteral("단축키"), strHtml, QMessageBox::Close, this);
    box.setTextFormat(Qt::RichText);
    box.exec();
}

void MainWindow::ShowAbout()
{
    QMessageBox::about(this, QStringLiteral("DentalLabeler 정보"),
                       QStringLiteral("<b>DentalLabeler %1</b><br>구강 스캔 메시 · 포인트 클라우드 치아 분할 라벨링<br><br>"
                                      "Qt %2 를 LGPLv3 로 동적 링크해 쓴다. 'Qt 정보'에서 Qt 라이선스를 볼 수 있다.")
                           .arg(QApplication::applicationVersion(), QString::fromLatin1(qVersion())));
}

void MainWindow::UpdateTitle()
{
    const QString strCase = m_kpPresenter->GetCaseId();
    if (strCase.isEmpty())
    {
        setWindowTitle(QStringLiteral("DentalLabeler"));
        return;
    }
    setWindowTitle(QStringLiteral("%1%2 — DentalLabeler").arg(strCase, m_kpPresenter->IsDirty() ? QStringLiteral(" ●") : QString()));
}

void MainWindow::UpdateStatus()
{
    const CaseData* kpCase = m_kpPresenter->GetCase();
    const bool bHasCase = kpCase != nullptr;
    m_pStatusProgress->setVisible(bHasCase);
    m_pStatusProgressText->setVisible(bHasCase);
    m_pStatusUnlabeled->setVisible(bHasCase);
    if (bHasCase)
    {
        const double dProgress = kpCase->m_document.GetProgress();
        m_pStatusProgress->setValue(static_cast<int>(dProgress * 1000.0));
        m_pStatusProgressText->setText(QStringLiteral("라벨 %1%").arg(dProgress * 100.0, 0, 'f', 1));
        m_pStatusUnlabeled->setText(QStringLiteral("미분류 %L1").arg(kpCase->m_document.GetCount(kUnlabeled)));
        m_pStatusAutosave->setText(FormatAge(m_kpPresenter->GetLastAutosave()));
    }
    else
    {
        m_pStatusAutosave->clear();
    }

    const PaintSettings& settings = m_kpSystem->GetPaintSettings(m_kpSystem->GetActiveTool());
    const QString strReach = settings.m_eReach == EPaintReach::VISIBLE_ONLY ? QStringLiteral("보이는 면만") : QStringLiteral("관통");
    switch (m_kpSystem->GetActiveTool())
    {
    case ETool::BRUSH:
    case ETool::ERASER:
        m_pStatusTool->setText(QStringLiteral("브러시 %1 mm · %2").arg(static_cast<double>(settings.m_fRadius), 0, 'f', 1).arg(strReach));
        break;
    case ETool::LASSO:
        m_pStatusTool->setText(QStringLiteral("올가미 · %1").arg(strReach));
        break;
    case ETool::FILL:
        m_pStatusTool->setText(QStringLiteral("채우기"));
        break;
    case ETool::NAVIGATE:
        m_pStatusTool->setText(QStringLiteral("이동"));
        break;
    }
}

void MainWindow::UpdateActions()
{
    m_pActionSave->setEnabled(m_kpPresenter->HasCase());
    m_pActionUndo->setEnabled(m_kpPresenter->CanUndo());
    m_pActionRedo->setEnabled(m_kpPresenter->CanRedo());
}
