#pragma once

#include <cstdint>
#include <vector>

struct Mesh;

// 정점 → 이웃 정점 (CSR). 측지 거리 브러시 · 채우기 · 곡률에 쓴다
class MeshTopology
{
public:
    MeshTopology();

public:
    void Build(const Mesh& mesh);
    void Clear();

public:
    bool IsEmpty() const;
    int GetVertexCount() const;
    int GetNeighborCount(int vertex) const;
    const std::uint32_t* GetNeighbors(int vertex) const;

private:
    std::vector<std::uint32_t> m_vecOffsets;
    std::vector<std::uint32_t> m_vecNeighbors;
};
