#include "Document/Mesh.h"

#include <algorithm>
#include <cmath>
#include <limits>

int Mesh::GetVertexCount() const
{
    return static_cast<int>(m_vecPositions.size() / 3);
}

int Mesh::GetFaceCount() const
{
    return static_cast<int>(m_vecIndices.size() / 3);
}

bool Mesh::HasFaces() const
{
    return !m_vecIndices.empty();
}

bool Mesh::HasNormals() const
{
    return !m_vecNormals.empty() && m_vecNormals.size() == m_vecPositions.size();
}

QVector3D Mesh::GetPosition(int vertex) const
{
    const size_t iOffset = static_cast<size_t>(vertex) * 3;
    return QVector3D(m_vecPositions[iOffset], m_vecPositions[iOffset + 1], m_vecPositions[iOffset + 2]);
}

QVector3D Mesh::GetNormal(int vertex) const
{
    const size_t iOffset = static_cast<size_t>(vertex) * 3;
    return QVector3D(m_vecNormals[iOffset], m_vecNormals[iOffset + 1], m_vecNormals[iOffset + 2]);
}

const std::uint32_t* Mesh::GetFace(int face) const
{
    return m_vecIndices.data() + static_cast<size_t>(face) * 3;
}

void Mesh::ComputeNormals()
{
    if (!HasFaces())
    {
        return;
    }

    m_vecNormals.assign(m_vecPositions.size(), 0.0f);
    const int iFaceCount = GetFaceCount();
    for (int iFace = 0; iFace < iFaceCount; ++iFace)
    {
        const std::uint32_t* kpFace = GetFace(iFace);
        const QVector3D v3A = GetPosition(static_cast<int>(kpFace[0]));
        const QVector3D v3B = GetPosition(static_cast<int>(kpFace[1]));
        const QVector3D v3C = GetPosition(static_cast<int>(kpFace[2]));
        // 정규화하지 않은 외적의 크기가 넓이의 두 배라서 그대로 더하면 넓이 가중이 된다
        const QVector3D v3Cross = QVector3D::crossProduct(v3B - v3A, v3C - v3A);
        for (int iCorner = 0; iCorner < 3; ++iCorner)
        {
            const size_t iOffset = static_cast<size_t>(kpFace[iCorner]) * 3;
            m_vecNormals[iOffset] += v3Cross.x();
            m_vecNormals[iOffset + 1] += v3Cross.y();
            m_vecNormals[iOffset + 2] += v3Cross.z();
        }
    }

    for (size_t i = 0; i < m_vecNormals.size(); i += 3)
    {
        const float fLength = std::sqrt(m_vecNormals[i] * m_vecNormals[i]
                                        + m_vecNormals[i + 1] * m_vecNormals[i + 1]
                                        + m_vecNormals[i + 2] * m_vecNormals[i + 2]);
        if (fLength > 1e-20f)
        {
            m_vecNormals[i] /= fLength;
            m_vecNormals[i + 1] /= fLength;
            m_vecNormals[i + 2] /= fLength;
        }
    }
}

void Mesh::GetBounds(QVector3D& v3Min, QVector3D& v3Max) const
{
    const float fMax = std::numeric_limits<float>::max();
    float arrMin[3] = { fMax, fMax, fMax };
    float arrMax[3] = { -fMax, -fMax, -fMax };
    for (size_t i = 0; i < m_vecPositions.size(); i += 3)
    {
        for (size_t iAxis = 0; iAxis < 3; ++iAxis)
        {
            arrMin[iAxis] = std::min(arrMin[iAxis], m_vecPositions[i + iAxis]);
            arrMax[iAxis] = std::max(arrMax[iAxis], m_vecPositions[i + iAxis]);
        }
    }
    if (m_vecPositions.empty())
    {
        v3Min = QVector3D();
        v3Max = QVector3D();
        return;
    }
    v3Min = QVector3D(arrMin[0], arrMin[1], arrMin[2]);
    v3Max = QVector3D(arrMax[0], arrMax[1], arrMax[2]);
}
