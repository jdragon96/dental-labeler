#include "Algorithm/Geometry/Curvature.h"

#include "Algorithm/Geometry/MeshTopology.h"
#include "Algorithm/Geometry/ParallelFor.h"
#include "Document/Mesh.h"

#include <algorithm>

namespace Curvature
{
std::vector<float> ComputeVertexCurvature(const Mesh& mesh, const MeshTopology& topology)
{
    const int iVertexCount = mesh.GetVertexCount();
    std::vector<float> vecCurvature(static_cast<size_t>(iVertexCount), 0.0f);
    if (!mesh.HasNormals() || topology.GetVertexCount() != iVertexCount)
    {
        return vecCurvature;
    }

    ParallelFor(iVertexCount, 20000, [&](int, int begin, int end) {
        for (int iVertex = begin; iVertex < end; ++iVertex)
        {
            const QVector3D v3Position = mesh.GetPosition(iVertex);
            const QVector3D v3Normal = mesh.GetNormal(iVertex);
            const std::uint32_t* kpNeighbors = topology.GetNeighbors(iVertex);
            const int iNeighborCount = topology.GetNeighborCount(iVertex);
            float fSum = 0.0f;
            int iUsed = 0;
            for (int i = 0; i < iNeighborCount; ++i)
            {
                const QVector3D v3Edge = mesh.GetPosition(static_cast<int>(kpNeighbors[i])) - v3Position;
                const float fLengthSquared = v3Edge.lengthSquared();
                if (fLengthSquared > 1e-12f)
                {
                    // 이웃이 접평면 위(법선 쪽)에 있으면 오목하다
                    fSum += 2.0f * QVector3D::dotProduct(v3Normal, v3Edge) / fLengthSquared;
                    ++iUsed;
                }
            }
            vecCurvature[static_cast<size_t>(iVertex)] = iUsed > 0 ? -fSum / static_cast<float>(iUsed) : 0.0f;
        }
    });
    return vecCurvature;
}

float GetStopThreshold(float sensitivity)
{
    const float fSensitivity = std::clamp(sensitivity, 0.0f, 1.0f);
    // 민감도 0 → 1.6/mm 보다 오목해야 멈춤, 1 → 0.1/mm 만 오목해도 멈춤
    return 1.6f - 1.5f * fSensitivity;
}
}
