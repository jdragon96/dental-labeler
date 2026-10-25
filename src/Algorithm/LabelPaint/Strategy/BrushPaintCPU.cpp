#include "Algorithm/LabelPaint/Strategy/BrushPaintCPU.h"

#include "Algorithm/Geometry/MeshTopology.h"
#include "Algorithm/LabelPaint/Common/PaintPolicy.h"
#include "Document/Mesh.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

namespace
{
constexpr float kInfinity = std::numeric_limits<float>::max();
}

BrushPaintCPU::BrushPaintCPU()
    : m_vecDistance()
    , m_vecTouched()
{
}

BrushPaintCPU::~BrushPaintCPU() = default;

bool BrushPaintCPU::Execute(const PaintInput& in, PaintOutput& out)
{
    out.m_vecIndices.clear();
    if (in.m_kpMesh == nullptr || in.m_kpLabels == nullptr || in.m_fRadius <= 0.0f)
    {
        return false;
    }
    const bool bGeodesic = in.m_kpTopology != nullptr && !in.m_kpTopology->IsEmpty()
                           && (in.m_iSeedFace >= 0 || in.m_iSeedVertex >= 0);
    return bGeodesic ? ExecuteGeodesic(in, out) : ExecuteSphere(in, out);
}

bool BrushPaintCPU::ExecuteGeodesic(const PaintInput& in, PaintOutput& out)
{
    const Mesh& mesh = *in.m_kpMesh;
    const MeshTopology& topology = *in.m_kpTopology;
    const std::vector<LabelValue>& vecLabels = *in.m_kpLabels;
    const int iVertexCount = mesh.GetVertexCount();
    if (static_cast<int>(m_vecDistance.size()) != iVertexCount)
    {
        m_vecDistance.assign(static_cast<size_t>(iVertexCount), kInfinity);
    }

    using HeapItem = std::pair<float, std::uint32_t>;
    std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> queHeap;

    std::uint32_t arrSeeds[3] = { 0, 0, 0 };
    int iSeedCount = 0;
    if (in.m_iSeedFace >= 0)
    {
        const std::uint32_t* kpFace = mesh.GetFace(in.m_iSeedFace);
        for (int i = 0; i < 3; ++i)
        {
            arrSeeds[iSeedCount++] = kpFace[i];
        }
    }
    else
    {
        arrSeeds[iSeedCount++] = static_cast<std::uint32_t>(in.m_iSeedVertex);
    }
    for (int i = 0; i < iSeedCount; ++i)
    {
        const std::uint32_t uSeed = arrSeeds[i];
        const float fDistance = (mesh.GetPosition(static_cast<int>(uSeed)) - in.m_v3SeedPoint).length();
        if (fDistance < m_vecDistance[uSeed])
        {
            if (m_vecDistance[uSeed] == kInfinity)
            {
                m_vecTouched.push_back(uSeed);
            }
            m_vecDistance[uSeed] = fDistance;
            queHeap.push({ fDistance, uSeed });
        }
    }

    const bool bVisibleOnly = in.m_eReach == EPaintReach::VISIBLE_ONLY;
    const bool bCurvatureStop = in.m_bCurvatureStop && in.m_kpCurvature != nullptr
                                && static_cast<int>(in.m_kpCurvature->size()) == iVertexCount;
    while (!queHeap.empty())
    {
        const HeapItem item = queHeap.top();
        queHeap.pop();
        const float fDistance = item.first;
        const std::uint32_t uVertex = item.second;
        if (fDistance > m_vecDistance[uVertex] || fDistance > in.m_fRadius)
        {
            continue;
        }

        const int iVertex = static_cast<int>(uVertex);
        if ((!bVisibleOnly || PaintPolicy::IsFacingCamera(in, iVertex)) && PaintPolicy::CanPaint(in, vecLabels[uVertex]))
        {
            out.m_vecIndices.push_back(uVertex);
        }

        // 오목한 고랑 정점까지는 칠하되 그 너머로는 번지지 않는다 (시작점은 예외)
        const bool bSeed = std::find(arrSeeds, arrSeeds + iSeedCount, uVertex) != arrSeeds + iSeedCount;
        if (bCurvatureStop && !bSeed && (*in.m_kpCurvature)[uVertex] < -in.m_fCurvatureThreshold)
        {
            continue;
        }

        const QVector3D v3Position = mesh.GetPosition(iVertex);
        const std::uint32_t* kpNeighbors = topology.GetNeighbors(iVertex);
        const int iNeighborCount = topology.GetNeighborCount(iVertex);
        for (int i = 0; i < iNeighborCount; ++i)
        {
            const std::uint32_t uNeighbor = kpNeighbors[i];
            const float fNext = fDistance + (mesh.GetPosition(static_cast<int>(uNeighbor)) - v3Position).length();
            if (fNext <= in.m_fRadius && fNext < m_vecDistance[uNeighbor])
            {
                if (m_vecDistance[uNeighbor] == kInfinity)
                {
                    m_vecTouched.push_back(uNeighbor);
                }
                m_vecDistance[uNeighbor] = fNext;
                queHeap.push({ fNext, uNeighbor });
            }
        }
    }

    for (std::uint32_t uVertex : m_vecTouched)
    {
        m_vecDistance[uVertex] = kInfinity;
    }
    m_vecTouched.clear();
    return true;
}

bool BrushPaintCPU::ExecuteSphere(const PaintInput& in, PaintOutput& out) const
{
    const Mesh& mesh = *in.m_kpMesh;
    const std::vector<LabelValue>& vecLabels = *in.m_kpLabels;
    const float fRadiusSquared = in.m_fRadius * in.m_fRadius;
    const bool bVisibleOnly = in.m_eReach == EPaintReach::VISIBLE_ONLY;
    const int iVertexCount = mesh.GetVertexCount();
    for (int iVertex = 0; iVertex < iVertexCount; ++iVertex)
    {
        if ((mesh.GetPosition(iVertex) - in.m_v3SeedPoint).lengthSquared() > fRadiusSquared)
        {
            continue;
        }
        if (bVisibleOnly && !PaintPolicy::IsFacingCamera(in, iVertex))
        {
            continue;
        }
        if (PaintPolicy::CanPaint(in, vecLabels[static_cast<size_t>(iVertex)]))
        {
            out.m_vecIndices.push_back(static_cast<std::uint32_t>(iVertex));
        }
    }
    return true;
}
