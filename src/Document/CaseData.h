#pragma once

#include "Algorithm/Geometry/Bvh.h"
#include "Algorithm/Geometry/DentalFrame.h"
#include "Algorithm/Geometry/MeshTopology.h"
#include "Core/Types.h"
#include "Document/LabelDocument.h"
#include "Document/Mesh.h"

#include <QDateTime>
#include <QString>

#include <memory>
#include <vector>

// 열린 케이스 하나: 원본 메시와 계산해 둔 구조, 라벨 문서
struct CaseData
{
    QString m_strMeshPath;
    QString m_strCaseId;
    QString m_strScannerComment;
    EJaw m_eJaw = EJaw::UNKNOWN;

    Mesh m_mesh;
    MeshTopology m_topology;
    Bvh m_bvh;
    std::vector<float> m_vecCurvature;
    DentalFrame m_frame;

    LabelDocument m_document;
    bool m_bLabelFileLoaded = false;

    // 마지막 저장보다 새 자동 저장본이 있으면 채운다 (DL-90 ③)
    std::unique_ptr<LabelDocument> m_upRecovery;
    QDateTime m_dtRecovery;
    int m_iRecoveryEditCount = 0;
};
