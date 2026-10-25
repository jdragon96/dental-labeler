#include "Algorithm/LabelPaint/Common/PaintPolicy.h"

#include "Document/Mesh.h"

namespace PaintPolicy
{
bool CanPaint(const PaintInput& in, LabelValue current)
{
    if (current == in.m_target)
    {
        return false;
    }
    if (in.m_kpLockedLabels != nullptr && in.m_kpLockedLabels->count(current) > 0)
    {
        return false;
    }
    if (in.m_kpHiddenLabels != nullptr && in.m_kpHiddenLabels->count(current) > 0)
    {
        return false;
    }

    switch (in.m_eOverwrite)
    {
    case EOverwriteRule::ALL:
        return true;
    case EOverwriteRule::UNLABELED_AND_GINGIVA:
        return current == kUnlabeled || current == kGingiva;
    case EOverwriteRule::UNLABELED_ONLY:
        return current == kUnlabeled;
    }
    return false;
}

bool IsFacingCamera(const PaintInput& in, int vertex)
{
    const Mesh& mesh = *in.m_kpMesh;
    if (!mesh.HasNormals())
    {
        return true;
    }
    return QVector3D::dotProduct(mesh.GetNormal(vertex), in.m_v3Camera - mesh.GetPosition(vertex)) > 0.0f;
}
}
