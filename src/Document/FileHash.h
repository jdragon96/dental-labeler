#pragma once

#include <QString>

namespace FileHash
{
// 파일 전체의 SHA-256 (소문자 16진수). 읽을 수 없으면 빈 문자열
QString ComputeSha256(const QString& path);
}
