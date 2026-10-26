#include "Document/LabelFile.h"

#include "Core/LabelTable.h"
#include "Document/LabelDocument.h"
#include "Document/Mesh.h"
#include "Document/PlyReader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <algorithm>
#include <map>

namespace
{
const QString kCommentPrefix = QStringLiteral("dentallabeler ");
const QString kLabeledSuffix = QStringLiteral(".labeled.ply");
const QString kAutosaveSuffix = QStringLiteral(".autosave.ply");
// 1: int label = FDI 번호 (2026-10-26). 2: int fdi · label(클래스) · instance
constexpr int kFormatVersion = 2;

// 파일 이름 · 메모의 공백과 한글을 ASCII 로 바꿔 comment 한 줄 안에 넣는다
QString Encode(const QString& text)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(text));
}

QString Decode(const QString& text)
{
    return QString::fromUtf8(QByteArray::fromPercentEncoding(text.toLatin1()));
}

// 이전 형식의 값(-1 미분류, 0 치은, 11–48 FDI)을 정점 코드로
bool ConvertLegacyValue(int value, int& code)
{
    if (value == kUnlabeled || value == kGingiva)
    {
        code = value;
        return true;
    }
    if (LabelTable::IsFdi(value))
    {
        code = LabelTable::MakeCode(LabelTable::kClassTooth, value);
        return true;
    }
    return false;
}

// 인스턴스를 매길 대상의 순서: 클래스 → 치식 순서 (상악 18 … 28, 하악 48 … 38) → 자리 없음
int GetArchRank(int fdi)
{
    const std::vector<int>& vecOrder = LabelTable::GetArchOrder(EJaw::UNKNOWN);
    auto it = std::find(vecOrder.begin(), vecOrder.end(), fdi);
    return it == vecOrder.end() ? static_cast<int>(vecOrder.size()) : static_cast<int>(it - vecOrder.begin());
}

std::map<int, int> AssignInstances(const std::vector<LabelValue>& vecCodes)
{
    std::vector<bool> vecPresent(static_cast<size_t>(LabelTable::kCodeSlotCount), false);
    for (LabelValue code : vecCodes)
    {
        if (code > kGingiva)
        {
            vecPresent[static_cast<size_t>(code) + 1] = true;
        }
    }
    std::vector<int> vecObjects;
    for (size_t i = 0; i < vecPresent.size(); ++i)
    {
        if (vecPresent[i])
        {
            vecObjects.push_back(static_cast<int>(i) - 1);
        }
    }
    std::sort(vecObjects.begin(), vecObjects.end(), [](int a, int b) {
        const int iClassA = LabelTable::GetClassOf(a);
        const int iClassB = LabelTable::GetClassOf(b);
        if (iClassA != iClassB)
        {
            return iClassA < iClassB;
        }
        return GetArchRank(LabelTable::GetFdiOf(a)) < GetArchRank(LabelTable::GetFdiOf(b));
    });

    std::map<int, int> mapInstances;
    for (size_t i = 0; i < vecObjects.size(); ++i)
    {
        mapInstances[vecObjects[i]] = static_cast<int>(i) + 1;
    }
    return mapInstances;
}
}

namespace LabelFile
{
CasePaths ResolveCasePaths(const QString& openedPath)
{
    const QFileInfo info(openedPath);
    const QDir dir = info.absoluteDir();
    const QString strName = info.fileName();

    CasePaths paths;
    if (strName.endsWith(kLabeledSuffix, Qt::CaseInsensitive))
    {
        paths.m_strCaseId = strName.left(strName.size() - kLabeledSuffix.size());
        const QString strOriginal = dir.filePath(paths.m_strCaseId + QStringLiteral(".ply"));
        paths.m_bStandalone = !QFileInfo::exists(strOriginal);
        paths.m_strMesh = paths.m_bStandalone ? info.absoluteFilePath() : strOriginal;
    }
    else
    {
        paths.m_strCaseId = info.completeBaseName();
        paths.m_strMesh = info.absoluteFilePath();
    }
    paths.m_strLabeled = dir.filePath(paths.m_strCaseId + kLabeledSuffix);
    paths.m_strAutosave = dir.filePath(paths.m_strCaseId + kAutosaveSuffix);
    paths.m_strLegacyJson = dir.filePath(paths.m_strCaseId + QStringLiteral(".labels.json"));
    return paths;
}

std::vector<PlyIntField> EncodeFields(const std::vector<LabelValue>& vecCodes)
{
    const std::map<int, int> mapInstances = AssignInstances(vecCodes);
    std::vector<PlyIntField> vecFields(3);
    vecFields[0].m_strName = "fdi";
    vecFields[1].m_strName = "label";
    vecFields[2].m_strName = "instance";
    for (PlyIntField& field : vecFields)
    {
        field.m_vecValues.resize(vecCodes.size());
    }
    for (size_t i = 0; i < vecCodes.size(); ++i)
    {
        const int iCode = vecCodes[i];
        vecFields[0].m_vecValues[i] = LabelTable::GetFdiOf(iCode);
        vecFields[1].m_vecValues[i] = LabelTable::GetClassOf(iCode);
        auto it = mapInstances.find(iCode);
        vecFields[2].m_vecValues[i] = it == mapInstances.end() ? 0 : it->second;
    }
    return vecFields;
}

bool HasLabelFields(const PlyReader& reader)
{
    return reader.HasField("label") || reader.HasField("fdi");
}

bool DecodeFields(const PlyReader& reader, std::vector<int>& vecCodes, QString& strError)
{
    const bool bLabel = reader.HasField("label");
    const bool bFdi = reader.HasField("fdi");
    vecCodes.clear();

    if (bLabel && bFdi)
    {
        const std::vector<int>& vecLabel = reader.GetField("label");
        const std::vector<int>& vecFdi = reader.GetField("fdi");
        vecCodes.resize(vecLabel.size());
        for (size_t i = 0; i < vecLabel.size(); ++i)
        {
            const int iCode = vecLabel[i] < 0 ? static_cast<int>(kUnlabeled) : LabelTable::MakeCode(vecLabel[i], vecFdi[i]);
            if (!LabelTable::IsValidCode(iCode))
            {
                strError = QStringLiteral("%L1번째 정점의 label %2 · fdi %3 조합을 쓸 수 없다. 클래스 표에 없거나 FDI 번호가 맞지 않는다.")
                               .arg(i + 1)
                               .arg(vecLabel[i])
                               .arg(vecFdi[i]);
                return false;
            }
            vecCodes[i] = iCode;
        }
        return true;
    }

    // label 만 있으면 이전 형식(label = FDI 번호), fdi 만 있으면 0 을 치은으로 본다
    const std::vector<int>& vecValues = bLabel ? reader.GetField("label") : reader.GetField("fdi");
    if (!bLabel && !bFdi)
    {
        strError = QStringLiteral("라벨 필드(fdi · label)가 없다.");
        return false;
    }
    vecCodes.resize(vecValues.size());
    for (size_t i = 0; i < vecValues.size(); ++i)
    {
        if (!ConvertLegacyValue(vecValues[i], vecCodes[i]))
        {
            strError = QStringLiteral("%1 필드에 FDI 번호가 아닌 값이 있다 (%L2번째 정점: %3).")
                           .arg(bLabel ? QStringLiteral("label") : QStringLiteral("fdi"))
                           .arg(i + 1)
                           .arg(vecValues[i]);
            return false;
        }
    }
    return true;
}

bool Save(const QString& path, const Mesh& mesh, const QStringList& lstSourceComments, const LabelDocument& document,
          QString& strError, int unsavedEditCount)
{
    const SourceInfo& source = document.GetSource();
    const std::vector<PlyIntField> vecFields = EncodeFields(document.GetLabels());

    QStringList lstComments = StripOwnComments(lstSourceComments);
    lstComments << kCommentPrefix + QStringLiteral("format %1").arg(kFormatVersion)
                << kCommentPrefix + QStringLiteral("fields fdi label instance");
    lstComments << kCommentPrefix + QStringLiteral("class -1 unlabeled");
    for (const LabelTable::ClassInfo& info : LabelTable::GetClasses())
    {
        lstComments << kCommentPrefix + QStringLiteral("class %1 %2").arg(info.m_iValue).arg(QString::fromLatin1(info.m_kpKey));
    }
    // 대상마다 instance 번호와 클래스 · 자리 (사람이 헤더만 보고도 알 수 있게)
    std::map<int, std::pair<int, int>> mapObjects;
    for (size_t i = 0; i < document.GetLabels().size(); ++i)
    {
        const int iInstance = vecFields[2].m_vecValues[i];
        if (iInstance > 0 && mapObjects.count(iInstance) == 0)
        {
            mapObjects[iInstance] = { vecFields[1].m_vecValues[i], vecFields[0].m_vecValues[i] };
        }
    }
    for (const auto& it : mapObjects)
    {
        lstComments << kCommentPrefix + QStringLiteral("instance %1 label %2 fdi %3").arg(it.first).arg(it.second.first).arg(it.second.second);
    }

    lstComments << kCommentPrefix + QStringLiteral("source %1 %2 %3 %4")
                                        .arg(Encode(source.m_strFile), source.m_strSha256.isEmpty() ? QStringLiteral("-") : source.m_strSha256)
                                        .arg(source.m_iVertexCount)
                                        .arg(source.m_iFaceCount)
                << kCommentPrefix + QStringLiteral("jaw %1").arg(LabelTable::GetJawKey(document.GetJaw()))
                << kCommentPrefix + QStringLiteral("status %1").arg(Encode(document.GetStatus()));
    if (!document.GetAssignee().isEmpty())
    {
        lstComments << kCommentPrefix + QStringLiteral("assignee %1").arg(Encode(document.GetAssignee()));
    }
    if (document.GetUpdatedAt().isValid())
    {
        const QDateTime& dtUpdated = document.GetUpdatedAt();
        lstComments << kCommentPrefix + QStringLiteral("updated %1").arg(dtUpdated.toOffsetFromUtc(dtUpdated.offsetFromUtc()).toString(Qt::ISODate));
    }
    for (const auto& it : document.GetTeeth())
    {
        QString strTooth = kCommentPrefix + QStringLiteral("tooth %1 %2").arg(it.first).arg(LabelTable::GetToothStateKey(it.second.m_eState));
        if (!it.second.m_strNote.isEmpty())
        {
            strTooth += QLatin1Char(' ') + Encode(it.second.m_strNote);
        }
        lstComments << strTooth;
    }
    if (unsavedEditCount >= 0)
    {
        lstComments << kCommentPrefix + QStringLiteral("unsaved_edits %1").arg(unsavedEditCount);
    }

    PlyWriter writer;
    if (!writer.Write(path, mesh, lstComments, vecFields))
    {
        strError = writer.GetError();
        return false;
    }
    return true;
}

bool Load(const QString& path, LabelDocument& document, Mesh& mesh, QString& strError, int* pUnsavedEditCount)
{
    PlyReader reader;
    if (!reader.Read(path, mesh))
    {
        strError = reader.GetError();
        return false;
    }
    std::vector<int> vecCodes;
    if (!DecodeFields(reader, vecCodes, strError))
    {
        return false;
    }

    SourceInfo fallback;
    fallback.m_strFile = QFileInfo(path).fileName();
    fallback.m_iVertexCount = mesh.GetVertexCount();
    fallback.m_iFaceCount = mesh.GetFaceCount();
    return BuildDocument(vecCodes, reader.GetComments(), fallback, document, strError, pUnsavedEditCount);
}

bool BuildDocument(const std::vector<int>& vecCodes, const QStringList& lstComments, const SourceInfo& fallbackSource,
                   LabelDocument& document, QString& strError, int* pUnsavedEditCount)
{
    SourceInfo source = fallbackSource;
    EJaw eJaw = EJaw::UNKNOWN;
    QString strStatus = QStringLiteral("new");
    QString strAssignee;
    QDateTime dtUpdated;
    std::map<int, ToothInfo> mapTeeth;
    int iUnsavedEdits = 0;

    for (const QString& comment : lstComments)
    {
        if (!comment.startsWith(kCommentPrefix))
        {
            continue;
        }
        const QStringList lstTokens = comment.mid(kCommentPrefix.size()).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (lstTokens.isEmpty())
        {
            continue;
        }
        const QString& strKey = lstTokens.first();
        if (strKey == QLatin1String("format") && lstTokens.size() >= 2 && lstTokens[1].toInt() > kFormatVersion)
        {
            strError = QStringLiteral("더 새 버전의 앱이 만든 라벨 파일이다 (format %1).").arg(lstTokens[1]);
            return false;
        }
        if (strKey == QLatin1String("source") && lstTokens.size() >= 5)
        {
            source.m_strFile = Decode(lstTokens[1]);
            source.m_strSha256 = lstTokens[2] == QLatin1String("-") ? QString() : lstTokens[2];
            source.m_iVertexCount = lstTokens[3].toInt();
            source.m_iFaceCount = lstTokens[4].toInt();
        }
        else if (strKey == QLatin1String("jaw") && lstTokens.size() >= 2)
        {
            eJaw = LabelTable::ParseJawKey(lstTokens[1]);
        }
        else if (strKey == QLatin1String("status") && lstTokens.size() >= 2)
        {
            strStatus = Decode(lstTokens[1]);
        }
        else if (strKey == QLatin1String("assignee") && lstTokens.size() >= 2)
        {
            strAssignee = Decode(lstTokens[1]);
        }
        else if (strKey == QLatin1String("updated") && lstTokens.size() >= 2)
        {
            dtUpdated = QDateTime::fromString(lstTokens[1], Qt::ISODate);
        }
        else if (strKey == QLatin1String("tooth") && lstTokens.size() >= 3)
        {
            const int iFdi = lstTokens[1].toInt();
            ToothInfo info;
            if (LabelTable::IsFdi(iFdi) && LabelTable::ParseToothStateKey(lstTokens[2], info.m_eState))
            {
                info.m_strNote = lstTokens.size() >= 4 ? Decode(lstTokens[3]) : QString();
                mapTeeth[iFdi] = info;
            }
        }
        else if (strKey == QLatin1String("unsaved_edits") && lstTokens.size() >= 2)
        {
            iUnsavedEdits = lstTokens[1].toInt();
        }
    }

    std::vector<LabelValue> vecValues;
    vecValues.reserve(vecCodes.size());
    for (size_t i = 0; i < vecCodes.size(); ++i)
    {
        if (!LabelTable::IsValidCode(vecCodes[i]))
        {
            strError = QStringLiteral("%L1번째 정점의 라벨 값(%2)을 쓸 수 없다.").arg(i + 1).arg(vecCodes[i]);
            return false;
        }
        vecValues.push_back(static_cast<LabelValue>(vecCodes[i]));
    }
    if (static_cast<int>(vecValues.size()) != fallbackSource.m_iVertexCount)
    {
        strError = QStringLiteral("라벨 수(%L1)가 정점 수(%L2)와 다르다.").arg(vecValues.size()).arg(fallbackSource.m_iVertexCount);
        return false;
    }

    LabelDocument loaded;
    loaded.Reset(source, eJaw);
    loaded.SetLabels(std::move(vecValues));
    for (const auto& it : mapTeeth)
    {
        loaded.SetTooth(it.first, it.second);
    }
    loaded.SetStatus(strStatus);
    loaded.SetAssignee(strAssignee);
    loaded.SetUpdatedAt(dtUpdated);
    if (pUnsavedEditCount != nullptr)
    {
        *pUnsavedEditCount = iUnsavedEdits;
    }
    document = std::move(loaded);
    return true;
}

QStringList StripOwnComments(const QStringList& lstComments)
{
    QStringList lstOthers;
    for (const QString& comment : lstComments)
    {
        if (!comment.startsWith(kCommentPrefix))
        {
            lstOthers << comment;
        }
    }
    return lstOthers;
}

bool LoadLegacyJson(const QString& path, LabelDocument& document, QString& strError)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        strError = QStringLiteral("이전 라벨 파일을 열 수 없다: %1").arg(file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument jsonDocument = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject objRoot = jsonDocument.object();
    if (parseError.error != QJsonParseError::NoError || objRoot.value(QStringLiteral("format")).toString() != QLatin1String("dentallabeler.labels"))
    {
        strError = QStringLiteral("이전 형식 라벨 파일이 아니다.");
        return false;
    }

    const QJsonObject objSource = objRoot.value(QStringLiteral("source")).toObject();
    SourceInfo source;
    source.m_strFile = objSource.value(QStringLiteral("file")).toString();
    source.m_iVertexCount = objSource.value(QStringLiteral("vertex_count")).toInt();
    source.m_iFaceCount = objSource.value(QStringLiteral("face_count")).toInt();
    source.m_strSha256 = objSource.value(QStringLiteral("sha256")).toString();

    std::vector<int> vecCodes;
    const QJsonArray arrLabels = objRoot.value(QStringLiteral("labels")).toArray();
    vecCodes.reserve(static_cast<size_t>(arrLabels.size()));
    for (const QJsonValue& value : arrLabels)
    {
        int iCode = kUnlabeled;
        if (!ConvertLegacyValue(value.toInt(kUnlabeled), iCode))
        {
            strError = QStringLiteral("이전 라벨 파일에 FDI 번호가 아닌 값(%1)이 있다.").arg(value.toInt());
            return false;
        }
        vecCodes.push_back(iCode);
    }

    // 새 형식의 comment 로 옮겨 BuildDocument 의 검사를 그대로 쓴다
    QStringList lstComments;
    lstComments << kCommentPrefix + QStringLiteral("jaw %1").arg(objRoot.value(QStringLiteral("jaw")).toString())
                << kCommentPrefix + QStringLiteral("status %1").arg(Encode(objRoot.value(QStringLiteral("status")).toString(QStringLiteral("new"))))
                << kCommentPrefix + QStringLiteral("updated %1").arg(objRoot.value(QStringLiteral("updated_at")).toString());
    const QString strAssignee = objRoot.value(QStringLiteral("assignee")).toString();
    if (!strAssignee.isEmpty())
    {
        lstComments << kCommentPrefix + QStringLiteral("assignee %1").arg(Encode(strAssignee));
    }
    const QJsonObject objTeeth = objRoot.value(QStringLiteral("teeth")).toObject();
    for (auto it = objTeeth.begin(); it != objTeeth.end(); ++it)
    {
        const QJsonObject objTooth = it.value().toObject();
        QString strTooth = kCommentPrefix + QStringLiteral("tooth %1 %2").arg(it.key(), objTooth.value(QStringLiteral("state")).toString());
        const QString strNote = objTooth.value(QStringLiteral("note")).toString();
        if (!strNote.isEmpty())
        {
            strTooth += QLatin1Char(' ') + Encode(strNote);
        }
        lstComments << strTooth;
    }
    return BuildDocument(vecCodes, lstComments, source, document, strError);
}
}
