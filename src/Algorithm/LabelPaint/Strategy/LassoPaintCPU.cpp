#include "Algorithm/LabelPaint/Strategy/LassoPaintCPU.h"

#include "Algorithm/Geometry/Bvh.h"
#include "Algorithm/Geometry/ParallelFor.h"
#include "Algorithm/LabelPaint/Common/PaintPolicy.h"
#include "Document/Mesh.h"

#include <QRectF>
#include <QVector4D>

#include <algorithm>
#include <cstdint>
#include <vector>

bool LassoPaintCPU::Execute(const PaintInput& in, PaintOutput& out)
{
    out.m_vecIndices.clear();
    if (in.m_kpMesh == nullptr || in.m_kpLabels == nullptr || in.m_vecPolygon.size() < 3
        || in.m_viewportSize.isEmpty())
    {
        return false;
    }

    const Mesh& mesh = *in.m_kpMesh;
    const std::vector<LabelValue>& vecLabels = *in.m_kpLabels;
    const int iVertexCount = mesh.GetVertexCount();
    const float fWidth = static_cast<float>(in.m_viewportSize.width());
    const float fHeight = static_cast<float>(in.m_viewportSize.height());

    // 크기 0 인 QRectF 는 united() 에서 무시되므로 직접 잰다
    double dLeft = in.m_vecPolygon.front().x();
    double dRight = dLeft;
    double dTop = in.m_vecPolygon.front().y();
    double dBottom = dTop;
    for (const QPointF& ptCorner : in.m_vecPolygon)
    {
        dLeft = std::min(dLeft, ptCorner.x());
        dRight = std::max(dRight, ptCorner.x());
        dTop = std::min(dTop, ptCorner.y());
        dBottom = std::max(dBottom, ptCorner.y());
    }
    const QRectF rcBounds(QPointF(dLeft, dTop), QPointF(dRight, dBottom));

    const bool bVisibleOnly = in.m_eReach == EPaintReach::VISIBLE_ONLY;
    const bool bOcclusionTest = bVisibleOnly && in.m_kpBvh != nullptr && !in.m_kpBvh->IsEmpty();

    constexpr int kMinChunk = 16384;
    std::vector<std::vector<std::uint32_t>> vecChunks(static_cast<size_t>(GetParallelChunkCount(iVertexCount, kMinChunk)));
    ParallelFor(iVertexCount, kMinChunk, [&](int chunk, int begin, int end) {
        std::vector<std::uint32_t>& vecPicked = vecChunks[static_cast<size_t>(chunk)];
        for (int iVertex = begin; iVertex < end; ++iVertex)
        {
            if (!PaintPolicy::CanPaint(in, vecLabels[static_cast<size_t>(iVertex)]))
            {
                continue;
            }
            const QVector3D v3Position = mesh.GetPosition(iVertex);
            const QVector4D v4Clip = in.m_matViewProjection * QVector4D(v3Position, 1.0f);
            if (v4Clip.w() <= 1e-6f)
            {
                continue;
            }
            const float fNdcX = v4Clip.x() / v4Clip.w();
            const float fNdcY = v4Clip.y() / v4Clip.w();
            const QPointF ptScreen((fNdcX * 0.5f + 0.5f) * fWidth, (0.5f - fNdcY * 0.5f) * fHeight);
            if (!rcBounds.contains(ptScreen) || !IsInsidePolygon(in.m_vecPolygon, ptScreen))
            {
                continue;
            }
            if (bVisibleOnly && !PaintPolicy::IsFacingCamera(in, iVertex))
            {
                continue;
            }
            if (bOcclusionTest)
            {
                const QVector3D v3ToVertex = v3Position - in.m_v3Camera;
                const float fDistance = v3ToVertex.length();
                // 정점 자신의 면에 걸리지 않게 조금 앞에서 멈춘다
                const float fMaxT = fDistance * (1.0f - 1e-4f) - 1e-3f;
                if (fMaxT > 0.0f && in.m_kpBvh->IsOccluded(in.m_v3Camera, v3ToVertex / fDistance, fMaxT))
                {
                    continue;
                }
            }
            vecPicked.push_back(static_cast<std::uint32_t>(iVertex));
        }
    });

    for (const std::vector<std::uint32_t>& vecPicked : vecChunks)
    {
        out.m_vecIndices.insert(out.m_vecIndices.end(), vecPicked.begin(), vecPicked.end());
    }
    return true;
}

bool LassoPaintCPU::IsInsidePolygon(const std::vector<QPointF>& vecPolygon, const QPointF& point)
{
    bool bInside = false;
    const size_t iCount = vecPolygon.size();
    for (size_t i = 0, j = iCount - 1; i < iCount; j = i++)
    {
        const QPointF& ptA = vecPolygon[i];
        const QPointF& ptB = vecPolygon[j];
        if ((ptA.y() > point.y()) != (ptB.y() > point.y()))
        {
            const double dCrossX = ptA.x() + (point.y() - ptA.y()) * (ptB.x() - ptA.x()) / (ptB.y() - ptA.y());
            if (point.x() < dCrossX)
            {
                bInside = !bInside;
            }
        }
    }
    return bInside;
}
