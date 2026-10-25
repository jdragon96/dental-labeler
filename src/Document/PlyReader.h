#pragma once

#include <QString>

struct Mesh;

// PLY 읽기 (ascii · binary_little_endian · binary_big_endian).
// vertex 의 x y z (+ nx ny nz) 와 face 의 vertex_indices 만 쓰고 나머지 속성 · 요소는 건너뛴다.
// 다각형 면은 부채꼴로 삼각형으로 나눈다. face 요소가 없으면 포인트 클라우드다.
class PlyReader
{
public:
    PlyReader();

public:
    bool Read(const QString& path, Mesh& mesh);

public:
    const QString& GetError() const;
    // 헤더의 comment 줄 (예: "Medit generated PLY File")
    const QString& GetComment() const;

private:
    QString m_strError;
    QString m_strComment;
};
