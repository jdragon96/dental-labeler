#include "Algorithm/LabelPaint/LabelPaint.h"

#include "Algorithm/LabelPaint/Strategy/BrushPaintCPU.h"
#include "Algorithm/LabelPaint/Strategy/FillPaintCPU.h"
#include "Algorithm/LabelPaint/Strategy/LassoPaintCPU.h"

LabelPaint::LabelPaint()
    : m_hashStrategies()
    , m_pCurrent(nullptr)
{
    m_hashStrategies[EPaintType::BRUSH] = std::make_unique<BrushPaintCPU>();
    m_hashStrategies[EPaintType::FILL] = std::make_unique<FillPaintCPU>();
    m_hashStrategies[EPaintType::LASSO] = std::make_unique<LassoPaintCPU>();
    m_pCurrent = m_hashStrategies[EPaintType::BRUSH].get();
}

LabelPaint::~LabelPaint() = default;

void LabelPaint::SetStrategy(EPaintType type)
{
    auto it = m_hashStrategies.find(type);
    if (it != m_hashStrategies.end())
    {
        m_pCurrent = it->second.get();
    }
}

bool LabelPaint::Execute(const PaintInput& in, PaintOutput& out)
{
    if (m_pCurrent == nullptr)
    {
        return false;
    }
    return m_pCurrent->Execute(in, out);
}
