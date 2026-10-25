#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

// 삼각형 메시 또는 포인트 클라우드(면 없음). 단위는 원본 그대로(구강 스캔은 mm)
struct Mesh
{
public:
    int GetVertexCount() const;
    int GetFaceCount() const;
    bool HasFaces() const;
    bool HasNormals() const;

    QVector3D GetPosition(int vertex) const;
    QVector3D GetNormal(int vertex) const;
    const std::uint32_t* GetFace(int face) const;

    // 면 넓이로 가중한 정점 법선
    void ComputeNormals();
    void GetBounds(QVector3D& v3Min, QVector3D& v3Max) const;

public:
    std::vector<float> m_vecPositions;
    std::vector<float> m_vecNormals;
    std::vector<std::uint32_t> m_vecIndices;
};
