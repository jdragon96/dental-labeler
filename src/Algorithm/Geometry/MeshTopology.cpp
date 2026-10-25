#include "Algorithm/Geometry/MeshTopology.h"

#include "Document/Mesh.h"

#include <algorithm>

MeshTopology::MeshTopology()
    : m_vecOffsets()
    , m_vecNeighbors()
{
}

void MeshTopology::Build(const Mesh& mesh)
{
    Clear();
    const int iVertexCount = mesh.GetVertexCount();
    const int iFaceCount = mesh.GetFaceCount();
    if (iVertexCount == 0 || iFaceCount == 0)
    {
        return;
    }

    // 1) 면의 변마다 양쪽 정점의 차수를 센다 (공유 변은 두 번 들어가고 아래에서 중복을 지운다)
    std::vector<std::uint32_t> vecRawOffsets(static_cast<size_t>(iVertexCount) + 1, 0);
    for (int iFace = 0; iFace < iFaceCount; ++iFace)
    {
        const std::uint32_t* kpFace = mesh.GetFace(iFace);
        for (int iCorner = 0; iCorner < 3; ++iCorner)
        {
            vecRawOffsets[kpFace[iCorner] + 1] += 2;
        }
    }
    for (size_t i = 1; i < vecRawOffsets.size(); ++i)
    {
        vecRawOffsets[i] += vecRawOffsets[i - 1];
    }

    // 2) 채운다
    std::vector<std::uint32_t> vecRaw(vecRawOffsets.back());
    std::vector<std::uint32_t> vecCursor(vecRawOffsets.begin(), vecRawOffsets.end() - 1);
    for (int iFace = 0; iFace < iFaceCount; ++iFace)
    {
        const std::uint32_t* kpFace = mesh.GetFace(iFace);
        for (int iCorner = 0; iCorner < 3; ++iCorner)
        {
            const std::uint32_t uVertex = kpFace[iCorner];
            vecRaw[vecCursor[uVertex]++] = kpFace[(iCorner + 1) % 3];
            vecRaw[vecCursor[uVertex]++] = kpFace[(iCorner + 2) % 3];
        }
    }

    // 3) 정점마다 정렬 · 중복 제거해 촘촘하게 옮긴다
    m_vecOffsets.assign(static_cast<size_t>(iVertexCount) + 1, 0);
    m_vecNeighbors.reserve(vecRaw.size() / 2 + static_cast<size_t>(iVertexCount));
    for (int iVertex = 0; iVertex < iVertexCount; ++iVertex)
    {
        auto itBegin = vecRaw.begin() + vecRawOffsets[static_cast<size_t>(iVertex)];
        auto itEnd = vecRaw.begin() + vecRawOffsets[static_cast<size_t>(iVertex) + 1];
        std::sort(itBegin, itEnd);
        itEnd = std::unique(itBegin, itEnd);
        for (auto it = itBegin; it != itEnd; ++it)
        {
            if (*it != static_cast<std::uint32_t>(iVertex))
            {
                m_vecNeighbors.push_back(*it);
            }
        }
        m_vecOffsets[static_cast<size_t>(iVertex) + 1] = static_cast<std::uint32_t>(m_vecNeighbors.size());
    }
    m_vecNeighbors.shrink_to_fit();
}

void MeshTopology::Clear()
{
    m_vecOffsets.clear();
    m_vecNeighbors.clear();
}

bool MeshTopology::IsEmpty() const
{
    return m_vecOffsets.empty();
}

int MeshTopology::GetVertexCount() const
{
    return m_vecOffsets.empty() ? 0 : static_cast<int>(m_vecOffsets.size()) - 1;
}

int MeshTopology::GetNeighborCount(int vertex) const
{
    return static_cast<int>(m_vecOffsets[static_cast<size_t>(vertex) + 1] - m_vecOffsets[static_cast<size_t>(vertex)]);
}

const std::uint32_t* MeshTopology::GetNeighbors(int vertex) const
{
    return m_vecNeighbors.data() + m_vecOffsets[static_cast<size_t>(vertex)];
}
