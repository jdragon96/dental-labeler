#include "TestHelpers.h"

#include <cmath>
#include <cstdint>

namespace TestHelpers
{
Mesh MakeGrid(int count, float spacing, float z)
{
    Mesh mesh;
    const float fOffset = 0.5f * spacing * static_cast<float>(count - 1);
    for (int iy = 0; iy < count; ++iy)
    {
        for (int ix = 0; ix < count; ++ix)
        {
            mesh.m_vecPositions.push_back(static_cast<float>(ix) * spacing - fOffset);
            mesh.m_vecPositions.push_back(static_cast<float>(iy) * spacing - fOffset);
            mesh.m_vecPositions.push_back(z);
        }
    }
    for (int iy = 0; iy + 1 < count; ++iy)
    {
        for (int ix = 0; ix + 1 < count; ++ix)
        {
            const std::uint32_t uA = static_cast<std::uint32_t>(iy * count + ix);
            const std::uint32_t uB = uA + 1;
            const std::uint32_t uC = uA + static_cast<std::uint32_t>(count);
            const std::uint32_t uD = uC + 1;
            // 반시계 방향이라 법선이 +Z
            mesh.m_vecIndices.insert(mesh.m_vecIndices.end(), { uA, uB, uD, uA, uD, uC });
        }
    }
    mesh.ComputeNormals();
    return mesh;
}

Mesh MakeValley(int count, float spacing, float slope)
{
    Mesh mesh = MakeGrid(count, spacing);
    for (size_t i = 0; i < mesh.m_vecPositions.size(); i += 3)
    {
        mesh.m_vecPositions[i + 2] = slope * std::fabs(mesh.m_vecPositions[i]);
    }
    mesh.ComputeNormals();
    return mesh;
}

Mesh Merge(const Mesh& first, const Mesh& second)
{
    Mesh mesh = first;
    const std::uint32_t uOffset = static_cast<std::uint32_t>(first.GetVertexCount());
    mesh.m_vecPositions.insert(mesh.m_vecPositions.end(), second.m_vecPositions.begin(), second.m_vecPositions.end());
    for (std::uint32_t uIndex : second.m_vecIndices)
    {
        mesh.m_vecIndices.push_back(uIndex + uOffset);
    }
    mesh.ComputeNormals();
    return mesh;
}

QString GetSamplePath()
{
    return QStringLiteral(DL_SAMPLE_DATA_DIR "/case_0001_lower.ply");
}
}
