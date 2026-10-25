#include "UI/View/Workspace/Viewport3DWidget.h"

#include "Core/LabelTable.h"
#include "Core/Observers.h"
#include "Core/SystemStatus.h"
#include "Document/CaseData.h"
#include "Presenter/Workspace/WorkspacePresenter.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr int kAttributePosition = 0;
constexpr int kAttributeNormal = 1;
constexpr int kAttributeColor = 2;
constexpr unsigned int kGlProgramPointSize = 0x8642;

// GLSL ES 1.00 과 데스크톱 GLSL 1.10 양쪽에서 돈다. 데스크톱에서는 Qt 가 정밀도 한정자를 정의해 준다
const char* kVertexShader = R"(
attribute highp vec3 aPosition;
attribute mediump vec3 aNormal;
attribute lowp vec4 aColor;
uniform highp mat4 uMvp;
uniform highp mat4 uModelView;
uniform mediump mat3 uNormalMatrix;
uniform mediump float uPointSize;
varying mediump vec3 vNormal;
varying highp vec3 vViewPosition;
varying lowp vec4 vColor;
void main()
{
    vViewPosition = (uModelView * vec4(aPosition, 1.0)).xyz;
    vNormal = uNormalMatrix * aNormal;
    vColor = aColor;
    gl_PointSize = uPointSize;
    gl_Position = uMvp * vec4(aPosition, 1.0);
}
)";

const char* kFragmentShader = R"(
#ifdef GL_ES
precision mediump float;
#endif
varying mediump vec3 vNormal;
varying highp vec3 vViewPosition;
varying lowp vec4 vColor;
uniform mediump vec3 uBaseColor;
uniform mediump float uOpacity;
uniform mediump float uHasNormals;
void main()
{
    vec3 color = mix(uBaseColor, vColor.rgb, vColor.a * uOpacity);
    float light = 1.0;
    float specular = 0.0;
    if (uHasNormals > 0.5)
    {
        vec3 normal = normalize(vNormal);
        if (!gl_FrontFacing)
        {
            normal = -normal;
        }
        vec3 toEye = normalize(-vViewPosition);
        float diffuse = max(dot(normal, toEye), 0.0);
        light = 0.30 + 0.70 * diffuse;
        specular = pow(diffuse, 48.0) * 0.16;
    }
    vec3 outColor = color * light + vec3(specular);
    if (!gl_FrontFacing)
    {
        outColor *= 0.55;
    }
    gl_FragColor = vec4(outColor, 1.0);
}
)";

std::array<std::array<unsigned char, 4>, 50> BuildPalette()
{
    std::array<std::array<unsigned char, 4>, 50> arrPalette = {};
    for (int iLabel = -1; iLabel <= 48; ++iLabel)
    {
        std::array<unsigned char, 4>& arrColor = arrPalette[static_cast<size_t>(iLabel + 1)];
        if (!LabelTable::IsValidLabel(iLabel) || iLabel == kUnlabeled)
        {
            arrColor = { 0, 0, 0, 0 };
            continue;
        }
        const QColor colLabel = LabelTable::GetColor(iLabel);
        arrColor = { static_cast<unsigned char>(colLabel.red()), static_cast<unsigned char>(colLabel.green()),
                     static_cast<unsigned char>(colLabel.blue()), 255 };
    }
    return arrPalette;
}
}

Viewport3DWidget::Viewport3DWidget(Observers* kpObservers, SystemStatus* kpSystem, WorkspacePresenter* kpPresenter,
                                   QWidget* parent)
    : QOpenGLWidget(parent)
    , m_kpObservers(kpObservers)
    , m_kpSystem(kpSystem)
    , m_kpPresenter(kpPresenter)
    , m_camera()
    , m_upProgram()
    , m_bufGeometry(QOpenGLBuffer::VertexBuffer)
    , m_bufColors(QOpenGLBuffer::VertexBuffer)
    , m_bufIndices(QOpenGLBuffer::IndexBuffer)
    , m_bGeometryDirty(false)
    , m_bIndicesDirty(false)
    , m_bColorsDirty(false)
    , m_bPointMode(false)
    , m_bHasNormals(false)
    , m_iIndexCount(0)
    , m_vecColorBytes()
    , m_eDrag(EDrag::NONE)
    , m_ptLastMouse()
    , m_ptLastDab()
    , m_bSpaceHeld(false)
    , m_bHover(false)
    , m_ptHover()
    , m_hoverPick()
    , m_vecLasso()
    , m_bLassoErase(false)
    , m_strPendingDigits()
    , m_strFlashText()
    , m_timerFlash()
    , m_timerTooltip()
    , m_bShowTooltip(false)
    , m_animTarget()
    , m_v3AnimFrom()
    , m_v3AnimTo()
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 240);

    m_timerFlash.setSingleShot(true);
    m_timerFlash.setInterval(900);
    connect(&m_timerFlash, &QTimer::timeout, this, [this] {
        m_strFlashText.clear();
        update();
    });
    m_timerTooltip.setSingleShot(true);
    m_timerTooltip.setInterval(400);
    connect(&m_timerTooltip, &QTimer::timeout, this, [this] {
        m_bShowTooltip = true;
        update();
    });

    m_animTarget.setDuration(300);
    m_animTarget.setStartValue(0.0);
    m_animTarget.setEndValue(1.0);
    m_animTarget.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_animTarget, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        const float fT = value.toFloat();
        m_camera.SetTarget(m_v3AnimFrom + (m_v3AnimTo - m_v3AnimFrom) * fT);
        update();
    });

    connect(m_kpPresenter, &WorkspacePresenter::CaseOpened, this, &Viewport3DWidget::OnCaseOpened);
    connect(m_kpPresenter, &WorkspacePresenter::LabelsChanged, this, &Viewport3DWidget::OnLabelsChanged);
    connect(m_kpPresenter, &WorkspacePresenter::LabelStateChanged, this, &Viewport3DWidget::OnLabelStateChanged);
    connect(m_kpPresenter, &WorkspacePresenter::LoadStarted, this, [this] { update(); });
    connect(m_kpPresenter, &WorkspacePresenter::LoadFailed, this, [this] { update(); });

    connect(m_kpObservers, &Observers::ActiveLabelChanged, this, &Viewport3DWidget::OnActiveLabelChanged);
    connect(m_kpObservers, &Observers::ActiveToolChanged, this, [this] { update(); });
    connect(m_kpObservers, &Observers::DisplayModeChanged, this, [this] { update(); });
    connect(m_kpObservers, &Observers::LabelOpacityChanged, this, [this] { update(); });
    connect(m_kpObservers, &Observers::PaintSettingsChanged, this, [this] { update(); });
    connect(m_kpObservers, &Observers::PendingLabelDigitsChanged, this, &Viewport3DWidget::OnPendingDigitsChanged);
    connect(m_kpObservers, &Observers::ViewPresetRequested, this, &Viewport3DWidget::ApplyPreset);
    connect(m_kpObservers, &Observers::FitViewRequested, this, &Viewport3DWidget::FitAll);
    connect(m_kpObservers, &Observers::FocusLabelRequested, this, &Viewport3DWidget::FocusLabel);
}

Viewport3DWidget::~Viewport3DWidget()
{
    if (context() != nullptr)
    {
        makeCurrent();
        ReleaseGL();
        doneCurrent();
    }
}

void Viewport3DWidget::ApplyPreset(EViewPreset preset)
{
    const CaseData* kpCase = m_kpPresenter->GetCase();
    if (kpCase == nullptr)
    {
        return;
    }

    DentalFrame frame = kpCase->m_frame;
    if (!frame.m_bValid)
    {
        frame.m_v3Occlusal = QVector3D(0.0f, 0.0f, 1.0f);
        frame.m_v3Anterior = QVector3D(0.0f, -1.0f, 0.0f);
        frame.m_v3PatientUp = frame.m_v3Occlusal;
        frame.m_v3PatientLeft = QVector3D::crossProduct(frame.m_v3PatientUp, frame.m_v3Anterior);
    }

    QVector3D v3ToCamera;
    QVector3D v3Up = frame.m_v3PatientUp;
    switch (preset)
    {
    case EViewPreset::OCCLUSAL:
        // 앞니가 화면 아래, 환자 왼쪽이 화면 오른쪽 (치식 표기와 같은 방향)
        v3ToCamera = frame.m_v3Occlusal;
        v3Up = -frame.m_v3Anterior;
        break;
    case EViewPreset::FRONT:
        v3ToCamera = frame.m_v3Anterior;
        break;
    case EViewPreset::LEFT:
        v3ToCamera = frame.m_v3PatientLeft;
        break;
    case EViewPreset::RIGHT:
        v3ToCamera = -frame.m_v3PatientLeft;
        break;
    case EViewPreset::LINGUAL:
        // 혀 쪽에서 비스듬히 내려다본다
        v3ToCamera = (-frame.m_v3Anterior + frame.m_v3Occlusal * 0.8f).normalized();
        break;
    }

    QVector3D v3Min;
    QVector3D v3Max;
    kpCase->m_mesh.GetBounds(v3Min, v3Max);
    m_animTarget.stop();
    m_camera.Frame(0.5f * (v3Min + v3Max), std::max(0.5f * (v3Max - v3Min).length(), 1.0f) * 0.9f);
    m_camera.SetView(v3ToCamera, v3Up);
    update();
}

void Viewport3DWidget::FitAll()
{
    const CaseData* kpCase = m_kpPresenter->GetCase();
    if (kpCase == nullptr)
    {
        return;
    }
    QVector3D v3Min;
    QVector3D v3Max;
    kpCase->m_mesh.GetBounds(v3Min, v3Max);
    m_animTarget.stop();
    m_camera.Frame(0.5f * (v3Min + v3Max), std::max(0.5f * (v3Max - v3Min).length(), 1.0f) * 0.9f);
    update();
}

void Viewport3DWidget::FocusLabel(int label)
{
    QVector3D v3Min;
    QVector3D v3Max;
    if (!m_kpPresenter->GetLabelBounds(label, v3Min, v3Max))
    {
        return;
    }
    m_animTarget.stop();
    m_camera.FitBox(v3Min, v3Max);
    update();
}

void Viewport3DWidget::initializeGL()
{
    initializeOpenGLFunctions();
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &Viewport3DWidget::ReleaseGL, Qt::UniqueConnection);

    m_upProgram = std::make_unique<QOpenGLShaderProgram>();
    m_upProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
    m_upProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader);
    m_upProgram->bindAttributeLocation("aPosition", kAttributePosition);
    m_upProgram->bindAttributeLocation("aNormal", kAttributeNormal);
    m_upProgram->bindAttributeLocation("aColor", kAttributeColor);
    if (!m_upProgram->link())
    {
        qWarning("셰이더 링크 실패: %s", qPrintable(m_upProgram->log()));
        m_upProgram.reset();
    }

    if (context()->isOpenGLES() && !context()->hasExtension(QByteArrayLiteral("GL_OES_element_index_uint")))
    {
        qWarning("GL_OES_element_index_uint 가 없어 큰 메시를 그리지 못할 수 있다");
    }

    m_bGeometryDirty = m_kpPresenter->HasCase();
    m_bIndicesDirty = m_bGeometryDirty;
    m_bColorsDirty = m_bGeometryDirty;
}

void Viewport3DWidget::resizeGL(int, int)
{
}

void Viewport3DWidget::paintGL()
{
    QPainter painter(this);
    DrawBackground(painter);
    painter.beginNativePainting();
    DrawScene();
    painter.endNativePainting();
    DrawOverlay(painter);
}

void Viewport3DWidget::mousePressEvent(QMouseEvent* event)
{
    setFocus(Qt::MouseFocusReason);
    const QPointF ptMouse = event->localPos();
    m_ptLastMouse = ptMouse;
    if (!m_kpPresenter->HasCase() || m_eDrag != EDrag::NONE)
    {
        return;
    }
    m_animTarget.stop();

    const bool bCtrl = (event->modifiers() & Qt::ControlModifier) != 0;
    const bool bAlt = (event->modifiers() & Qt::AltModifier) != 0;
    const bool bShift = (event->modifiers() & Qt::ShiftModifier) != 0;
    if (event->button() == Qt::RightButton)
    {
        m_eDrag = bShift ? EDrag::PAN : EDrag::ORBIT;
        return;
    }
    if (event->button() == Qt::MiddleButton)
    {
        m_eDrag = EDrag::PAN;
        return;
    }
    if (event->button() != Qt::LeftButton)
    {
        return;
    }
    if (m_bSpaceHeld)
    {
        m_eDrag = EDrag::ORBIT;
        return;
    }

    const ETool eTool = m_kpSystem->GetActiveTool();
    if (bAlt && eTool != ETool::NAVIGATE)
    {
        const PickResult pick = PickAt(ptMouse);
        if (pick.m_bHit)
        {
            m_kpPresenter->PickLabelFromVertex(pick.m_iVertex);
        }
        return;
    }

    switch (eTool)
    {
    case ETool::NAVIGATE:
        m_eDrag = EDrag::ORBIT;
        break;
    case ETool::BRUSH:
    case ETool::ERASER:
    {
        // 메시 밖에서 시작해도 획은 시작하고, 표면 위를 지날 때부터 칠한다
        m_kpPresenter->BeginStroke(eTool == ETool::ERASER || bCtrl);
        const PickResult pick = PickAt(ptMouse);
        if (pick.m_bHit)
        {
            m_kpPresenter->PaintDab(pick, m_camera.GetPosition());
        }
        m_ptLastDab = ptMouse;
        m_eDrag = EDrag::PAINT;
        break;
    }
    case ETool::FILL:
    {
        const PickResult pick = PickAt(ptMouse);
        if (pick.m_bHit)
        {
            m_kpPresenter->ApplyFill(pick, m_camera.GetPosition(), bCtrl);
        }
        break;
    }
    case ETool::LASSO:
        m_vecLasso.clear();
        m_vecLasso.push_back(ptMouse);
        m_bLassoErase = bCtrl;
        m_eDrag = EDrag::LASSO;
        break;
    }
    update();
}

void Viewport3DWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF ptMouse = event->localPos();
    const QPointF ptDelta = ptMouse - m_ptLastMouse;
    m_ptLastMouse = ptMouse;

    switch (m_eDrag)
    {
    case EDrag::ORBIT:
        m_camera.Orbit(static_cast<float>(ptDelta.x()), static_cast<float>(ptDelta.y()));
        break;
    case EDrag::PAN:
        m_camera.Pan(static_cast<float>(ptDelta.x()), static_cast<float>(ptDelta.y()), height());
        break;
    case EDrag::PAINT:
        PaintAlong(ptMouse);
        break;
    case EDrag::LASSO:
        if (m_vecLasso.empty() || QLineF(m_vecLasso.back(), ptMouse).length() > 2.0)
        {
            m_vecLasso.push_back(ptMouse);
        }
        break;
    case EDrag::NONE:
        break;
    }

    m_bHover = true;
    m_ptHover = ptMouse;
    if (m_eDrag == EDrag::NONE || m_eDrag == EDrag::PAINT)
    {
        m_hoverPick = PickAt(ptMouse);
        emit HoverChanged(m_hoverPick);
    }
    m_bShowTooltip = false;
    m_timerTooltip.start();
    update();
}

void Viewport3DWidget::mouseReleaseEvent(QMouseEvent*)
{
    FinishInteraction();
}

void Viewport3DWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_kpPresenter->HasCase())
    {
        return;
    }
    const PickResult pick = PickAt(event->localPos());
    if (pick.m_bHit)
    {
        AnimateTargetTo(pick.m_v3Point);
    }
}

void Viewport3DWidget::wheelEvent(QWheelEvent* event)
{
    if (!m_kpPresenter->HasCase())
    {
        return;
    }
    const QPoint ptAngle = event->angleDelta();
    const float fSteps = static_cast<float>(ptAngle.y() != 0 ? ptAngle.y() : ptAngle.x()) / 120.0f;
    if ((event->modifiers() & Qt::ShiftModifier) != 0)
    {
        PaintSettings settings = m_kpSystem->GetPaintSettings(ETool::BRUSH);
        settings.m_fRadius *= std::pow(1.1f, fSteps);
        m_kpSystem->SetPaintSettings(ETool::BRUSH, settings);
        return;
    }

    m_animTarget.stop();
    const PickResult pick = PickAt(event->position());
    m_camera.Zoom(fSteps, pick.m_bHit ? pick.m_v3Point : m_camera.GetTarget());
    m_hoverPick = PickAt(event->position());
    update();
}

void Viewport3DWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat())
    {
        m_bSpaceHeld = true;
        setCursor(Qt::OpenHandCursor);
        return;
    }
    if (event->key() == Qt::Key_Escape && m_eDrag == EDrag::LASSO)
    {
        m_vecLasso.clear();
        m_eDrag = EDrag::NONE;
        update();
        return;
    }
    QOpenGLWidget::keyPressEvent(event);
}

void Viewport3DWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat())
    {
        m_bSpaceHeld = false;
        unsetCursor();
        return;
    }
    QOpenGLWidget::keyReleaseEvent(event);
}

void Viewport3DWidget::leaveEvent(QEvent* event)
{
    m_bHover = false;
    m_bShowTooltip = false;
    m_timerTooltip.stop();
    update();
    QOpenGLWidget::leaveEvent(event);
}

void Viewport3DWidget::focusOutEvent(QFocusEvent* event)
{
    m_bSpaceHeld = false;
    unsetCursor();
    FinishInteraction();
    QOpenGLWidget::focusOutEvent(event);
}

void Viewport3DWidget::OnCaseOpened()
{
    m_bGeometryDirty = true;
    m_bIndicesDirty = true;
    m_bColorsDirty = true;
    m_hoverPick = PickResult();
    ApplyPreset(EViewPreset::OCCLUSAL);
}

void Viewport3DWidget::OnLabelsChanged()
{
    m_bColorsDirty = true;
    const CaseData* kpCase = m_kpPresenter->GetCase();
    if (kpCase != nullptr && !kpCase->m_document.GetHiddenLabels().empty())
    {
        m_bIndicesDirty = true;
    }
    update();
}

void Viewport3DWidget::OnLabelStateChanged()
{
    m_bIndicesDirty = true;
    update();
}

void Viewport3DWidget::OnActiveLabelChanged(int label)
{
    m_strFlashText = QStringLiteral("%1 · %2").arg(label).arg(LabelTable::GetName(label));
    m_timerFlash.start();
    update();
}

void Viewport3DWidget::OnPendingDigitsChanged(const QString& digits)
{
    m_strPendingDigits = digits;
    update();
}

void Viewport3DWidget::UploadGeometry()
{
    m_bGeometryDirty = false;
    const CaseData* kpCase = m_kpPresenter->GetCase();
    if (kpCase == nullptr)
    {
        return;
    }
    const Mesh& mesh = kpCase->m_mesh;
    const int iVertexCount = mesh.GetVertexCount();
    m_bPointMode = !mesh.HasFaces();
    m_bHasNormals = mesh.HasNormals();

    std::vector<float> vecInterleaved(static_cast<size_t>(iVertexCount) * 6, 0.0f);
    for (int i = 0; i < iVertexCount; ++i)
    {
        const size_t iSource = static_cast<size_t>(i) * 3;
        const size_t iTarget = static_cast<size_t>(i) * 6;
        vecInterleaved[iTarget] = mesh.m_vecPositions[iSource];
        vecInterleaved[iTarget + 1] = mesh.m_vecPositions[iSource + 1];
        vecInterleaved[iTarget + 2] = mesh.m_vecPositions[iSource + 2];
        if (m_bHasNormals)
        {
            vecInterleaved[iTarget + 3] = mesh.m_vecNormals[iSource];
            vecInterleaved[iTarget + 4] = mesh.m_vecNormals[iSource + 1];
            vecInterleaved[iTarget + 5] = mesh.m_vecNormals[iSource + 2];
        }
    }

    if (!m_bufGeometry.isCreated())
    {
        m_bufGeometry.create();
    }
    m_bufGeometry.bind();
    m_bufGeometry.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_bufGeometry.allocate(vecInterleaved.data(), static_cast<int>(vecInterleaved.size() * sizeof(float)));
    m_bufGeometry.release();

    m_vecColorBytes.assign(static_cast<size_t>(iVertexCount) * 4, 0);
    if (!m_bufColors.isCreated())
    {
        m_bufColors.create();
    }
    m_bufColors.bind();
    m_bufColors.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_bufColors.allocate(static_cast<int>(m_vecColorBytes.size()));
    m_bufColors.release();

    m_bColorsDirty = true;
    m_bIndicesDirty = true;
}

void Viewport3DWidget::UploadIndices()
{
    m_bIndicesDirty = false;
    const CaseData* kpCase = m_kpPresenter->GetCase();
    if (kpCase == nullptr)
    {
        return;
    }
    const Mesh& mesh = kpCase->m_mesh;
    const LabelDocument& document = kpCase->m_document;
    const std::set<int>& setHidden = document.GetHiddenLabels();
    const std::vector<LabelValue>& vecLabels = document.GetLabels();

    // 숨긴 라벨의 정점이 낀 면은 그리지 않는다
    std::vector<std::uint32_t> vecIndices;
    if (mesh.HasFaces())
    {
        if (setHidden.empty())
        {
            vecIndices = mesh.m_vecIndices;
        }
        else
        {
            vecIndices.reserve(mesh.m_vecIndices.size());
            for (size_t i = 0; i < mesh.m_vecIndices.size(); i += 3)
            {
                const bool bHidden = setHidden.count(vecLabels[mesh.m_vecIndices[i]]) > 0
                                     || setHidden.count(vecLabels[mesh.m_vecIndices[i + 1]]) > 0
                                     || setHidden.count(vecLabels[mesh.m_vecIndices[i + 2]]) > 0;
                if (!bHidden)
                {
                    vecIndices.insert(vecIndices.end(), mesh.m_vecIndices.begin() + static_cast<long>(i),
                                      mesh.m_vecIndices.begin() + static_cast<long>(i) + 3);
                }
            }
        }
    }
    else
    {
        vecIndices.reserve(vecLabels.size());
        for (size_t i = 0; i < vecLabels.size(); ++i)
        {
            if (setHidden.empty() || setHidden.count(vecLabels[i]) == 0)
            {
                vecIndices.push_back(static_cast<std::uint32_t>(i));
            }
        }
    }

    if (!m_bufIndices.isCreated())
    {
        m_bufIndices.create();
    }
    m_bufIndices.bind();
    m_bufIndices.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_bufIndices.allocate(vecIndices.data(), static_cast<int>(vecIndices.size() * sizeof(std::uint32_t)));
    m_bufIndices.release();
    m_iIndexCount = static_cast<int>(vecIndices.size());
}

void Viewport3DWidget::UploadColors()
{
    m_bColorsDirty = false;
    const CaseData* kpCase = m_kpPresenter->GetCase();
    if (kpCase == nullptr || !m_bufColors.isCreated())
    {
        return;
    }
    static const std::array<std::array<unsigned char, 4>, 50> arrPalette = BuildPalette();
    const std::vector<LabelValue>& vecLabels = kpCase->m_document.GetLabels();
    if (m_vecColorBytes.size() != vecLabels.size() * 4)
    {
        return;
    }
    for (size_t i = 0; i < vecLabels.size(); ++i)
    {
        const std::array<unsigned char, 4>& arrColor = arrPalette[static_cast<size_t>(vecLabels[i] + 1)];
        std::copy(arrColor.begin(), arrColor.end(), m_vecColorBytes.begin() + static_cast<long>(i * 4));
    }
    m_bufColors.bind();
    m_bufColors.write(0, m_vecColorBytes.data(), static_cast<int>(m_vecColorBytes.size()));
    m_bufColors.release();
}

void Viewport3DWidget::ReleaseGL()
{
    m_upProgram.reset();
    m_bufGeometry.destroy();
    m_bufColors.destroy();
    m_bufIndices.destroy();
    m_iIndexCount = 0;
}

PickResult Viewport3DWidget::PickAt(const QPointF& point) const
{
    QVector3D v3Origin;
    QVector3D v3Direction;
    m_camera.GetRay(point, size(), v3Origin, v3Direction);
    return m_kpPresenter->Pick(v3Origin, v3Direction, m_camera.GetPixelAngle(height()));
}

void Viewport3DWidget::PaintAlong(const QPointF& to)
{
    // 빠르게 움직여도 틈이 생기지 않게 브러시 반지름의 절반 간격으로 찍는다
    const float fStep = std::max(2.0f, GetBrushScreenRadius(m_hoverPick.m_bHit ? m_hoverPick.m_v3Point : m_camera.GetTarget()) * 0.5f);
    const QPointF ptDelta = to - m_ptLastDab;
    const float fLength = static_cast<float>(std::hypot(ptDelta.x(), ptDelta.y()));
    if (fLength < fStep)
    {
        return;
    }
    const int iSteps = static_cast<int>(std::ceil(fLength / fStep));
    const QVector3D v3Camera = m_camera.GetPosition();
    for (int i = 1; i <= iSteps; ++i)
    {
        const QPointF ptDab = m_ptLastDab + ptDelta * (static_cast<double>(i) / iSteps);
        const PickResult pick = PickAt(ptDab);
        if (pick.m_bHit)
        {
            m_kpPresenter->PaintDab(pick, v3Camera);
        }
    }
    m_ptLastDab = to;
}

void Viewport3DWidget::FinishInteraction()
{
    if (m_eDrag == EDrag::PAINT)
    {
        m_kpPresenter->EndStroke();
    }
    else if (m_eDrag == EDrag::LASSO)
    {
        if (m_vecLasso.size() >= 3)
        {
            const float fAspect = static_cast<float>(width()) / static_cast<float>(std::max(1, height()));
            const QMatrix4x4 matViewProjection = m_camera.GetProjectionMatrix(fAspect) * m_camera.GetViewMatrix();
            m_kpPresenter->ApplyLasso(m_vecLasso, matViewProjection, size(), m_camera.GetPosition(), m_bLassoErase);
        }
        m_vecLasso.clear();
    }
    m_eDrag = EDrag::NONE;
    update();
}

float Viewport3DWidget::GetBrushScreenRadius(const QVector3D& point) const
{
    const float fRadius = m_kpSystem->GetPaintSettings(ETool::BRUSH).m_fRadius;
    const float fDepth = std::max(QVector3D::dotProduct(point - m_camera.GetPosition(), m_camera.GetForward()), 1e-3f);
    return fRadius / m_camera.GetWorldPerPixel(fDepth, height());
}

void Viewport3DWidget::AnimateTargetTo(const QVector3D& target)
{
    m_animTarget.stop();
    m_v3AnimFrom = m_camera.GetTarget();
    m_v3AnimTo = target;
    m_animTarget.start();
}

void Viewport3DWidget::DrawBackground(QPainter& painter)
{
    QRadialGradient gradient(QPointF(width() * 0.5, height() * 0.42), std::max(width(), height()) * 0.75);
    gradient.setColorAt(0.0, QColor(0x32, 0x3A, 0x43));
    gradient.setColorAt(1.0, QColor(0x13, 0x16, 0x1A));
    painter.fillRect(rect(), gradient);
}

void Viewport3DWidget::DrawScene()
{
    const qreal dRatio = devicePixelRatioF();
    glViewport(0, 0, static_cast<GLsizei>(width() * dRatio), static_cast<GLsizei>(height() * dRatio));
    glClear(GL_DEPTH_BUFFER_BIT);
    if (!m_kpPresenter->HasCase() || !m_upProgram)
    {
        return;
    }
    if (m_bGeometryDirty)
    {
        UploadGeometry();
    }
    if (m_bIndicesDirty)
    {
        UploadIndices();
    }
    if (m_bColorsDirty)
    {
        UploadColors();
    }
    if (m_iIndexCount == 0)
    {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    if (!context()->isOpenGLES())
    {
        glEnable(kGlProgramPointSize);
    }

    const float fAspect = static_cast<float>(width()) / static_cast<float>(std::max(1, height()));
    const QMatrix4x4 matView = m_camera.GetViewMatrix();
    const QMatrix4x4 matProjection = m_camera.GetProjectionMatrix(fAspect);
    const bool bLabelMode = m_kpSystem->GetDisplayMode() == EDisplayMode::LABEL;

    m_upProgram->bind();
    m_upProgram->setUniformValue("uMvp", matProjection * matView);
    m_upProgram->setUniformValue("uModelView", matView);
    m_upProgram->setUniformValue("uNormalMatrix", matView.normalMatrix());
    m_upProgram->setUniformValue("uBaseColor", QVector3D(0.80f, 0.78f, 0.75f));
    m_upProgram->setUniformValue("uOpacity", bLabelMode ? m_kpSystem->GetLabelOpacity() : 0.0f);
    m_upProgram->setUniformValue("uHasNormals", m_bHasNormals ? 1.0f : 0.0f);
    m_upProgram->setUniformValue("uPointSize", static_cast<float>(2.5 * dRatio));

    m_bufGeometry.bind();
    m_upProgram->enableAttributeArray(kAttributePosition);
    m_upProgram->setAttributeBuffer(kAttributePosition, GL_FLOAT, 0, 3, 6 * sizeof(float));
    m_upProgram->enableAttributeArray(kAttributeNormal);
    m_upProgram->setAttributeBuffer(kAttributeNormal, GL_FLOAT, 3 * sizeof(float), 3, 6 * sizeof(float));
    m_bufColors.bind();
    m_upProgram->enableAttributeArray(kAttributeColor);
    m_upProgram->setAttributeBuffer(kAttributeColor, GL_UNSIGNED_BYTE, 0, 4, 0);
    m_bufIndices.bind();

    glDrawElements(m_bPointMode ? GL_POINTS : GL_TRIANGLES, m_iIndexCount, GL_UNSIGNED_INT, nullptr);

    m_bufIndices.release();
    m_upProgram->disableAttributeArray(kAttributeColor);
    m_upProgram->disableAttributeArray(kAttributeNormal);
    m_upProgram->disableAttributeArray(kAttributePosition);
    m_bufColors.release();
    m_upProgram->release();
    glDisable(GL_DEPTH_TEST);
}

void Viewport3DWidget::DrawOverlay(QPainter& painter)
{
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor colMuted(0x8B, 0x96, 0xA1);
    const QColor colText(0xD6, 0xDC, 0xE2);
    QFont fontBase = font();
    fontBase.setPixelSize(12);
    painter.setFont(fontBase);

    const CaseData* kpCase = m_kpPresenter->GetCase();
    if (kpCase == nullptr)
    {
        painter.setPen(colMuted);
        const QString strMessage = m_kpPresenter->IsLoading()
                                       ? QStringLiteral("여는 중…")
                                       : QStringLiteral("PLY 파일을 여기에 끌어다 놓거나 Ctrl+O 로 연다");
        const QRectF rcDrop = QRectF(rect()).adjusted(width() * 0.2, height() * 0.38, -width() * 0.2, -height() * 0.38);
        painter.setPen(QPen(QColor(0x4A, 0x53, 0x5D), 1.5, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rcDrop, 10, 10);
        painter.setPen(colMuted);
        painter.drawText(rcDrop, Qt::AlignCenter, strMessage);
        return;
    }

    // 왼쪽 위: 케이스 정보
    const Mesh& mesh = kpCase->m_mesh;
    painter.setPen(colText);
    QFont fontBold = fontBase;
    fontBold.setBold(true);
    painter.setFont(fontBold);
    painter.drawText(QPointF(12, 22), kpCase->m_strCaseId);
    painter.setFont(fontBase);
    painter.setPen(colMuted);
    const QString strKind = mesh.HasFaces() ? QStringLiteral("메시") : QStringLiteral("포인트 클라우드");
    painter.drawText(QPointF(12, 40), QStringLiteral("%1 · %2").arg(LabelTable::GetJawName(kpCase->m_eJaw), strKind));
    const QString strCounts = mesh.HasFaces()
                                  ? QStringLiteral("정점 %L1 · 면 %L2 · mm").arg(mesh.GetVertexCount()).arg(mesh.GetFaceCount())
                                  : QStringLiteral("점 %L1 · mm").arg(mesh.GetVertexCount());
    painter.drawText(QPointF(12, 56), strCounts);

    // 위 가운데: 숫자 입력 중 / 방금 고른 라벨
    if (!m_strPendingDigits.isEmpty() || !m_strFlashText.isEmpty())
    {
        const QString strText = !m_strPendingDigits.isEmpty()
                                    ? QStringLiteral("라벨 입력  %1_   두 번째 숫자를 기다리는 중").arg(m_strPendingDigits)
                                    : m_strFlashText;
        const int iTextWidth = painter.fontMetrics().horizontalAdvance(strText) + 28;
        DrawPill(painter, QRectF((width() - iTextWidth) * 0.5, 12, iTextWidth, 30), strText, QColor(0x5B, 0x8C, 0xFF));
    }

    // 브러시 커서: 표면에 닿은 실제 반경
    if (IsBrushTool() && m_bHover && m_hoverPick.m_bHit && m_eDrag != EDrag::ORBIT && m_eDrag != EDrag::PAN && !m_bSpaceHeld)
    {
        const float fRadius = GetBrushScreenRadius(m_hoverPick.m_v3Point);
        const bool bErase = m_kpSystem->GetActiveTool() == ETool::ERASER;
        const QColor colLabel = bErase ? QColor(Qt::white) : LabelTable::GetColor(m_kpSystem->GetActiveLabel());
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255, 200), 3.0));
        painter.drawEllipse(m_ptHover, fRadius, fRadius);
        painter.setPen(QPen(colLabel, 1.6, bErase ? Qt::DashLine : Qt::SolidLine));
        painter.drawEllipse(m_ptHover, fRadius, fRadius);
        painter.setPen(QPen(Qt::white, 1.2));
        painter.drawLine(m_ptHover + QPointF(-4, 0), m_ptHover + QPointF(4, 0));
        painter.drawLine(m_ptHover + QPointF(0, -4), m_ptHover + QPointF(0, 4));
    }

    // 올가미
    if (m_eDrag == EDrag::LASSO && m_vecLasso.size() > 1)
    {
        QPolygonF polygon;
        for (const QPointF& ptCorner : m_vecLasso)
        {
            polygon << ptCorner;
        }
        const QColor colLabel = m_bLassoErase ? QColor(Qt::white) : LabelTable::GetColor(m_kpSystem->GetActiveLabel());
        QColor colFill = colLabel;
        colFill.setAlpha(40);
        painter.setBrush(colFill);
        painter.setPen(QPen(colLabel, 1.6, Qt::DashLine));
        painter.drawPolygon(polygon);
    }

    // 머무르면 정점 정보
    if (m_bShowTooltip && m_bHover && m_hoverPick.m_bHit && m_eDrag == EDrag::NONE)
    {
        const QString strTip = QStringLiteral("정점 #%L1 · %2 %3 · 곡률 %4")
                                   .arg(m_hoverPick.m_iVertex)
                                   .arg(m_hoverPick.m_label == kUnlabeled ? QString() : QString::number(m_hoverPick.m_label))
                                   .arg(LabelTable::GetName(m_hoverPick.m_label))
                                   .arg(static_cast<double>(m_hoverPick.m_fCurvature), 0, 'f', 2);
        const int iTextWidth = painter.fontMetrics().horizontalAdvance(strTip) + 18;
        QRectF rcTip(m_ptHover.x() + 18, m_ptHover.y() + 18, iTextWidth, 24);
        if (rcTip.right() > width() - 8)
        {
            rcTip.moveRight(m_ptHover.x() - 18);
        }
        DrawPill(painter, rcTip, strTip, QColor(0x3A, 0x42, 0x4B));
    }

    // 아래 가운데: 도구 조작 힌트
    const QString strHint = GetToolHint();
    const int iHintWidth = painter.fontMetrics().horizontalAdvance(strHint) + 32;
    painter.setPen(colMuted);
    DrawPill(painter, QRectF((width() - iHintWidth) * 0.5, height() - 42, iHintWidth, 28), strHint, QColor(0x3A, 0x42, 0x4B));
}

void Viewport3DWidget::DrawPill(QPainter& painter, const QRectF& rect, const QString& text, const QColor& border)
{
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(QColor(12, 14, 17, 220));
    painter.drawRoundedRect(rect, rect.height() * 0.5, rect.height() * 0.5);
    painter.setPen(QColor(0xD6, 0xDC, 0xE2));
    painter.drawText(rect, Qt::AlignCenter, text);
}

QString Viewport3DWidget::GetToolHint() const
{
    switch (m_kpSystem->GetActiveTool())
    {
    case ETool::NAVIGATE:
        return QStringLiteral("왼쪽 드래그 회전 · 가운데 드래그 이동 · 휠 확대 · 더블클릭 회전 중심");
    case ETool::BRUSH:
        return QStringLiteral("왼쪽 드래그 칠하기 · Ctrl+드래그 지우기 · [ ] 크기 · 오른쪽 드래그 회전 · 휠 확대");
    case ETool::ERASER:
        return QStringLiteral("왼쪽 드래그 지우기 · [ ] 크기 · 오른쪽 드래그 회전 · 휠 확대");
    case ETool::FILL:
        return QStringLiteral("클릭 같은 라벨 영역 채우기 · Ctrl+클릭 영역 지우기 · Alt+클릭 스포이드");
    case ETool::LASSO:
        return QStringLiteral("왼쪽 드래그로 둘러싸 칠하기 · Ctrl+드래그 지우기 · Esc 취소");
    }
    return QString();
}

bool Viewport3DWidget::IsBrushTool() const
{
    const ETool eTool = m_kpSystem->GetActiveTool();
    return eTool == ETool::BRUSH || eTool == ETool::ERASER;
}
