#pragma once

#include "Core/Types.h"

#include <QString>
#include <QVector3D>

// 커서 아래 표면
struct PickResult
{
    bool m_bHit = false;
    int m_iVertex = -1;
    int m_iFace = -1;
    QVector3D m_v3Point;
    LabelValue m_label = kUnlabeled;
    float m_fCurvature = 0.0f;
};

// 라벨 목록 · 치식도 한 칸
struct LabelRowData
{
    int m_iLabel = kUnlabeled;
    QString m_strName;
    int m_iCount = 0;
    EToothState m_eState = EToothState::NORMAL;
    QString m_strNote;
    bool m_bLocked = false;
    bool m_bHidden = false;
};

struct HistoryRowData
{
    QString m_strTime;
    QString m_strName;
    int m_iVertexCount = 0;
    bool m_bApplied = true;
};
