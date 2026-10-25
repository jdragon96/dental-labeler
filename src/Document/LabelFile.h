#pragma once

#include <QString>

class LabelDocument;

// 라벨 파일 (docs/ScreenPlan.html 부록 B). 원본 메시 옆 <이름>.labels.json
namespace LabelFile
{
QString GetLabelPath(const QString& meshPath);
QString GetAutosavePath(const QString& meshPath);

// unsavedEditCount 가 0 이상이면 자동 저장본으로 쓴다 (마지막 저장 뒤 편집 수를 함께 남긴다)
bool Save(const QString& path, const LabelDocument& document, QString& strError, int unsavedEditCount = -1);
bool Load(const QString& path, LabelDocument& document, QString& strError, int* pUnsavedEditCount = nullptr);
}
