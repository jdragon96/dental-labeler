#pragma once

#include <QByteArray>
#include <QVector3D>

#include <cstdint>
#include <vector>

// 원본 파일의 정점 속성 하나(x, red, nx …)를 원래 형식 그대로 담는다. 리틀 엔디언, 정점 수 × m_iSize 바이트
struct VertexColumn
{
    QByteArray m_strName;
    // PLY 형식 이름 (char uchar short ushort int uint float double)
    QByteArray m_strType;
    int m_iSize = 0;
    std::vector<unsigned char> m_vecData;
};

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
    // 정점 수와 위치가 모두 같은지. 라벨 파일이 이 메시의 것인지 확인할 때 쓴다
    bool IsSameGeometry(const Mesh& other) const;

public:
    std::vector<float> m_vecPositions;
    std::vector<float> m_vecNormals;
    std::vector<std::uint32_t> m_vecIndices;
    // 라벨 PLY 를 쓸 때 원본 정점 속성(x y z, 색, 법선 …)을 값 · 형식 그대로 옮겨 적는다. label 은 담지 않는다
    std::vector<VertexColumn> m_vecSourceColumns;
};
