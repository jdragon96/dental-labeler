#include "Core/LabelTable.h"

#include <QFileInfo>
#include <QRegularExpression>

#include <array>

namespace
{
struct LabelColor
{
    int iLabel;
    unsigned int uRgb;
};

// 아치에서 이웃한 두 치아가 색상으로 갈리도록 고른 기본 팔레트. 붉은 분홍 계열은 치은에만 쓴다
const std::array<LabelColor, 33> kLabelColors = { {
    { 11, 0x4FA8F0 }, { 12, 0xF19A4A }, { 13, 0xB07AE8 }, { 14, 0x3FB6A0 },
    { 15, 0xE7C64A }, { 16, 0x5A7BEA }, { 17, 0xC99A2E }, { 18, 0x8FD0E8 },
    { 21, 0xE2B640 }, { 22, 0x43C3B8 }, { 23, 0x8E83F2 }, { 24, 0xF0915A },
    { 25, 0x59B86A }, { 26, 0xC58AF0 }, { 27, 0x3C9FE0 }, { 28, 0xD9D35A },
    { 31, 0xF2A93B }, { 32, 0x2FB7C9 }, { 33, 0xA884F0 }, { 34, 0x9BCB45 },
    { 35, 0x4D8EF0 }, { 36, 0xF07E4F }, { 37, 0x36C08A }, { 38, 0xD6A7F5 },
    { 41, 0x6C7FF2 }, { 42, 0xF0D24A }, { 43, 0x29BFB0 }, { 44, 0xEE8A3C },
    { 45, 0x7FCB5B }, { 46, 0x3E9BE8 }, { 47, 0xE0B23A }, { 48, 0xB98AF0 },
    { 0, 0xC98B95 },
} };
}

namespace LabelTable
{
bool IsTooth(int label)
{
    const int iQuadrant = label / 10;
    const int iPosition = label % 10;
    return label > 0 && iQuadrant >= 1 && iQuadrant <= 4 && iPosition >= 1 && iPosition <= 8;
}

bool IsValidLabel(int label)
{
    return label == kUnlabeled || label == kGingiva || IsTooth(label);
}

EJaw GetJaw(int label)
{
    if (!IsTooth(label))
    {
        return EJaw::UNKNOWN;
    }
    return label / 10 <= 2 ? EJaw::UPPER : EJaw::LOWER;
}

QColor GetColor(int label)
{
    for (const LabelColor& labelColor : kLabelColors)
    {
        if (labelColor.iLabel == label)
        {
            return QColor(QRgb(labelColor.uRgb));
        }
    }
    return QColor(0x8B, 0x96, 0xA1);
}

QString GetToothTypeName(int label)
{
    static const std::array<const char*, 9> arrNames = { {
        "", "중절치", "측절치", "견치", "제1소구치", "제2소구치", "제1대구치", "제2대구치", "제3대구치",
    } };
    if (!IsTooth(label))
    {
        return QString();
    }
    return QString::fromUtf8(arrNames[static_cast<size_t>(label % 10)]);
}

QString GetName(int label)
{
    if (label == kUnlabeled)
    {
        return QStringLiteral("미분류");
    }
    if (label == kGingiva)
    {
        return QStringLiteral("치은");
    }
    if (!IsTooth(label))
    {
        return QString::number(label);
    }
    static const std::array<const char*, 5> arrQuadrants = { {
        "", "상악 우측", "상악 좌측", "하악 좌측", "하악 우측",
    } };
    return QString::fromUtf8(arrQuadrants[static_cast<size_t>(label / 10)]) + QLatin1Char(' ') + GetToothTypeName(label);
}

const std::vector<int>& GetArchOrder(EJaw jaw)
{
    static const std::vector<int> vecUpper = { 18, 17, 16, 15, 14, 13, 12, 11, 21, 22, 23, 24, 25, 26, 27, 28 };
    static const std::vector<int> vecLower = { 48, 47, 46, 45, 44, 43, 42, 41, 31, 32, 33, 34, 35, 36, 37, 38 };
    static const std::vector<int> vecBoth = [] {
        std::vector<int> vecAll = vecUpper;
        vecAll.insert(vecAll.end(), vecLower.begin(), vecLower.end());
        return vecAll;
    }();

    switch (jaw)
    {
    case EJaw::UPPER:
        return vecUpper;
    case EJaw::LOWER:
        return vecLower;
    case EJaw::UNKNOWN:
        break;
    }
    return vecBoth;
}

QString GetToothStateName(EToothState state)
{
    switch (state)
    {
    case EToothState::NORMAL:
        return QStringLiteral("정상");
    case EToothState::MISSING:
        return QStringLiteral("결손");
    case EToothState::IMPLANT:
        return QStringLiteral("임플란트");
    case EToothState::CROWN:
        return QStringLiteral("보철");
    case EToothState::PARTIAL_ERUPTION:
        return QStringLiteral("부분 맹출");
    case EToothState::SUPERNUMERARY:
        return QStringLiteral("과잉치");
    }
    return QString();
}

QString GetToothStateKey(EToothState state)
{
    switch (state)
    {
    case EToothState::NORMAL:
        return QStringLiteral("normal");
    case EToothState::MISSING:
        return QStringLiteral("missing");
    case EToothState::IMPLANT:
        return QStringLiteral("implant");
    case EToothState::CROWN:
        return QStringLiteral("crown");
    case EToothState::PARTIAL_ERUPTION:
        return QStringLiteral("partial_eruption");
    case EToothState::SUPERNUMERARY:
        return QStringLiteral("supernumerary");
    }
    return QString();
}

bool ParseToothStateKey(const QString& key, EToothState& state)
{
    const std::array<EToothState, 6> arrStates = { {
        EToothState::NORMAL, EToothState::MISSING, EToothState::IMPLANT,
        EToothState::CROWN, EToothState::PARTIAL_ERUPTION, EToothState::SUPERNUMERARY,
    } };
    for (EToothState eState : arrStates)
    {
        if (GetToothStateKey(eState) == key)
        {
            state = eState;
            return true;
        }
    }
    return false;
}

QString GetJawName(EJaw jaw)
{
    switch (jaw)
    {
    case EJaw::UPPER:
        return QStringLiteral("상악");
    case EJaw::LOWER:
        return QStringLiteral("하악");
    case EJaw::UNKNOWN:
        break;
    }
    return QStringLiteral("턱 모름");
}

QString GetJawKey(EJaw jaw)
{
    switch (jaw)
    {
    case EJaw::UPPER:
        return QStringLiteral("upper");
    case EJaw::LOWER:
        return QStringLiteral("lower");
    case EJaw::UNKNOWN:
        break;
    }
    return QStringLiteral("unknown");
}

EJaw ParseJawKey(const QString& key)
{
    if (key == QLatin1String("upper"))
    {
        return EJaw::UPPER;
    }
    if (key == QLatin1String("lower"))
    {
        return EJaw::LOWER;
    }
    return EJaw::UNKNOWN;
}

EJaw GuessJawFromFileName(const QString& fileName)
{
    const QString strBase = QFileInfo(fileName).completeBaseName().toLower();
    static const QRegularExpression regexLower(QStringLiteral("(^|[_\\-. ])(lower|mandible|mandibular|mand|l)($|[_\\-. ])"));
    static const QRegularExpression regexUpper(QStringLiteral("(^|[_\\-. ])(upper|maxilla|maxillary|max|u)($|[_\\-. ])"));
    const bool bLower = regexLower.match(strBase).hasMatch();
    const bool bUpper = regexUpper.match(strBase).hasMatch();
    if (bLower == bUpper)
    {
        return EJaw::UNKNOWN;
    }
    return bLower ? EJaw::LOWER : EJaw::UPPER;
}
}
