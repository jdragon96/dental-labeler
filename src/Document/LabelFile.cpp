#include "Document/LabelFile.h"

#include "Core/LabelTable.h"
#include "Document/LabelDocument.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <vector>

namespace
{
const QString kFormatName = QStringLiteral("dentallabeler.labels");
constexpr int kFormatVersion = 1;
const QString kSchemaName = QStringLiteral("fdi-permanent@1");
}

namespace LabelFile
{
QString GetLabelPath(const QString& meshPath)
{
    const QFileInfo fileInfo(meshPath);
    return fileInfo.dir().filePath(fileInfo.completeBaseName() + QStringLiteral(".labels.json"));
}

QString GetAutosavePath(const QString& meshPath)
{
    const QFileInfo fileInfo(meshPath);
    return fileInfo.dir().filePath(fileInfo.completeBaseName() + QStringLiteral(".autosave"));
}

bool Save(const QString& path, const LabelDocument& document, QString& strError, int unsavedEditCount)
{
    const SourceInfo& source = document.GetSource();

    QJsonObject objSource;
    objSource.insert(QStringLiteral("file"), source.m_strFile);
    objSource.insert(QStringLiteral("vertex_count"), source.m_iVertexCount);
    objSource.insert(QStringLiteral("face_count"), source.m_iFaceCount);
    objSource.insert(QStringLiteral("sha256"), source.m_strSha256);

    QJsonArray arrLabels;
    for (LabelValue value : document.GetLabels())
    {
        arrLabels.append(static_cast<int>(value));
    }

    QJsonObject objTeeth;
    for (const auto& it : document.GetTeeth())
    {
        QJsonObject objTooth;
        objTooth.insert(QStringLiteral("state"), LabelTable::GetToothStateKey(it.second.m_eState));
        if (!it.second.m_strNote.isEmpty())
        {
            objTooth.insert(QStringLiteral("note"), it.second.m_strNote);
        }
        objTeeth.insert(QString::number(it.first), objTooth);
    }

    QJsonObject objRoot;
    objRoot.insert(QStringLiteral("format"), kFormatName);
    objRoot.insert(QStringLiteral("version"), kFormatVersion);
    objRoot.insert(QStringLiteral("case_id"), QFileInfo(source.m_strFile).completeBaseName());
    objRoot.insert(QStringLiteral("jaw"), LabelTable::GetJawKey(document.GetJaw()));
    objRoot.insert(QStringLiteral("source"), objSource);
    objRoot.insert(QStringLiteral("schema"), kSchemaName);
    objRoot.insert(QStringLiteral("labels"), arrLabels);
    objRoot.insert(QStringLiteral("teeth"), objTeeth);
    objRoot.insert(QStringLiteral("status"), document.GetStatus());
    objRoot.insert(QStringLiteral("assignee"), document.GetAssignee());
    objRoot.insert(QStringLiteral("updated_at"), document.GetUpdatedAt().toOffsetFromUtc(document.GetUpdatedAt().offsetFromUtc()).toString(Qt::ISODate));
    objRoot.insert(QStringLiteral("review"), QJsonArray());
    if (unsavedEditCount >= 0)
    {
        objRoot.insert(QStringLiteral("unsaved_edit_count"), unsavedEditCount);
    }

    // 쓰는 도중 꺼져도 이전 파일이 남도록 임시 파일에 쓴 뒤 바꿔 끼운다
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        strError = QStringLiteral("라벨 파일을 쓸 수 없다: %1").arg(file.errorString());
        return false;
    }
    file.write(QJsonDocument(objRoot).toJson(QJsonDocument::Compact));
    if (!file.commit())
    {
        strError = QStringLiteral("라벨 파일을 저장하지 못했다: %1").arg(file.errorString());
        return false;
    }
    return true;
}

bool Load(const QString& path, LabelDocument& document, QString& strError, int* pUnsavedEditCount)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        strError = QStringLiteral("라벨 파일을 열 수 없다: %1").arg(file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument jsonDocument = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !jsonDocument.isObject())
    {
        strError = QStringLiteral("라벨 파일이 JSON 형식이 아니다: %1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject objRoot = jsonDocument.object();
    if (objRoot.value(QStringLiteral("format")).toString() != kFormatName)
    {
        strError = QStringLiteral("DentalLabeler 라벨 파일이 아니다.");
        return false;
    }
    if (objRoot.value(QStringLiteral("version")).toInt() > kFormatVersion)
    {
        strError = QStringLiteral("더 새 버전의 앱이 만든 라벨 파일이다 (version %1).")
                       .arg(objRoot.value(QStringLiteral("version")).toInt());
        return false;
    }

    const QJsonObject objSource = objRoot.value(QStringLiteral("source")).toObject();
    SourceInfo source;
    source.m_strFile = objSource.value(QStringLiteral("file")).toString();
    source.m_iVertexCount = objSource.value(QStringLiteral("vertex_count")).toInt();
    source.m_iFaceCount = objSource.value(QStringLiteral("face_count")).toInt();
    source.m_strSha256 = objSource.value(QStringLiteral("sha256")).toString();

    const QJsonArray arrLabels = objRoot.value(QStringLiteral("labels")).toArray();
    if (arrLabels.size() != source.m_iVertexCount)
    {
        strError = QStringLiteral("라벨 수(%L1)가 기록된 정점 수(%L2)와 다르다.")
                       .arg(arrLabels.size())
                       .arg(source.m_iVertexCount);
        return false;
    }

    std::vector<LabelValue> vecLabels;
    vecLabels.reserve(static_cast<size_t>(arrLabels.size()));
    for (const QJsonValue& value : arrLabels)
    {
        vecLabels.push_back(static_cast<LabelValue>(value.toInt(kUnlabeled)));
    }

    LabelDocument loaded;
    loaded.Reset(source, LabelTable::ParseJawKey(objRoot.value(QStringLiteral("jaw")).toString()));
    if (!loaded.SetLabels(std::move(vecLabels)))
    {
        strError = QStringLiteral("라벨 파일에 FDI 번호가 아닌 값이 있다.");
        return false;
    }

    const QJsonObject objTeeth = objRoot.value(QStringLiteral("teeth")).toObject();
    for (auto it = objTeeth.begin(); it != objTeeth.end(); ++it)
    {
        bool bOk = false;
        const int iLabel = it.key().toInt(&bOk);
        if (!bOk || !LabelTable::IsTooth(iLabel))
        {
            continue;
        }
        const QJsonObject objTooth = it.value().toObject();
        ToothInfo info;
        if (!LabelTable::ParseToothStateKey(objTooth.value(QStringLiteral("state")).toString(), info.m_eState))
        {
            info.m_eState = EToothState::NORMAL;
        }
        info.m_strNote = objTooth.value(QStringLiteral("note")).toString();
        loaded.SetTooth(iLabel, info);
    }

    loaded.SetStatus(objRoot.value(QStringLiteral("status")).toString(QStringLiteral("new")));
    loaded.SetAssignee(objRoot.value(QStringLiteral("assignee")).toString());
    loaded.SetUpdatedAt(QDateTime::fromString(objRoot.value(QStringLiteral("updated_at")).toString(), Qt::ISODate));

    if (pUnsavedEditCount != nullptr)
    {
        *pUnsavedEditCount = objRoot.value(QStringLiteral("unsaved_edit_count")).toInt(0);
    }

    document = std::move(loaded);
    return true;
}
}
