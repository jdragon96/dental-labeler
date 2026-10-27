#pragma once

#include "Core/Types.h"

#include <QString>
#include <QStringList>

#include <map>
#include <utility>
#include <vector>

// 번호 바꾸기 계획 (DL-21). 라벨 코드 → 라벨 코드 대응표와 알릴 것
struct RenumberPlan
{
public:
    bool IsValid() const;
    // 치식 순서(클래스 → 자리)로 늘어놓은 대응. 미리 보기 · 기록 이름에 쓴다
    std::vector<std::pair<int, int>> GetOrderedPairs() const;
    // "48 → 47 · 47 → 46 …"
    QString Describe(int maxItems = 4) const;

public:
    std::map<int, int> m_mapCodes;
    QStringList m_lstWarnings;
    QString m_strError;
};

namespace LabelRenumber
{
// 치식 순서(하악 48 … 41, 31 … 38)로 [firstFdi, lastFdi] 자리를 steps 칸 옮긴다.
// steps > 0 이면 치식 순서 뒤쪽(하악 48 → 47, 41 → 31), < 0 이면 앞쪽. 그 자리의 치아 · 스캔바디를 함께 옮긴다
RenumberPlan PlanArchShift(EJaw jaw, int firstFdi, int lastFdi, int steps, const std::vector<int>& vecPresentCodes);
// fromCode 를 toCode 로 바꾼다. swap 이면 toCode 에 있던 정점은 fromCode 로 간다, 아니면 합쳐진다
RenumberPlan PlanSingle(int fromCode, int toCode, bool swap, const std::vector<int>& vecPresentCodes);
}
