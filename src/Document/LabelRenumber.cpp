#include "Document/LabelRenumber.h"

#include "Core/LabelTable.h"

#include <algorithm>
#include <cstdlib>
#include <set>

bool RenumberPlan::IsValid() const
{
    return m_strError.isEmpty() && !m_mapCodes.empty();
}

std::vector<std::pair<int, int>> RenumberPlan::GetOrderedPairs() const
{
    auto sortKey = [](int code) {
        const int iFdi = LabelTable::GetFdiOf(code);
        if (!LabelTable::IsFdi(iFdi))
        {
            return LabelTable::GetClassOf(code) * 100 + 99;
        }
        const std::vector<int>& vecOrder = LabelTable::GetArchOrder(LabelTable::GetJawOfFdi(iFdi));
        return LabelTable::GetClassOf(code) * 100 + static_cast<int>(std::find(vecOrder.begin(), vecOrder.end(), iFdi) - vecOrder.begin());
    };
    std::vector<std::pair<int, int>> vecPairs(m_mapCodes.begin(), m_mapCodes.end());
    std::stable_sort(vecPairs.begin(), vecPairs.end(),
                     [&](const std::pair<int, int>& a, const std::pair<int, int>& b) { return sortKey(a.first) < sortKey(b.first); });
    return vecPairs;
}

QString RenumberPlan::Describe(int maxItems) const
{
    QStringList lstItems;
    for (const auto& it : GetOrderedPairs())
    {
        if (lstItems.size() == maxItems)
        {
            lstItems << QStringLiteral("외 %1개").arg(static_cast<int>(m_mapCodes.size()) - maxItems);
            break;
        }
        lstItems << QStringLiteral("%1 → %2").arg(LabelTable::GetShortText(it.first), LabelTable::GetShortText(it.second));
    }
    return lstItems.join(QStringLiteral(" · "));
}

namespace
{
void AddMergeWarnings(RenumberPlan& plan, const std::set<int>& setPresent)
{
    for (const auto& it : plan.GetOrderedPairs())
    {
        // 옮겨 가는 자리에 이미 정점이 있고, 그 정점은 옮겨지지 않으면 둘이 합쳐진다
        if (setPresent.count(it.second) > 0 && plan.m_mapCodes.count(it.second) == 0)
        {
            plan.m_lstWarnings << QStringLiteral("%1 에 이미 정점이 있어 %2 와 합쳐진다.")
                                      .arg(LabelTable::GetShortText(it.second), LabelTable::GetShortText(it.first));
        }
    }
}
}

namespace LabelRenumber
{
RenumberPlan PlanArchShift(EJaw jaw, int firstFdi, int lastFdi, int steps, const std::vector<int>& vecPresentCodes)
{
    RenumberPlan plan;
    const EJaw eJaw = jaw != EJaw::UNKNOWN ? jaw : LabelTable::GetJawOfFdi(firstFdi);
    if (!LabelTable::IsFdi(firstFdi) || !LabelTable::IsFdi(lastFdi) || LabelTable::GetJawOfFdi(firstFdi) != eJaw
        || LabelTable::GetJawOfFdi(lastFdi) != eJaw)
    {
        plan.m_strError = QStringLiteral("같은 턱의 치아 번호를 고른다.");
        return plan;
    }
    if (steps == 0)
    {
        plan.m_strError = QStringLiteral("몇 칸 옮길지 고른다.");
        return plan;
    }

    const std::vector<int>& vecOrder = LabelTable::GetArchOrder(eJaw);
    const int iCount = static_cast<int>(vecOrder.size());
    int iFirst = static_cast<int>(std::find(vecOrder.begin(), vecOrder.end(), firstFdi) - vecOrder.begin());
    int iLast = static_cast<int>(std::find(vecOrder.begin(), vecOrder.end(), lastFdi) - vecOrder.begin());
    if (iFirst > iLast)
    {
        std::swap(iFirst, iLast);
    }

    const std::set<int> setPresent(vecPresentCodes.begin(), vecPresentCodes.end());
    for (int iIndex = iFirst; iIndex <= iLast; ++iIndex)
    {
        const int iTarget = iIndex + steps;
        const int iFdi = vecOrder[static_cast<size_t>(iIndex)];
        bool bHasAny = false;
        for (const LabelTable::ClassInfo& info : LabelTable::GetClasses())
        {
            if (info.m_eFdiUse != LabelTable::EFdiUse::NONE && setPresent.count(LabelTable::MakeCode(info.m_iValue, iFdi)) > 0)
            {
                bHasAny = true;
            }
        }
        if (!bHasAny)
        {
            continue;
        }
        if (iTarget < 0 || iTarget >= iCount)
        {
            plan.m_mapCodes.clear();
            plan.m_strError = QStringLiteral("%1 을 %2칸 옮길 자리가 없다. 범위나 방향을 확인한다.").arg(iFdi).arg(std::abs(steps));
            return plan;
        }
        const int iTargetFdi = vecOrder[static_cast<size_t>(iTarget)];
        for (const LabelTable::ClassInfo& info : LabelTable::GetClasses())
        {
            const int iFrom = LabelTable::MakeCode(info.m_iValue, iFdi);
            if (info.m_eFdiUse != LabelTable::EFdiUse::NONE && setPresent.count(iFrom) > 0)
            {
                plan.m_mapCodes[iFrom] = LabelTable::MakeCode(info.m_iValue, iTargetFdi);
            }
        }
    }

    if (plan.m_mapCodes.empty())
    {
        plan.m_strError = QStringLiteral("고른 범위에 칠한 치아가 없다.");
        return plan;
    }
    AddMergeWarnings(plan, setPresent);
    return plan;
}

RenumberPlan PlanSingle(int fromCode, int toCode, bool swap, const std::vector<int>& vecPresentCodes)
{
    RenumberPlan plan;
    if (fromCode == toCode)
    {
        plan.m_strError = QStringLiteral("바꿀 라벨과 새 라벨이 같다.");
        return plan;
    }
    if (!LabelTable::IsValidCode(fromCode) || !LabelTable::IsValidCode(toCode) || fromCode == kUnlabeled || toCode == kUnlabeled)
    {
        plan.m_strError = QStringLiteral("쓸 수 없는 라벨이다.");
        return plan;
    }
    const std::set<int> setPresent(vecPresentCodes.begin(), vecPresentCodes.end());
    if (setPresent.count(fromCode) == 0)
    {
        plan.m_strError = QStringLiteral("%1 에 정점이 없다.").arg(LabelTable::GetShortText(fromCode));
        return plan;
    }

    plan.m_mapCodes[fromCode] = toCode;
    if (swap && setPresent.count(toCode) > 0)
    {
        plan.m_mapCodes[toCode] = fromCode;
    }
    AddMergeWarnings(plan, setPresent);
    return plan;
}
}
