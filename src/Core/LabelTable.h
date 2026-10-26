#pragma once

#include "Core/Types.h"

#include <QColor>
#include <QString>

#include <vector>

// 정점 라벨 코드와 클래스 표 (docs/ScreenPlan.html 3.4, 부록 B).
//
// 앱 안에서는 정점마다 코드 하나를 둔다: -1 미분류, 그 밖에는 클래스 × 100 + FDI (위치가 없으면 0).
//   0 치은 · 136 치아 36 · 236 36 자리 스캔바디 · 200 위치 없는 스캔바디
// 파일에는 이 코드를 fdi · label(클래스) · instance 세 필드로 풀어 쓴다.
namespace LabelTable
{
constexpr int kClassGingiva = 0;
constexpr int kClassTooth = 1;
constexpr int kClassScanbody = 2;
constexpr int kCodeClassStride = 100;
constexpr int kMaxClass = 9;
// 코드 -1 … 999 를 담는 칸 수
constexpr int kCodeSlotCount = (kMaxClass + 1) * kCodeClassStride + 1;

enum class EFdiUse
{
    NONE,
    REQUIRED,
    ALLOWED,
};

struct ClassInfo
{
    int m_iValue;
    const char* m_kpKey;
    const char* m_kpName;
    EFdiUse m_eFdiUse;
};

// 클래스를 더하려면 이 표에 한 줄 넣는다 (LabelTable.cpp)
const std::vector<ClassInfo>& GetClasses();
const ClassInfo* FindClass(int classValue);
QString GetClassName(int classValue);

int MakeCode(int classValue, int fdi);
// 미분류는 -1
int GetClassOf(int code);
// 위치가 없으면 0
int GetFdiOf(int code);
bool IsValidCode(int code);
bool IsToothCode(int code);

// FDI 번호(11–48) 자체
bool IsFdi(int fdi);
EJaw GetJawOfFdi(int fdi);
QString GetFdiName(int fdi);
QString GetFdiTypeName(int fdi);
// 치식 표기 순서 FDI (환자 오른쪽이 화면 왼쪽). 하악: 48 … 41, 31 … 38
const std::vector<int>& GetArchOrder(EJaw jaw);

// 코드 기준
EJaw GetJaw(int code);
QColor GetColor(int code);
// "하악 좌측 제2대구치", "36 자리 스캔바디", "치은", "미분류"
QString GetName(int code);
// 목록 · 칩에 쓰는 짧은 표기: "36", "SB36", "SB", "0", "−1"
QString GetShortText(int code);

QString GetToothStateName(EToothState state);
QString GetToothStateKey(EToothState state);
bool ParseToothStateKey(const QString& key, EToothState& state);

QString GetJawName(EJaw jaw);
QString GetJawKey(EJaw jaw);
EJaw ParseJawKey(const QString& key);
EJaw GuessJawFromFileName(const QString& fileName);
}
