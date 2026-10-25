#pragma once

#include "Core/Types.h"

#include <QColor>
#include <QString>

#include <vector>

// FDI 번호 · 이름 · 기본 색 (docs/ScreenPlan.html 3.4)
namespace LabelTable
{
bool IsTooth(int label);
bool IsValidLabel(int label);
EJaw GetJaw(int label);

QColor GetColor(int label);
QString GetName(int label);
QString GetToothTypeName(int label);

// 치식 표기 순서 (환자 오른쪽이 화면 왼쪽). 하악: 48 … 41, 31 … 38
const std::vector<int>& GetArchOrder(EJaw jaw);

QString GetToothStateName(EToothState state);
QString GetToothStateKey(EToothState state);
bool ParseToothStateKey(const QString& key, EToothState& state);

QString GetJawName(EJaw jaw);
QString GetJawKey(EJaw jaw);
EJaw ParseJawKey(const QString& key);
EJaw GuessJawFromFileName(const QString& fileName);
}
