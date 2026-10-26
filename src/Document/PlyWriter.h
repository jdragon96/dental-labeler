#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <vector>

struct Mesh;

// 정점마다 붙일 int 필드 (예: fdi · label · instance)
struct PlyIntField
{
    QByteArray m_strName;
    std::vector<int> m_vecValues;
};

// 라벨 PLY 쓰기 (binary_little_endian).
// 정점: 원본 속성(Mesh::m_vecSourceColumns)을 형식 그대로 쓰고 그 뒤에 int 필드를 주어진 순서로 붙인다.
//       원본 속성이 없는 메시(다른 형식에서 읽은 것)는 float x y z 를 쓴다.
// 면: property list uchar int vertex_indices (삼각형)
class PlyWriter
{
public:
    bool Write(const QString& path, const Mesh& mesh, const QStringList& lstComments,
               const std::vector<PlyIntField>& vecFields);

public:
    const QString& GetError() const;

private:
    QString m_strError;
};
