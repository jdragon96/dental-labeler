#pragma once

#include "Algorithm/Geometry/Bvh.h"
#include "Algorithm/Geometry/DentalFrame.h"
#include "Algorithm/Geometry/MeshTopology.h"
#include "Core/Types.h"
#include "Document/LabelDocument.h"
#include "Document/Mesh.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

// 열린 케이스 하나: 원본 메시와 계산해 둔 구조, 라벨 문서
struct CaseData
{
    // 원본 메시. 앱은 이 파일을 쓰지 않는다 (원본 없이 라벨 파일만 열었을 때는 라벨 파일)
    QString m_strMeshPath;
    // 저장 · 자동 저장할 곳: <이름>.labeled.ply, <이름>.autosave.ply
    QString m_strLabeledPath;
    QString m_strAutosavePath;
    bool m_bStandalone = false;
    QString m_strCaseId;
    // 원본 헤더의 comment (스캐너 정보 등). 라벨 파일에도 그대로 옮긴다
    QStringList m_lstSourceComments;
    // 열 때 사용자에게 알릴 것 (예: 원본에 있던 label 필드를 불러왔다)
    QString m_strLoadNotice;
    EJaw m_eJaw = EJaw::UNKNOWN;

    Mesh m_mesh;
    MeshTopology m_topology;
    Bvh m_bvh;
    std::vector<float> m_vecCurvature;
    DentalFrame m_frame;

    LabelDocument m_document;
    bool m_bLabelFileLoaded = false;
    // 열자마자 저장이 필요한 상태 (이전 형식에서 넘겨받은 라벨)
    bool m_bNeedsSave = false;

    // 마지막 저장보다 새 자동 저장본이 있으면 채운다 (DL-90 ③)
    std::unique_ptr<LabelDocument> m_upRecovery;
    QDateTime m_dtRecovery;
    int m_iRecoveryEditCount = 0;
};
