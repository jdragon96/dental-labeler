#pragma once

#include "Core/Types.h"

#include <QDateTime>
#include <QString>

#include <cstdint>
#include <map>
#include <set>
#include <vector>

// 라벨 파일이 기억하는 원본 메시 정보. 다르면 라벨을 엉뚱한 정점에 붙이게 되므로 열 때 비교한다
struct SourceInfo
{
    QString m_strFile;
    int m_iVertexCount = 0;
    int m_iFaceCount = 0;
    QString m_strSha256;
};

struct ToothInfo
{
    EToothState m_eState = EToothState::NORMAL;
    QString m_strNote;
};

// 케이스 하나의 정점 라벨, 치아 상태, 작업 상태
class LabelDocument
{
public:
    LabelDocument();

public:
    void Reset(const SourceInfo& source, EJaw jaw);
    // 라벨 배열을 통째로 바꾼다 (파일에서 읽을 때). 값이 잘못되면 false
    bool SetLabels(std::vector<LabelValue>&& vecLabels);
    void Assign(const std::vector<std::uint32_t>& vecIndices, LabelValue value);
    void Assign(const std::vector<std::uint32_t>& vecIndices, const std::vector<LabelValue>& vecValues);

public:
    const std::vector<LabelValue>& GetLabels() const;
    LabelValue GetLabel(int vertex) const;
    int GetVertexCount() const;
    int GetCount(int label) const;
    // 정점이 하나라도 있는 코드 (미분류 제외), 작은 코드부터
    std::vector<int> GetPresentCodes() const;
    int GetLabeledCount() const;
    double GetProgress() const;

    const SourceInfo& GetSource() const;
    void SetSource(const SourceInfo& source);
    EJaw GetJaw() const;
    void SetJaw(EJaw jaw);

    // 치아 상태 · 메모는 FDI 자리(11–48)마다 둔다
    ToothInfo GetTooth(int fdi) const;
    void SetTooth(int fdi, const ToothInfo& info);
    const std::map<int, ToothInfo>& GetTeeth() const;

    // 작업 중 보조 상태 (파일에 저장하지 않는다)
    bool IsLocked(int label) const;
    void SetLocked(int label, bool locked);
    const std::set<int>& GetLockedLabels() const;
    bool IsHidden(int label) const;
    void SetHidden(int label, bool hidden);
    void ClearHidden();
    const std::set<int>& GetHiddenLabels() const;

    const QString& GetStatus() const;
    void SetStatus(const QString& status);
    const QString& GetAssignee() const;
    void SetAssignee(const QString& assignee);
    const QDateTime& GetUpdatedAt() const;
    void SetUpdatedAt(const QDateTime& updatedAt);

private:
    static size_t GetCountSlot(int label);
    void SetOne(std::uint32_t vertex, LabelValue value);
    void RecountAll();

private:
    std::vector<LabelValue> m_vecLabels;
    // 코드 -1 … 999 마다 정점 수
    std::vector<int> m_vecCounts;
    SourceInfo m_source;
    EJaw m_eJaw;
    std::map<int, ToothInfo> m_mapTeeth;
    std::set<int> m_setLockedLabels;
    std::set<int> m_setHiddenLabels;
    QString m_strStatus;
    QString m_strAssignee;
    QDateTime m_dtUpdatedAt;
};
