#pragma once

#include <QVector3D>

#include <cstdint>
#include <limits>
#include <vector>

struct Mesh;

struct RayHit
{
    int m_iFace = -1;
    float m_fT = 0.0f;
    float m_fU = 0.0f;
    float m_fV = 0.0f;
};

// 삼각형 경계 볼륨 계층. 커서 아래 표면 찾기와 '보이는 면만' 가림 검사에 쓴다
class Bvh
{
public:
    Bvh();

public:
    void Build(const Mesh& mesh);
    void Clear();
    // dir 은 단위 벡터. 가장 가까운 교차점
    bool Raycast(const QVector3D& origin, const QVector3D& dir, RayHit& hit,
                 float maxT = std::numeric_limits<float>::max()) const;
    // (0, maxT) 안에 어떤 면이라도 있으면 true
    bool IsOccluded(const QVector3D& origin, const QVector3D& dir, float maxT) const;

public:
    bool IsEmpty() const;
    int GetNodeCount() const;

private:
    struct Node
    {
        float m_arrMin[3];
        float m_arrMax[3];
        // 잎: 첫 삼각형 위치, 안쪽 노드: 왼쪽 자식 (오른쪽은 +1)
        std::uint32_t m_uLeftOrFirst;
        std::uint32_t m_uCount;
    };

    template <bool AnyHit>
    bool Traverse(const QVector3D& origin, const QVector3D& dir, float maxT, RayHit& hit) const;

private:
    std::vector<Node> m_vecNodes;
    // BVH 순서로 다시 늘어놓은 삼각형 꼭짓점 (삼각형마다 9개)
    std::vector<float> m_vecTriangles;
    std::vector<std::uint32_t> m_vecFaceIds;
};
