#pragma once

#include "Core/Types.h"

#include <QDateTime>
#include <QString>

#include <cstdint>
#include <deque>
#include <vector>

class LabelDocument;

// 한 번의 스트로크나 명령으로 바뀐 정점과 전후 라벨
struct LabelEdit
{
    QString m_strName;
    QDateTime m_dtTime;
    std::vector<std::uint32_t> m_vecIndices;
    std::vector<LabelValue> m_vecBefore;
    std::vector<LabelValue> m_vecAfter;
    long long m_llId = 0;
};

// 실행 취소 기록. 오래된 것부터 버린다
class EditHistory
{
public:
    explicit EditHistory(int maxSteps = 500);

public:
    void Clear();
    void Push(LabelEdit&& edit);
    const LabelEdit* Undo(LabelDocument& document);
    const LabelEdit* Redo(LabelDocument& document);

public:
    bool CanUndo() const;
    bool CanRedo() const;
    // 적용된 편집 수 (0 … GetEdits().size())
    int GetAppliedCount() const;
    const std::deque<LabelEdit>& GetEdits() const;
    // 지금 상태를 가리키는 값. 저장할 때의 값과 비교해 저장 안 된 변경이 있는지 본다
    long long GetStateId() const;

private:
    std::deque<LabelEdit> m_deqEdits;
    int m_iApplied;
    int m_iMaxSteps;
    long long m_llNextId;
    long long m_llBaseStateId;
};
