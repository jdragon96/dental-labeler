#include "Document/PlyReader.h"

#include "Document/Mesh.h"

#include <QByteArray>
#include <QFile>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
enum class EPlyType
{
    INVALID,
    INT8,
    UINT8,
    INT16,
    UINT16,
    INT32,
    UINT32,
    FLOAT32,
    FLOAT64,
};

enum class EPlyFormat
{
    ASCII,
    BINARY_LITTLE_ENDIAN,
    BINARY_BIG_ENDIAN,
};

struct PlyProperty
{
    QByteArray m_strName;
    EPlyType m_eType = EPlyType::INVALID;
    bool m_bList = false;
    EPlyType m_eCountType = EPlyType::INVALID;
};

struct PlyElement
{
    QByteArray m_strName;
    qint64 m_llCount = 0;
    std::vector<PlyProperty> m_vecProperties;
};

struct PlyHeader
{
    EPlyFormat m_eFormat = EPlyFormat::ASCII;
    bool m_bHasFormat = false;
    std::vector<PlyElement> m_vecElements;
    QStringList m_lstComments;
    qint64 m_llBodyOffset = 0;
};

EPlyType ParseType(const QByteArray& name)
{
    if (name == "char" || name == "int8")
    {
        return EPlyType::INT8;
    }
    if (name == "uchar" || name == "uint8")
    {
        return EPlyType::UINT8;
    }
    if (name == "short" || name == "int16")
    {
        return EPlyType::INT16;
    }
    if (name == "ushort" || name == "uint16")
    {
        return EPlyType::UINT16;
    }
    if (name == "int" || name == "int32")
    {
        return EPlyType::INT32;
    }
    if (name == "uint" || name == "uint32")
    {
        return EPlyType::UINT32;
    }
    if (name == "float" || name == "float32")
    {
        return EPlyType::FLOAT32;
    }
    if (name == "double" || name == "float64")
    {
        return EPlyType::FLOAT64;
    }
    return EPlyType::INVALID;
}

int GetTypeSize(EPlyType type)
{
    switch (type)
    {
    case EPlyType::INT8:
    case EPlyType::UINT8:
        return 1;
    case EPlyType::INT16:
    case EPlyType::UINT16:
        return 2;
    case EPlyType::INT32:
    case EPlyType::UINT32:
    case EPlyType::FLOAT32:
        return 4;
    case EPlyType::FLOAT64:
        return 8;
    case EPlyType::INVALID:
        break;
    }
    return 0;
}

template <typename T>
double DecodeAs(const unsigned char* kpBytes)
{
    T value;
    std::memcpy(&value, kpBytes, sizeof(T));
    return static_cast<double>(value);
}

// 본문 커서. 바이너리와 ASCII 를 같은 방식으로 값 하나씩 읽는다
class BodyReader
{
public:
    BodyReader(const char* kpBegin, const char* kpEnd, EPlyFormat format)
        : m_kpCursor(kpBegin)
        , m_kpEnd(kpEnd)
        , m_eFormat(format)
        , m_bSwap((format == EPlyFormat::BINARY_BIG_ENDIAN) != (Q_BYTE_ORDER == Q_BIG_ENDIAN))
    {
    }

public:
    bool Read(EPlyType type, double& value)
    {
        if (m_eFormat == EPlyFormat::ASCII)
        {
            return ReadAscii(value);
        }
        return ReadBinary(type, value);
    }

    bool ReadCount(EPlyType type, qint64& count)
    {
        double dValue = 0.0;
        if (!Read(type, dValue) || dValue < 0.0 || dValue > 1.0e9)
        {
            return false;
        }
        count = static_cast<qint64>(dValue);
        return true;
    }

private:
    bool ReadBinary(EPlyType type, double& value)
    {
        const int iSize = GetTypeSize(type);
        if (iSize == 0 || m_kpEnd - m_kpCursor < iSize)
        {
            return false;
        }
        unsigned char arrBytes[8];
        std::memcpy(arrBytes, m_kpCursor, static_cast<size_t>(iSize));
        m_kpCursor += iSize;
        if (m_bSwap)
        {
            std::reverse(arrBytes, arrBytes + iSize);
        }

        switch (type)
        {
        case EPlyType::INT8:
            value = DecodeAs<std::int8_t>(arrBytes);
            return true;
        case EPlyType::UINT8:
            value = DecodeAs<std::uint8_t>(arrBytes);
            return true;
        case EPlyType::INT16:
            value = DecodeAs<std::int16_t>(arrBytes);
            return true;
        case EPlyType::UINT16:
            value = DecodeAs<std::uint16_t>(arrBytes);
            return true;
        case EPlyType::INT32:
            value = DecodeAs<std::int32_t>(arrBytes);
            return true;
        case EPlyType::UINT32:
            value = DecodeAs<std::uint32_t>(arrBytes);
            return true;
        case EPlyType::FLOAT32:
            value = DecodeAs<float>(arrBytes);
            return true;
        case EPlyType::FLOAT64:
            value = DecodeAs<double>(arrBytes);
            return true;
        case EPlyType::INVALID:
            break;
        }
        return false;
    }

    bool ReadAscii(double& value)
    {
        while (m_kpCursor < m_kpEnd && IsSpace(*m_kpCursor))
        {
            ++m_kpCursor;
        }
        const char* kpToken = m_kpCursor;
        while (m_kpCursor < m_kpEnd && !IsSpace(*m_kpCursor))
        {
            ++m_kpCursor;
        }
        if (kpToken == m_kpCursor)
        {
            return false;
        }
        // QByteArray::toDouble 은 로캘과 상관없이 '.' 을 소수점으로 읽는다
        bool bOk = false;
        value = QByteArray::fromRawData(kpToken, static_cast<int>(m_kpCursor - kpToken)).toDouble(&bOk);
        return bOk;
    }

    static bool IsSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

private:
    const char* m_kpCursor;
    const char* m_kpEnd;
    EPlyFormat m_eFormat;
    bool m_bSwap;
};

bool ParseHeader(const char* kpBegin, const char* kpEnd, PlyHeader& header, QString& strError)
{
    constexpr int kMaxHeaderLines = 10000;

    const char* kpLine = kpBegin;
    int iLine = 0;
    while (kpLine < kpEnd && iLine < kMaxHeaderLines)
    {
        const char* kpNewline = static_cast<const char*>(std::memchr(kpLine, '\n', static_cast<size_t>(kpEnd - kpLine)));
        if (kpNewline == nullptr)
        {
            break;
        }
        const QByteArray strLine = QByteArray(kpLine, static_cast<int>(kpNewline - kpLine)).trimmed();
        kpLine = kpNewline + 1;
        ++iLine;

        if (iLine == 1)
        {
            if (strLine != "ply")
            {
                strError = QStringLiteral("PLY 파일이 아니다. 첫 줄이 'ply' 가 아니다.");
                return false;
            }
            continue;
        }

        const QList<QByteArray> lstTokens = strLine.simplified().split(' ');
        if (lstTokens.isEmpty() || lstTokens.first().isEmpty())
        {
            continue;
        }

        const QByteArray& strKey = lstTokens.first();
        if (strKey == "format")
        {
            if (lstTokens.size() < 2)
            {
                strError = QStringLiteral("PLY 헤더의 format 줄이 잘못되었다.");
                return false;
            }
            if (lstTokens[1] == "ascii")
            {
                header.m_eFormat = EPlyFormat::ASCII;
            }
            else if (lstTokens[1] == "binary_little_endian")
            {
                header.m_eFormat = EPlyFormat::BINARY_LITTLE_ENDIAN;
            }
            else if (lstTokens[1] == "binary_big_endian")
            {
                header.m_eFormat = EPlyFormat::BINARY_BIG_ENDIAN;
            }
            else
            {
                strError = QStringLiteral("지원하지 않는 PLY 형식이다: %1").arg(QString::fromLatin1(lstTokens[1]));
                return false;
            }
            header.m_bHasFormat = true;
        }
        else if (strKey == "comment" || strKey == "obj_info")
        {
            const QString strComment = QString::fromUtf8(strLine.mid(strKey.size()).trimmed());
            if (!strComment.isEmpty())
            {
                header.m_lstComments.append(strComment);
            }
        }
        else if (strKey == "element")
        {
            bool bOk = false;
            PlyElement element;
            if (lstTokens.size() >= 3)
            {
                element.m_strName = lstTokens[1];
                element.m_llCount = lstTokens[2].toLongLong(&bOk);
            }
            if (!bOk || element.m_llCount < 0)
            {
                strError = QStringLiteral("PLY 헤더 %1번째 줄의 element 가 잘못되었다.").arg(iLine);
                return false;
            }
            header.m_vecElements.push_back(element);
        }
        else if (strKey == "property")
        {
            if (header.m_vecElements.empty())
            {
                strError = QStringLiteral("PLY 헤더 %1번째 줄의 property 앞에 element 가 없다.").arg(iLine);
                return false;
            }
            PlyProperty property;
            if (lstTokens.size() >= 5 && lstTokens[1] == "list")
            {
                property.m_bList = true;
                property.m_eCountType = ParseType(lstTokens[2]);
                property.m_eType = ParseType(lstTokens[3]);
                property.m_strName = lstTokens[4];
            }
            else if (lstTokens.size() >= 3)
            {
                property.m_eType = ParseType(lstTokens[1]);
                property.m_strName = lstTokens[2];
            }
            if (property.m_eType == EPlyType::INVALID || (property.m_bList && property.m_eCountType == EPlyType::INVALID))
            {
                strError = QStringLiteral("PLY 헤더 %1번째 줄의 property 형식을 알 수 없다.").arg(iLine);
                return false;
            }
            header.m_vecElements.back().m_vecProperties.push_back(property);
        }
        else if (strKey == "end_header")
        {
            if (!header.m_bHasFormat)
            {
                strError = QStringLiteral("PLY 헤더에 format 줄이 없다.");
                return false;
            }
            header.m_llBodyOffset = kpLine - kpBegin;
            return true;
        }
    }

    strError = QStringLiteral("PLY 헤더가 end_header 로 끝나지 않는다.");
    return false;
}

int FindProperty(const PlyElement& element, const char* kpName)
{
    for (size_t i = 0; i < element.m_vecProperties.size(); ++i)
    {
        if (!element.m_vecProperties[i].m_bList && element.m_vecProperties[i].m_strName == kpName)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool SkipProperty(BodyReader& reader, const PlyProperty& property)
{
    double dValue = 0.0;
    if (!property.m_bList)
    {
        return reader.Read(property.m_eType, dValue);
    }
    qint64 llCount = 0;
    if (!reader.ReadCount(property.m_eCountType, llCount))
    {
        return false;
    }
    for (qint64 i = 0; i < llCount; ++i)
    {
        if (!reader.Read(property.m_eType, dValue))
        {
            return false;
        }
    }
    return true;
}

bool ReadVertices(BodyReader& reader, const PlyElement& element, Mesh& mesh, QString& strError)
{
    const int iX = FindProperty(element, "x");
    const int iY = FindProperty(element, "y");
    const int iZ = FindProperty(element, "z");
    const int iNx = FindProperty(element, "nx");
    const int iNy = FindProperty(element, "ny");
    const int iNz = FindProperty(element, "nz");
    if (iX < 0 || iY < 0 || iZ < 0)
    {
        strError = QStringLiteral("vertex 요소에 x y z 속성이 없다.");
        return false;
    }
    if (element.m_llCount > std::numeric_limits<int>::max() / 3)
    {
        strError = QStringLiteral("정점이 너무 많다: %L1개").arg(element.m_llCount);
        return false;
    }

    const bool bNormals = iNx >= 0 && iNy >= 0 && iNz >= 0;
    const size_t iCount = static_cast<size_t>(element.m_llCount);
    mesh.m_vecPositions.resize(iCount * 3);
    if (bNormals)
    {
        mesh.m_vecNormals.resize(iCount * 3);
    }

    const int iPropertyCount = static_cast<int>(element.m_vecProperties.size());
    for (size_t iVertex = 0; iVertex < iCount; ++iVertex)
    {
        for (int iProperty = 0; iProperty < iPropertyCount; ++iProperty)
        {
            const PlyProperty& property = element.m_vecProperties[static_cast<size_t>(iProperty)];
            if (property.m_bList)
            {
                if (!SkipProperty(reader, property))
                {
                    strError = QStringLiteral("정점 목록이 %L1번째 정점에서 끊겼다.").arg(iVertex + 1);
                    return false;
                }
                continue;
            }

            double dValue = 0.0;
            if (!reader.Read(property.m_eType, dValue))
            {
                strError = QStringLiteral("정점 목록이 %L1번째 정점에서 끊겼다.").arg(iVertex + 1);
                return false;
            }
            const float fValue = static_cast<float>(dValue);
            if (iProperty == iX)
            {
                mesh.m_vecPositions[iVertex * 3] = fValue;
            }
            else if (iProperty == iY)
            {
                mesh.m_vecPositions[iVertex * 3 + 1] = fValue;
            }
            else if (iProperty == iZ)
            {
                mesh.m_vecPositions[iVertex * 3 + 2] = fValue;
            }
            else if (bNormals && iProperty == iNx)
            {
                mesh.m_vecNormals[iVertex * 3] = fValue;
            }
            else if (bNormals && iProperty == iNy)
            {
                mesh.m_vecNormals[iVertex * 3 + 1] = fValue;
            }
            else if (bNormals && iProperty == iNz)
            {
                mesh.m_vecNormals[iVertex * 3 + 2] = fValue;
            }
        }
    }
    return true;
}

bool ReadFaces(BodyReader& reader, const PlyElement& element, Mesh& mesh, QString& strError)
{
    int iIndexProperty = -1;
    for (size_t i = 0; i < element.m_vecProperties.size(); ++i)
    {
        const PlyProperty& property = element.m_vecProperties[i];
        if (property.m_bList && (property.m_strName == "vertex_indices" || property.m_strName == "vertex_index"))
        {
            iIndexProperty = static_cast<int>(i);
            break;
        }
    }
    if (iIndexProperty < 0)
    {
        strError = QStringLiteral("face 요소에 vertex_indices 속성이 없다.");
        return false;
    }

    mesh.m_vecIndices.reserve(static_cast<size_t>(element.m_llCount) * 3);
    std::vector<std::uint32_t> vecPolygon;
    const int iPropertyCount = static_cast<int>(element.m_vecProperties.size());
    for (qint64 iFace = 0; iFace < element.m_llCount; ++iFace)
    {
        for (int iProperty = 0; iProperty < iPropertyCount; ++iProperty)
        {
            const PlyProperty& property = element.m_vecProperties[static_cast<size_t>(iProperty)];
            if (iProperty != iIndexProperty)
            {
                if (!SkipProperty(reader, property))
                {
                    strError = QStringLiteral("면 목록이 %L1번째 면에서 끊겼다.").arg(iFace + 1);
                    return false;
                }
                continue;
            }

            qint64 llCorners = 0;
            if (!reader.ReadCount(property.m_eCountType, llCorners))
            {
                strError = QStringLiteral("면 목록이 %L1번째 면에서 끊겼다.").arg(iFace + 1);
                return false;
            }
            vecPolygon.clear();
            for (qint64 iCorner = 0; iCorner < llCorners; ++iCorner)
            {
                double dIndex = 0.0;
                if (!reader.Read(property.m_eType, dIndex))
                {
                    strError = QStringLiteral("면 목록이 %L1번째 면에서 끊겼다.").arg(iFace + 1);
                    return false;
                }
                if (dIndex < 0.0 || dIndex > static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
                {
                    strError = QStringLiteral("%L1번째 면에 잘못된 정점 번호가 있다.").arg(iFace + 1);
                    return false;
                }
                vecPolygon.push_back(static_cast<std::uint32_t>(dIndex));
            }
            // 사각형 이상은 첫 꼭짓점을 중심으로 부채꼴로 나눈다
            for (size_t iCorner = 1; iCorner + 1 < vecPolygon.size(); ++iCorner)
            {
                mesh.m_vecIndices.push_back(vecPolygon[0]);
                mesh.m_vecIndices.push_back(vecPolygon[iCorner]);
                mesh.m_vecIndices.push_back(vecPolygon[iCorner + 1]);
            }
        }
    }
    return true;
}

bool SkipElement(BodyReader& reader, const PlyElement& element, QString& strError)
{
    for (qint64 iItem = 0; iItem < element.m_llCount; ++iItem)
    {
        for (const PlyProperty& property : element.m_vecProperties)
        {
            if (!SkipProperty(reader, property))
            {
                strError = QStringLiteral("%1 목록이 %L2번째 항목에서 끊겼다.")
                               .arg(QString::fromLatin1(element.m_strName))
                               .arg(iItem + 1);
                return false;
            }
        }
    }
    return true;
}
}

PlyReader::PlyReader()
    : m_strError()
    , m_strComment()
{
}

bool PlyReader::Read(const QString& path, Mesh& mesh)
{
    m_strError.clear();
    m_strComment.clear();
    mesh = Mesh();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        m_strError = QStringLiteral("파일을 열 수 없다: %1").arg(file.errorString());
        return false;
    }
    const qint64 llSize = file.size();
    if (llSize <= 0)
    {
        m_strError = QStringLiteral("빈 파일이다.");
        return false;
    }

    QByteArray bufFallback;
    const char* kpBegin = nullptr;
    uchar* pMapped = file.map(0, llSize);
    if (pMapped != nullptr)
    {
        kpBegin = reinterpret_cast<const char*>(pMapped);
    }
    else
    {
        bufFallback = file.readAll();
        kpBegin = bufFallback.constData();
    }
    const char* kpEnd = kpBegin + llSize;

    PlyHeader header;
    if (!ParseHeader(kpBegin, kpEnd, header, m_strError))
    {
        return false;
    }
    m_strComment = header.m_lstComments.join(QStringLiteral("; "));

    BodyReader reader(kpBegin + header.m_llBodyOffset, kpEnd, header.m_eFormat);
    bool bHasVertices = false;
    for (const PlyElement& element : header.m_vecElements)
    {
        bool bOk = true;
        if (element.m_strName == "vertex")
        {
            bOk = ReadVertices(reader, element, mesh, m_strError);
            bHasVertices = true;
        }
        else if (element.m_strName == "face")
        {
            bOk = ReadFaces(reader, element, mesh, m_strError);
        }
        else
        {
            bOk = SkipElement(reader, element, m_strError);
        }
        if (!bOk)
        {
            mesh = Mesh();
            return false;
        }
    }

    if (!bHasVertices || mesh.GetVertexCount() == 0)
    {
        m_strError = QStringLiteral("정점이 하나도 없다.");
        mesh = Mesh();
        return false;
    }

    const std::uint32_t uVertexCount = static_cast<std::uint32_t>(mesh.GetVertexCount());
    for (size_t i = 0; i < mesh.m_vecIndices.size(); ++i)
    {
        if (mesh.m_vecIndices[i] >= uVertexCount)
        {
            m_strError = QStringLiteral("%L1번째 면이 없는 정점 %L2 을 가리킨다.")
                             .arg(i / 3 + 1)
                             .arg(mesh.m_vecIndices[i]);
            mesh = Mesh();
            return false;
        }
    }

    if (pMapped != nullptr)
    {
        file.unmap(pMapped);
    }
    return true;
}

const QString& PlyReader::GetError() const
{
    return m_strError;
}

const QString& PlyReader::GetComment() const
{
    return m_strComment;
}
