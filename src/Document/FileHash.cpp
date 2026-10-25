#include "Document/FileHash.h"

#include <QCryptographicHash>
#include <QFile>

namespace FileHash
{
QString ComputeSha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QString();
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
    {
        return QString();
    }
    return QString::fromLatin1(hash.result().toHex());
}
}
