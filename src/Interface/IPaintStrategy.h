#pragma once

#include "Core/Types.h"

#include <QMatrix4x4>
#include <QPointF>
#include <QSize>
#include <QVector3D>

#include <cstdint>
#include <set>
#include <vector>

struct Mesh;
class MeshTopology;
class Bvh;

// 라벨 칠하기 전략의 입력. 어떤 정점을 target 으로 바꿀지만 정하고 문서는 건드리지 않는다
struct PaintInput
{
    const Mesh* m_kpMesh = nullptr;
    const MeshTopology* m_kpTopology = nullptr;
    const Bvh* m_kpBvh = nullptr;
    const std::vector<float>* m_kpCurvature = nullptr;
    const std::vector<LabelValue>* m_kpLabels = nullptr;
    const std::set<int>* m_kpLockedLabels = nullptr;
    const std::set<int>* m_kpHiddenLabels = nullptr;

    LabelValue m_target = kUnlabeled;
    EOverwriteRule m_eOverwrite = EOverwriteRule::UNLABELED_AND_GINGIVA;
    EPaintReach m_eReach = EPaintReach::VISIBLE_ONLY;
    QVector3D m_v3Camera;

    // 브러시 · 채우기: 커서 아래 표면
    int m_iSeedFace = -1;
    int m_iSeedVertex = -1;
    QVector3D m_v3SeedPoint;
    float m_fRadius = 2.0f;
    bool m_bCurvatureStop = false;
    float m_fCurvatureThreshold = 1.0f;

    // 올가미: 화면 좌표 다각형 (위젯 픽셀, y 아래로)
    std::vector<QPointF> m_vecPolygon;
    QMatrix4x4 m_matViewProjection;
    QSize m_viewportSize;
};

struct PaintOutput
{
    std::vector<std::uint32_t> m_vecIndices;
};

class IPaintStrategy
{
public:
    virtual ~IPaintStrategy() = default;

public:
    virtual bool Execute(const PaintInput& in, PaintOutput& out) = 0;
};
