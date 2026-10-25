#include "UI/Common/OrbitCamera.h"

#include <QVector4D>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kDegreesPerPixel = 0.35f;
}

OrbitCamera::OrbitCamera()
    : m_v3Target()
    , m_qRotation()
    , m_fDistance(100.0f)
    , m_fFieldOfView(30.0f)
    , m_fSceneRadius(50.0f)
{
}

void OrbitCamera::Frame(const QVector3D& center, float radius)
{
    m_fSceneRadius = std::max(radius, 1e-3f);
    m_v3Target = center;
    // 구가 세로 시야에 여유 있게 들어오는 거리
    m_fDistance = m_fSceneRadius / std::sin(qDegreesToRadians(m_fFieldOfView * 0.5f)) * 1.05f;
}

void OrbitCamera::FitBox(const QVector3D& v3Min, const QVector3D& v3Max)
{
    const float fRadius = std::max(0.5f * (v3Max - v3Min).length(), 1.0f);
    m_v3Target = 0.5f * (v3Min + v3Max);
    m_fDistance = fRadius / std::sin(qDegreesToRadians(m_fFieldOfView * 0.5f)) * 1.2f;
}

void OrbitCamera::SetView(const QVector3D& toCamera, const QVector3D& up)
{
    const QVector3D v3Z = toCamera.normalized();
    QVector3D v3X = QVector3D::crossProduct(up, v3Z);
    if (v3X.lengthSquared() < 1e-8f)
    {
        v3X = QVector3D::crossProduct(QVector3D(0.0f, 0.0f, 1.0f), v3Z);
        if (v3X.lengthSquared() < 1e-8f)
        {
            v3X = QVector3D(1.0f, 0.0f, 0.0f);
        }
    }
    v3X.normalize();
    const QVector3D v3Y = QVector3D::crossProduct(v3Z, v3X);
    m_qRotation = QQuaternion::fromAxes(v3X, v3Y, v3Z);
}

void OrbitCamera::Orbit(float dxPixels, float dyPixels)
{
    const QQuaternion qYaw = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, -dxPixels * kDegreesPerPixel);
    const QQuaternion qPitch = QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, -dyPixels * kDegreesPerPixel);
    m_qRotation = (m_qRotation * qYaw * qPitch).normalized();
}

void OrbitCamera::Pan(float dxPixels, float dyPixels, int viewportHeight)
{
    const float fScale = GetWorldPerPixel(m_fDistance, viewportHeight);
    const QVector3D v3Right = m_qRotation.rotatedVector(QVector3D(1.0f, 0.0f, 0.0f));
    const QVector3D v3Up = m_qRotation.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f));
    m_v3Target += -v3Right * dxPixels * fScale + v3Up * dyPixels * fScale;
}

void OrbitCamera::Zoom(float steps, const QVector3D& anchor)
{
    const float fWanted = m_fDistance * std::pow(0.85f, steps);
    const float fClamped = std::clamp(fWanted, m_fSceneRadius * 0.02f, m_fSceneRadius * 30.0f);
    const float fFactor = fClamped / m_fDistance;
    m_v3Target = anchor + (m_v3Target - anchor) * fFactor;
    m_fDistance = fClamped;
}

void OrbitCamera::SetTarget(const QVector3D& target)
{
    m_v3Target = target;
}

QVector3D OrbitCamera::GetTarget() const
{
    return m_v3Target;
}

QVector3D OrbitCamera::GetPosition() const
{
    return m_v3Target + m_qRotation.rotatedVector(QVector3D(0.0f, 0.0f, m_fDistance));
}

QVector3D OrbitCamera::GetForward() const
{
    return m_qRotation.rotatedVector(QVector3D(0.0f, 0.0f, -1.0f));
}

float OrbitCamera::GetDistance() const
{
    return m_fDistance;
}

float OrbitCamera::GetSceneRadius() const
{
    return m_fSceneRadius;
}

QMatrix4x4 OrbitCamera::GetViewMatrix() const
{
    QMatrix4x4 matView;
    matView.lookAt(GetPosition(), m_v3Target, m_qRotation.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f)));
    return matView;
}

QMatrix4x4 OrbitCamera::GetProjectionMatrix(float aspect) const
{
    const float fNear = std::max(m_fDistance - m_fSceneRadius * 2.0f, m_fDistance * 0.02f);
    const float fFar = m_fDistance + m_fSceneRadius * 3.0f;
    QMatrix4x4 matProjection;
    matProjection.perspective(m_fFieldOfView, std::max(aspect, 1e-3f), fNear, fFar);
    return matProjection;
}

void OrbitCamera::GetRay(const QPointF& pixel, const QSize& viewport, QVector3D& origin, QVector3D& dir) const
{
    const float fAspect = static_cast<float>(viewport.width()) / static_cast<float>(std::max(1, viewport.height()));
    const QMatrix4x4 matInverse = (GetProjectionMatrix(fAspect) * GetViewMatrix()).inverted();
    const float fNdcX = static_cast<float>(2.0 * pixel.x() / std::max(1, viewport.width()) - 1.0);
    const float fNdcY = static_cast<float>(1.0 - 2.0 * pixel.y() / std::max(1, viewport.height()));
    const QVector4D v4Far = matInverse * QVector4D(fNdcX, fNdcY, 1.0f, 1.0f);
    origin = GetPosition();
    dir = (v4Far.toVector3DAffine() - origin).normalized();
}

float OrbitCamera::GetPixelAngle(int viewportHeight) const
{
    return qDegreesToRadians(m_fFieldOfView) / static_cast<float>(std::max(1, viewportHeight));
}

float OrbitCamera::GetWorldPerPixel(float depth, int viewportHeight) const
{
    return 2.0f * depth * std::tan(qDegreesToRadians(m_fFieldOfView * 0.5f)) / static_cast<float>(std::max(1, viewportHeight));
}
