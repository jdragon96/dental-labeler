#include "Document/EditHistory.h"

#include "Document/LabelDocument.h"

EditHistory::EditHistory(int maxSteps)
    : m_deqEdits()
    , m_iApplied(0)
    , m_iMaxSteps(maxSteps)
    , m_llNextId(1)
    , m_llBaseStateId(0)
{
}

void EditHistory::Clear()
{
    m_deqEdits.clear();
    m_iApplied = 0;
    m_llBaseStateId = 0;
}

void EditHistory::Push(LabelEdit&& edit)
{
    // 되돌린 뒤 새로 편집하면 다시 실행할 수 있던 기록은 버린다
    m_deqEdits.erase(m_deqEdits.begin() + m_iApplied, m_deqEdits.end());

    edit.m_llId = m_llNextId++;
    if (!edit.m_dtTime.isValid())
    {
        edit.m_dtTime = QDateTime::currentDateTime();
    }
    m_deqEdits.push_back(std::move(edit));
    ++m_iApplied;

    while (static_cast<int>(m_deqEdits.size()) > m_iMaxSteps)
    {
        m_llBaseStateId = m_deqEdits.front().m_llId;
        m_deqEdits.pop_front();
        --m_iApplied;
    }
}

const LabelEdit* EditHistory::Undo(LabelDocument& document)
{
    if (!CanUndo())
    {
        return nullptr;
    }
    --m_iApplied;
    const LabelEdit& edit = m_deqEdits[static_cast<size_t>(m_iApplied)];
    document.Assign(edit.m_vecIndices, edit.m_vecBefore);
    return &edit;
}

const LabelEdit* EditHistory::Redo(LabelDocument& document)
{
    if (!CanRedo())
    {
        return nullptr;
    }
    const LabelEdit& edit = m_deqEdits[static_cast<size_t>(m_iApplied)];
    ++m_iApplied;
    document.Assign(edit.m_vecIndices, edit.m_vecAfter);
    return &edit;
}

bool EditHistory::CanUndo() const
{
    return m_iApplied > 0;
}

bool EditHistory::CanRedo() const
{
    return m_iApplied < static_cast<int>(m_deqEdits.size());
}

int EditHistory::GetAppliedCount() const
{
    return m_iApplied;
}

const std::deque<LabelEdit>& EditHistory::GetEdits() const
{
    return m_deqEdits;
}

long long EditHistory::GetStateId() const
{
    if (m_iApplied == 0)
    {
        return m_llBaseStateId;
    }
    return m_deqEdits[static_cast<size_t>(m_iApplied - 1)].m_llId;
}
