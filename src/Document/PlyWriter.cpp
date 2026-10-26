#include "Document/PlyWriter.h"

#include "Document/Mesh.h"

#include <QByteArray>
#include <QSaveFile>
#include <QtEndian>

#include <cstring>

namespace
{
VertexColumn MakeFloatColumn(const char* kpName, const std::vector<float>& vecValues, int component, int vertexCount)
{
    VertexColumn column;
    column.m_strName = kpName;
    column.m_strType = "float";
    column.m_iSize = 4;
    column.m_vecData.resize(static_cast<size_t>(vertexCount) * 4);
    for (int i = 0; i < vertexCount; ++i)
    {
        quint32 uBits = 0;
        std::memcpy(&uBits, &vecValues[static_cast<size_t>(i) * 3 + static_cast<size_t>(component)], sizeof(uBits));
        qToLittleEndian(uBits, column.m_vecData.data() + static_cast<size_t>(i) * 4);
    }
    return column;
}

// PLY 헤더는 줄 단위라 comment 안의 줄바꿈은 공백으로 바꾼다
QByteArray SanitizeComment(const QString& comment)
{
    QByteArray strLine = comment.toUtf8();
    strLine.replace('\r', ' ');
    strLine.replace('\n', ' ');
    return strLine;
}
}

bool PlyWriter::Write(const QString& path, const Mesh& mesh, const QStringList& lstComments,
                      const std::vector<PlyIntField>& vecFields)
{
    m_strError.clear();
    const int iVertexCount = mesh.GetVertexCount();
    const int iFaceCount = mesh.GetFaceCount();
    for (const PlyIntField& field : vecFields)
    {
        if (static_cast<int>(field.m_vecValues.size()) != iVertexCount)
        {
            m_strError = QStringLiteral("%1 값 수(%L2)가 정점 수(%L3)와 다르다.")
                             .arg(QString::fromLatin1(field.m_strName))
                             .arg(field.m_vecValues.size())
                             .arg(iVertexCount);
            return false;
        }
    }

    std::vector<VertexColumn> vecFallback;
    const std::vector<VertexColumn>* kpColumns = &mesh.m_vecSourceColumns;
    if (kpColumns->empty())
    {
        vecFallback.push_back(MakeFloatColumn("x", mesh.m_vecPositions, 0, iVertexCount));
        vecFallback.push_back(MakeFloatColumn("y", mesh.m_vecPositions, 1, iVertexCount));
        vecFallback.push_back(MakeFloatColumn("z", mesh.m_vecPositions, 2, iVertexCount));
        // 메시의 법선은 면에서 다시 계산하므로, 면이 없는 포인트 클라우드만 법선을 남긴다
        if (!mesh.HasFaces() && mesh.HasNormals())
        {
            vecFallback.push_back(MakeFloatColumn("nx", mesh.m_vecNormals, 0, iVertexCount));
            vecFallback.push_back(MakeFloatColumn("ny", mesh.m_vecNormals, 1, iVertexCount));
            vecFallback.push_back(MakeFloatColumn("nz", mesh.m_vecNormals, 2, iVertexCount));
        }
        kpColumns = &vecFallback;
    }

    size_t iStride = sizeof(qint32) * vecFields.size();
    for (const VertexColumn& column : *kpColumns)
    {
        if (column.m_iSize <= 0 || column.m_vecData.size() != static_cast<size_t>(iVertexCount) * static_cast<size_t>(column.m_iSize))
        {
            m_strError = QStringLiteral("정점 속성 %1 의 크기가 맞지 않는다.").arg(QString::fromLatin1(column.m_strName));
            return false;
        }
        iStride += static_cast<size_t>(column.m_iSize);
    }

    QByteArray header("ply\nformat binary_little_endian 1.0\n");
    for (const QString& comment : lstComments)
    {
        header += "comment " + SanitizeComment(comment) + '\n';
    }
    header += "element vertex " + QByteArray::number(iVertexCount) + '\n';
    for (const VertexColumn& column : *kpColumns)
    {
        header += "property " + column.m_strType + ' ' + column.m_strName + '\n';
    }
    for (const PlyIntField& field : vecFields)
    {
        header += "property int " + field.m_strName + '\n';
    }
    if (iFaceCount > 0)
    {
        header += "element face " + QByteArray::number(iFaceCount) + '\n';
        header += "property list uchar int vertex_indices\n";
    }
    header += "end_header\n";

    QByteArray body;
    body.resize(static_cast<int>(static_cast<size_t>(iVertexCount) * iStride + static_cast<size_t>(iFaceCount) * 13));
    unsigned char* pCursor = reinterpret_cast<unsigned char*>(body.data());
    for (int iVertex = 0; iVertex < iVertexCount; ++iVertex)
    {
        for (const VertexColumn& column : *kpColumns)
        {
            const size_t iSize = static_cast<size_t>(column.m_iSize);
            std::memcpy(pCursor, column.m_vecData.data() + static_cast<size_t>(iVertex) * iSize, iSize);
            pCursor += iSize;
        }
        for (const PlyIntField& field : vecFields)
        {
            qToLittleEndian(static_cast<qint32>(field.m_vecValues[static_cast<size_t>(iVertex)]), pCursor);
            pCursor += sizeof(qint32);
        }
    }
    for (int iFace = 0; iFace < iFaceCount; ++iFace)
    {
        *pCursor++ = 3;
        const std::uint32_t* kpFace = mesh.GetFace(iFace);
        for (int iCorner = 0; iCorner < 3; ++iCorner)
        {
            qToLittleEndian(static_cast<qint32>(kpFace[iCorner]), pCursor);
            pCursor += sizeof(qint32);
        }
    }

    // 쓰는 도중 꺼져도 이전 파일이 남도록 임시 파일에 쓴 뒤 바꿔 끼운다
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        m_strError = QStringLiteral("파일을 쓸 수 없다: %1").arg(file.errorString());
        return false;
    }
    file.write(header);
    file.write(body);
    if (!file.commit())
    {
        m_strError = QStringLiteral("파일을 저장하지 못했다: %1").arg(file.errorString());
        return false;
    }
    return true;
}

const QString& PlyWriter::GetError() const
{
    return m_strError;
}
