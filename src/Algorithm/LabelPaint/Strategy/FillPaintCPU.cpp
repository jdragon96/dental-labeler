#include "Algorithm/LabelPaint/Strategy/FillPaintCPU.h"

#include "Algorithm/Geometry/MeshTopology.h"
#include "Algorithm/LabelPaint/Common/PaintPolicy.h"
#include "Document/Mesh.h"

#include <cstdint>
#include <vector>

bool FillPaintCPU::Execute(const PaintInput& in, PaintOutput& out)
{
    out.m_vecIndices.clear();
    if (in.m_kpMesh == nullptr || in.m_kpTopology == nullptr || in.m_kpTopology->IsEmpty()
        || in.m_kpLabels == nullptr || in.m_iSeedVertex < 0)
    {
        return false;
    }

    const MeshTopology& topology = *in.m_kpTopology;
    const std::vector<LabelValue>& vecLabels = *in.m_kpLabels;
    const LabelValue seedLabel = vecLabels[static_cast<size_t>(in.m_iSeedVertex)];
    if (seedLabel == in.m_target)
    {
        return true;
    }

    std::vector<char> vecVisited(vecLabels.size(), 0);
    std::vector<std::uint32_t> vecQueue;
    vecQueue.push_back(static_cast<std::uint32_t>(in.m_iSeedVertex));
    vecVisited[static_cast<size_t>(in.m_iSeedVertex)] = 1;
    for (size_t iHead = 0; iHead < vecQueue.size(); ++iHead)
    {
        const std::uint32_t uVertex = vecQueue[iHead];
        if (PaintPolicy::CanPaint(in, vecLabels[uVertex]))
        {
            out.m_vecIndices.push_back(uVertex);
        }

        const std::uint32_t* kpNeighbors = topology.GetNeighbors(static_cast<int>(uVertex));
        const int iNeighborCount = topology.GetNeighborCount(static_cast<int>(uVertex));
        for (int i = 0; i < iNeighborCount; ++i)
        {
            const std::uint32_t uNeighbor = kpNeighbors[i];
            if (vecVisited[uNeighbor] == 0 && vecLabels[uNeighbor] == seedLabel)
            {
                vecVisited[uNeighbor] = 1;
                vecQueue.push_back(uNeighbor);
            }
        }
    }
    return true;
}
