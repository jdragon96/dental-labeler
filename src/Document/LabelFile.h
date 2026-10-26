#pragma once

#include "Core/Types.h"
#include "Document/PlyWriter.h"

#include <QString>
#include <QStringList>

#include <vector>

class LabelDocument;
class PlyReader;
struct Mesh;
struct SourceInfo;

// 라벨 파일 (docs/ScreenPlan.html 부록 B).
// 원본 <이름>.ply 는 읽기만 하고, 라벨은 원본 정점 · 면을 복사한 <이름>.labeled.ply 에 정점마다 세 필드로 쓴다.
//   property int fdi       치아 번호 (11–48, 없으면 0)
//   property int label     클래스 (-1 미분류, 0 치은, 1 치아, 2 스캔바디 …)
//   property int instance  대상 번호 (치은 · 미분류 0, 그 밖은 대상마다 1부터)
// 치아 상태 · 메모 · 작업 상태는 헤더의 "comment dentallabeler …" 줄에 쓴다.
namespace LabelFile
{
struct CasePaths
{
    // 형상을 읽을 파일. 원본이 없고 라벨 파일만 있으면 라벨 파일 자체
    QString m_strMesh;
    QString m_strLabeled;
    QString m_strAutosave;
    // 이전 형식 (<이름>.labels.json). 새 라벨 파일이 없을 때만 읽고, 지우지 않는다
    QString m_strLegacyJson;
    QString m_strCaseId;
    // 원본 없이 라벨 파일만 연 경우 (m_strMesh == m_strLabeled)
    bool m_bStandalone = false;
};

// 연 파일이 원본이든 <이름>.labeled.ply 든 같은 케이스 경로로 맞춘다
CasePaths ResolveCasePaths(const QString& openedPath);

// 정점 코드(LabelTable)를 fdi · label · instance 필드로 푼다.
// instance 는 대상(클래스 + FDI 자리)마다 클래스 순서, 같은 클래스 안에서는 치식 순서로 1부터 매긴다
std::vector<PlyIntField> EncodeFields(const std::vector<LabelValue>& vecCodes);
// 읽은 필드를 정점 코드로 묶는다. label + fdi 가 있으면 지금 형식, label 만 있으면 이전 형식(label = FDI 번호).
// instance 는 코드에서 다시 매기므로 읽지 않는다
bool DecodeFields(const PlyReader& reader, std::vector<int>& vecCodes, QString& strError);
bool HasLabelFields(const PlyReader& reader);

bool Save(const QString& path, const Mesh& mesh, const QStringList& lstSourceComments, const LabelDocument& document,
          QString& strError, int unsavedEditCount = -1);
// 라벨 PLY 를 읽어 문서와 형상을 넘긴다. pUnsavedEditCount 는 자동 저장본일 때 쓰인 저장 안 된 편집 수
bool Load(const QString& path, LabelDocument& document, Mesh& mesh, QString& strError, int* pUnsavedEditCount = nullptr);
// 정점 코드와 comment 로 문서를 만든다. comment 에 출처가 없으면 fallbackSource 를 쓴다
bool BuildDocument(const std::vector<int>& vecCodes, const QStringList& lstComments, const SourceInfo& fallbackSource,
                   LabelDocument& document, QString& strError, int* pUnsavedEditCount = nullptr);
// DentalLabeler 가 붙인 comment 를 뺀 나머지 (스캐너가 남긴 comment)
QStringList StripOwnComments(const QStringList& lstComments);
// 이전 형식 <이름>.labels.json (2026-10-25 M1, labels = FDI 번호) 을 읽는다
bool LoadLegacyJson(const QString& path, LabelDocument& document, QString& strError);
}
