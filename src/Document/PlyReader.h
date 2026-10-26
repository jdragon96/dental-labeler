#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <map>
#include <vector>

struct Mesh;

// PLY 읽기 (ascii · binary_little_endian · binary_big_endian).
// vertex 의 x y z (+ nx ny nz) 와 face 의 vertex_indices 를 쓰고, 다른 정점 스칼라 속성은 Mesh::m_vecSourceColumns 에
// 원래 형식으로 담는다. 라벨 필드(fdi · label · instance, scalar_ 접두어 포함)는 GetField() 로 따로 준다.
// 다각형 면은 부채꼴로 삼각형으로 나눈다. face 요소가 없으면 포인트 클라우드다.
class PlyReader
{
public:
    PlyReader();

public:
    bool Read(const QString& path, Mesh& mesh);

public:
    const QString& GetError() const;
    // 헤더의 comment 줄들 (예: "Medit generated PLY File")
    QString GetComment() const;
    const QStringList& GetComments() const;
    // name: "fdi", "label", "instance" (scalar_fdi 등도 이 이름으로 담긴다)
    bool HasField(const QByteArray& name) const;
    const std::vector<int>& GetField(const QByteArray& name) const;

private:
    QString m_strError;
    QStringList m_lstComments;
    std::map<QByteArray, std::vector<int>> m_mapFields;
};
