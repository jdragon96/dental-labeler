#pragma once

#include "Core/Types.h"
#include "Presenter/Workspace/WorkspaceTypes.h"
#include "UI/Common/OrbitCamera.h"

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPointF>
#include <QTimer>
#include <QVariantAnimation>

#include <cstdint>
#include <memory>
#include <vector>

class Observers;
class SystemStatus;
class WorkspacePresenter;

// 3D 뷰포트 (DL-20 ⑩). 메시를 그리고, 마우스 입력을 칠하기 · 카메라 조작으로 바꿔 Presenter 에 넘긴다
class Viewport3DWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    Viewport3DWidget(Observers* kpObservers, SystemStatus* kpSystem, WorkspacePresenter* kpPresenter,
                     QWidget* parent = nullptr);
    ~Viewport3DWidget() override;

public:
    void ApplyPreset(EViewPreset preset);
    void FitAll();
    void FocusLabel(int label);

signals:
    void HoverChanged(const PickResult& pick);

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    enum class EDrag
    {
        NONE,
        ORBIT,
        PAN,
        PAINT,
        LASSO,
    };

private:
    void OnCaseOpened();
    void OnLabelsChanged();
    void OnLabelStateChanged();
    void OnActiveLabelChanged(int label);
    void OnPendingDigitsChanged(const QString& digits);

    void UploadGeometry();
    void UploadIndices();
    void UploadColors();
    void ReleaseGL();

    PickResult PickAt(const QPointF& point) const;
    void PaintAlong(const QPointF& to);
    void FinishInteraction();
    float GetBrushScreenRadius(const QVector3D& point) const;
    void AnimateTargetTo(const QVector3D& target);

    void DrawBackground(QPainter& painter);
    void DrawScene();
    void DrawOverlay(QPainter& painter);
    void DrawPill(QPainter& painter, const QRectF& rect, const QString& text, const QColor& border);
    QString GetToolHint() const;
    bool IsBrushTool() const;

private:
    Observers* m_kpObservers;
    SystemStatus* m_kpSystem;
    WorkspacePresenter* m_kpPresenter;
    OrbitCamera m_camera;

    std::unique_ptr<QOpenGLShaderProgram> m_upProgram;
    QOpenGLBuffer m_bufGeometry;
    QOpenGLBuffer m_bufColors;
    QOpenGLBuffer m_bufIndices;
    bool m_bGeometryDirty;
    bool m_bIndicesDirty;
    bool m_bColorsDirty;
    bool m_bPointMode;
    bool m_bHasNormals;
    int m_iIndexCount;
    std::vector<unsigned char> m_vecColorBytes;

    EDrag m_eDrag;
    QPointF m_ptLastMouse;
    QPointF m_ptLastDab;
    bool m_bSpaceHeld;
    bool m_bHover;
    QPointF m_ptHover;
    PickResult m_hoverPick;
    std::vector<QPointF> m_vecLasso;
    bool m_bLassoErase;

    QString m_strPendingDigits;
    QString m_strFlashText;
    QTimer m_timerFlash;
    QTimer m_timerTooltip;
    bool m_bShowTooltip;
    QVariantAnimation m_animTarget;
    QVector3D m_v3AnimFrom;
    QVector3D m_v3AnimTo;
};
