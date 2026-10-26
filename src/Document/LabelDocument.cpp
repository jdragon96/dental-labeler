#include "Document/LabelDocument.h"

#include "Core/LabelTable.h"

#include <algorithm>

LabelDocument::LabelDocument()
    : m_vecLabels()
    , m_vecCounts(static_cast<size_t>(LabelTable::kCodeSlotCount), 0)
    , m_source()
    , m_eJaw(EJaw::UNKNOWN)
    , m_mapTeeth()
    , m_setLockedLabels()
    , m_setHiddenLabels()
    , m_strStatus(QStringLiteral("new"))
    , m_strAssignee()
    , m_dtUpdatedAt()
{
}

void LabelDocument::Reset(const SourceInfo& source, EJaw jaw)
{
    m_source = source;
    m_eJaw = jaw;
    m_vecLabels.assign(static_cast<size_t>(source.m_iVertexCount), kUnlabeled);
    m_mapTeeth.clear();
    m_setLockedLabels.clear();
    m_setHiddenLabels.clear();
    m_strStatus = QStringLiteral("new");
    m_dtUpdatedAt = QDateTime();
    RecountAll();
}

bool LabelDocument::SetLabels(std::vector<LabelValue>&& vecLabels)
{
    for (LabelValue value : vecLabels)
    {
        if (!LabelTable::IsValidCode(value))
        {
            return false;
        }
    }
    m_vecLabels = std::move(vecLabels);
    RecountAll();
    return true;
}

void LabelDocument::Assign(const std::vector<std::uint32_t>& vecIndices, LabelValue value)
{
    for (std::uint32_t uVertex : vecIndices)
    {
        SetOne(uVertex, value);
    }
}

void LabelDocument::Assign(const std::vector<std::uint32_t>& vecIndices, const std::vector<LabelValue>& vecValues)
{
    const size_t iCount = std::min(vecIndices.size(), vecValues.size());
    for (size_t i = 0; i < iCount; ++i)
    {
        SetOne(vecIndices[i], vecValues[i]);
    }
}

const std::vector<LabelValue>& LabelDocument::GetLabels() const
{
    return m_vecLabels;
}

LabelValue LabelDocument::GetLabel(int vertex) const
{
    return m_vecLabels[static_cast<size_t>(vertex)];
}

int LabelDocument::GetVertexCount() const
{
    return static_cast<int>(m_vecLabels.size());
}

int LabelDocument::GetCount(int label) const
{
    if (!LabelTable::IsValidCode(label))
    {
        return 0;
    }
    return m_vecCounts[GetCountSlot(label)];
}

int LabelDocument::GetLabeledCount() const
{
    return GetVertexCount() - GetCount(kUnlabeled);
}

double LabelDocument::GetProgress() const
{
    if (m_vecLabels.empty())
    {
        return 0.0;
    }
    return static_cast<double>(GetLabeledCount()) / static_cast<double>(m_vecLabels.size());
}

const SourceInfo& LabelDocument::GetSource() const
{
    return m_source;
}

void LabelDocument::SetSource(const SourceInfo& source)
{
    m_source = source;
}

EJaw LabelDocument::GetJaw() const
{
    return m_eJaw;
}

void LabelDocument::SetJaw(EJaw jaw)
{
    m_eJaw = jaw;
}

ToothInfo LabelDocument::GetTooth(int label) const
{
    auto it = m_mapTeeth.find(label);
    if (it == m_mapTeeth.end())
    {
        return ToothInfo();
    }
    return it->second;
}

void LabelDocument::SetTooth(int label, const ToothInfo& info)
{
    if (info.m_eState == EToothState::NORMAL && info.m_strNote.isEmpty())
    {
        m_mapTeeth.erase(label);
        return;
    }
    m_mapTeeth[label] = info;
}

const std::map<int, ToothInfo>& LabelDocument::GetTeeth() const
{
    return m_mapTeeth;
}

bool LabelDocument::IsLocked(int label) const
{
    return m_setLockedLabels.count(label) > 0;
}

void LabelDocument::SetLocked(int label, bool locked)
{
    if (locked)
    {
        m_setLockedLabels.insert(label);
    }
    else
    {
        m_setLockedLabels.erase(label);
    }
}

const std::set<int>& LabelDocument::GetLockedLabels() const
{
    return m_setLockedLabels;
}

bool LabelDocument::IsHidden(int label) const
{
    return m_setHiddenLabels.count(label) > 0;
}

void LabelDocument::SetHidden(int label, bool hidden)
{
    if (hidden)
    {
        m_setHiddenLabels.insert(label);
    }
    else
    {
        m_setHiddenLabels.erase(label);
    }
}

void LabelDocument::ClearHidden()
{
    m_setHiddenLabels.clear();
}

const std::set<int>& LabelDocument::GetHiddenLabels() const
{
    return m_setHiddenLabels;
}

const QString& LabelDocument::GetStatus() const
{
    return m_strStatus;
}

void LabelDocument::SetStatus(const QString& status)
{
    m_strStatus = status;
}

const QString& LabelDocument::GetAssignee() const
{
    return m_strAssignee;
}

void LabelDocument::SetAssignee(const QString& assignee)
{
    m_strAssignee = assignee;
}

const QDateTime& LabelDocument::GetUpdatedAt() const
{
    return m_dtUpdatedAt;
}

void LabelDocument::SetUpdatedAt(const QDateTime& updatedAt)
{
    m_dtUpdatedAt = updatedAt;
}

size_t LabelDocument::GetCountSlot(int label)
{
    // 코드 -1 … 999 를 0 … 1000 칸에 둔다
    return static_cast<size_t>(label + 1);
}

std::vector<int> LabelDocument::GetPresentCodes() const
{
    std::vector<int> vecCodes;
    for (size_t i = 1; i < m_vecCounts.size(); ++i)
    {
        if (m_vecCounts[i] > 0)
        {
            vecCodes.push_back(static_cast<int>(i) - 1);
        }
    }
    return vecCodes;
}

void LabelDocument::SetOne(std::uint32_t vertex, LabelValue value)
{
    LabelValue& current = m_vecLabels[vertex];
    if (current == value)
    {
        return;
    }
    --m_vecCounts[GetCountSlot(current)];
    ++m_vecCounts[GetCountSlot(value)];
    current = value;
}

void LabelDocument::RecountAll()
{
    std::fill(m_vecCounts.begin(), m_vecCounts.end(), 0);
    for (LabelValue value : m_vecLabels)
    {
        ++m_vecCounts[GetCountSlot(value)];
    }
}
