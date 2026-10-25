#pragma once

#include "Core/Types.h"

#include <QVector3D>

struct Mesh;

// 시점 프리셋(교합면 · 정면 · 좌 · 우 · 설측)에 쓰는 치열 좌표계. 메시 모양에서 추정한다
struct DentalFrame
{
    QVector3D m_v3Center;
    // 치관이 향하는 쪽
    QVector3D m_v3Occlusal;
    // 앞니 쪽
    QVector3D m_v3Anterior;
    QVector3D m_v3PatientLeft;
    // 환자 머리 쪽. 하악은 교합면 쪽, 상악은 그 반대
    QVector3D m_v3PatientUp;
    float m_fRadius = 1.0f;
    bool m_bValid = false;
};

namespace DentalFrameEstimator
{
// 주성분 분석: 분산이 가장 작은 축이 교합 방향, 나머지 두 축 중 끝으로 갈수록 폭이 확 달라지는 축이 앞뒤 방향이다
DentalFrame Compute(const Mesh& mesh, EJaw jaw);
}
