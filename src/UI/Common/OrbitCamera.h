#pragma once

#include <QMatrix4x4>
#include <QPointF>
#include <QQuaternion>
#include <QSize>
#include <QVector3D>

// 회전 중심(target)을 도는 원근 카메라. 회전은 화면 기준으로 자유롭게 돈다
class OrbitCamera
{
public:
    OrbitCamera();

public:
    void Frame(const QVector3D& center, float radius);
    void FitBox(const QVector3D& v3Min, const QVector3D& v3Max);
    // toCamera: 회전 중심에서 카메라 쪽 방향, up: 화면 위쪽
    void SetView(const QVector3D& toCamera, const QVector3D& up);
    void Orbit(float dxPixels, float dyPixels);
    void Pan(float dxPixels, float dyPixels, int viewportHeight);
    // steps > 0 이면 anchor 쪽으로 다가간다. anchor 는 화면에서 제자리에 머문다
    void Zoom(float steps, const QVector3D& anchor);
    void SetTarget(const QVector3D& target);

public:
    QVector3D GetTarget() const;
    QVector3D GetPosition() const;
    QVector3D GetForward() const;
    float GetDistance() const;
    float GetSceneRadius() const;
    QMatrix4x4 GetViewMatrix() const;
    QMatrix4x4 GetProjectionMatrix(float aspect) const;
    void GetRay(const QPointF& pixel, const QSize& viewport, QVector3D& origin, QVector3D& dir) const;
    // 화면 세로 1픽셀이 차지하는 시야각 (라디안)
    float GetPixelAngle(int viewportHeight) const;
    // 카메라에서 depth 만큼 떨어진 곳에서 화면 1픽셀의 실제 길이
    float GetWorldPerPixel(float depth, int viewportHeight) const;

private:
    QVector3D m_v3Target;
    QQuaternion m_qRotation;
    float m_fDistance;
    float m_fFieldOfView;
    float m_fSceneRadius;
};
