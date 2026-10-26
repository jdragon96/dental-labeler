#include "Core/LabelTable.h"

#include <QFileInfo>
#include <QRegularExpression>

#include <array>

namespace
{
struct FdiColor
{
    int iFdi;
    unsigned int uRgb;
};

// 아치에서 이웃한 두 치아가 색상으로 갈리도록 고른 기본 팔레트. 붉은 분홍 계열은 치은에만 쓴다
const std::array<FdiColor, 32> kFdiColors = { {
    { 11, 0x4FA8F0 }, { 12, 0xF19A4A }, { 13, 0xB07AE8 }, { 14, 0x3FB6A0 },
    { 15, 0xE7C64A }, { 16, 0x5A7BEA }, { 17, 0xC99A2E }, { 18, 0x8FD0E8 },
    { 21, 0xE2B640 }, { 22, 0x43C3B8 }, { 23, 0x8E83F2 }, { 24, 0xF0915A },
    { 25, 0x59B86A }, { 26, 0xC58AF0 }, { 27, 0x3C9FE0 }, { 28, 0xD9D35A },
    { 31, 0xF2A93B }, { 32, 0x2FB7C9 }, { 33, 0xA884F0 }, { 34, 0x9BCB45 },
    { 35, 0x4D8EF0 }, { 36, 0xF07E4F }, { 37, 0x36C08A }, { 38, 0xD6A7F5 },
    { 41, 0x6C7FF2 }, { 42, 0xF0D24A }, { 43, 0x29BFB0 }, { 44, 0xEE8A3C },
    { 45, 0x7FCB5B }, { 46, 0x3E9BE8 }, { 47, 0xE0B23A }, { 48, 0xB98AF0 },
} };

const QColor kGingivaColor(0xC9, 0x8B, 0x95);
const QColor kScanbodyColor(0xA0, 0xAE, 0xC0);
const QColor kOtherColor(0x8B, 0x96, 0xA1);

QColor GetFdiColor(int fdi)
{
    for (const FdiColor& fdiColor : kFdiColors)
    {
        if (fdiColor.iFdi == fdi)
        {
            return QColor(QRgb(fdiColor.uRgb));
        }
    }
    return kOtherColor;
}
}

namespace LabelTable
{
const std::vector<ClassInfo>& GetClasses()
{
    static const std::vector<ClassInfo> vecClasses = {
        { kClassGingiva, "gingiva", "치은", EFdiUse::NONE },
        { kClassTooth, "tooth", "치아", EFdiUse::REQUIRED },
        { kClassScanbody, "scanbody", "스캔바디", EFdiUse::ALLOWED },
    };
    return vecClasses;
}

const ClassInfo* FindClass(int classValue)
{
    for (const ClassInfo& info : GetClasses())
    {
        if (info.m_iValue == classValue)
        {
            return &info;
        }
    }
    return nullptr;
}

QString GetClassName(int classValue)
{
    const ClassInfo* kpInfo = FindClass(classValue);
    return kpInfo != nullptr ? QString::fromUtf8(kpInfo->m_kpName) : QStringLiteral("클래스 %1").arg(classValue);
}

int MakeCode(int classValue, int fdi)
{
    return classValue * kCodeClassStride + fdi;
}

int GetClassOf(int code)
{
    return code < 0 ? -1 : code / kCodeClassStride;
}

int GetFdiOf(int code)
{
    return code < 0 ? 0 : code % kCodeClassStride;
}

bool IsValidCode(int code)
{
    if (code == kUnlabeled)
    {
        return true;
    }
    if (code < 0 || code >= (kMaxClass + 1) * kCodeClassStride)
    {
        return false;
    }
    const ClassInfo* kpInfo = FindClass(GetClassOf(code));
    if (kpInfo == nullptr)
    {
        return false;
    }
    const int iFdi = GetFdiOf(code);
    switch (kpInfo->m_eFdiUse)
    {
    case EFdiUse::NONE:
        return iFdi == 0;
    case EFdiUse::REQUIRED:
        return IsFdi(iFdi);
    case EFdiUse::ALLOWED:
        return iFdi == 0 || IsFdi(iFdi);
    }
    return false;
}

bool IsToothCode(int code)
{
    return GetClassOf(code) == kClassTooth && IsFdi(GetFdiOf(code));
}

bool IsFdi(int fdi)
{
    const int iQuadrant = fdi / 10;
    const int iPosition = fdi % 10;
    return fdi > 0 && iQuadrant >= 1 && iQuadrant <= 4 && iPosition >= 1 && iPosition <= 8;
}

EJaw GetJawOfFdi(int fdi)
{
    if (!IsFdi(fdi))
    {
        return EJaw::UNKNOWN;
    }
    return fdi / 10 <= 2 ? EJaw::UPPER : EJaw::LOWER;
}

QString GetFdiTypeName(int fdi)
{
    static const std::array<const char*, 9> arrNames = { {
        "", "중절치", "측절치", "견치", "제1소구치", "제2소구치", "제1대구치", "제2대구치", "제3대구치",
    } };
    if (!IsFdi(fdi))
    {
        return QString();
    }
    return QString::fromUtf8(arrNames[static_cast<size_t>(fdi % 10)]);
}

QString GetFdiName(int fdi)
{
    static const std::array<const char*, 5> arrQuadrants = { {
        "", "상악 우측", "상악 좌측", "하악 좌측", "하악 우측",
    } };
    if (!IsFdi(fdi))
    {
        return QString();
    }
    return QString::fromUtf8(arrQuadrants[static_cast<size_t>(fdi / 10)]) + QLatin1Char(' ') + GetFdiTypeName(fdi);
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

EJaw GetJaw(int code)
{
    return GetJawOfFdi(GetFdiOf(code));
}

QColor GetColor(int code)
{
    if (code == kGingiva)
    {
        return kGingivaColor;
    }
    const int iClass = GetClassOf(code);
    const int iFdi = GetFdiOf(code);
    if (iClass == kClassTooth)
    {
        return GetFdiColor(iFdi);
    }
    if (iClass == kClassScanbody)
    {
        // 스캔바디는 그 자리 치아 색을 옅게 쓴다. 자리가 없으면 금속 회색
        if (!IsFdi(iFdi))
        {
            return kScanbodyColor;
        }
        const QColor colFdi = GetFdiColor(iFdi);
        return QColor((colFdi.red() + 255) / 2, (colFdi.green() + 255) / 2, (colFdi.blue() + 255) / 2);
    }
    return kOtherColor;
}

QString GetName(int code)
{
    if (code == kUnlabeled)
    {
        return QStringLiteral("미분류");
    }
    const int iClass = GetClassOf(code);
    const int iFdi = GetFdiOf(code);
    if (iClass == kClassTooth && IsFdi(iFdi))
    {
        return GetFdiName(iFdi);
    }
    if (IsFdi(iFdi))
    {
        return QStringLiteral("%1 자리 %2").arg(iFdi).arg(GetClassName(iClass));
    }
    return GetClassName(iClass);
}

QString GetShortText(int code)
{
    if (code == kUnlabeled)
    {
        return QStringLiteral("−1");
    }
    const int iClass = GetClassOf(code);
    const int iFdi = GetFdiOf(code);
    if (iClass == kClassTooth)
    {
        return QString::number(iFdi);
    }
    if (iClass == kClassScanbody)
    {
        return iFdi > 0 ? QStringLiteral("SB%1").arg(iFdi) : QStringLiteral("SB");
    }
    if (iClass == kClassGingiva)
    {
        return QStringLiteral("0");
    }
    return QStringLiteral("C%1%2").arg(iClass).arg(iFdi > 0 ? QStringLiteral("·%1").arg(iFdi) : QString());
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
