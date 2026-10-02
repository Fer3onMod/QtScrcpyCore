#ifndef KEYMAP_MODEL_SCHEMESTORE_H
#define KEYMAP_MODEL_SCHEMESTORE_H

#include <QByteArray>
#include <QString>
#include <QVector>

#include "scheme.h"

class SchemeStore
{
public:
    static const int CurrentVersion = 2;

    QVector<Scheme> schemes;

    QByteArray toJson(QString *error = nullptr) const;
    bool loadJson(const QByteArray &json, QString *error = nullptr);
    bool loadFile(const QString &path, QString *error = nullptr);
    bool saveFile(const QString &path, QString *error = nullptr) const;

    static bool migrateLegacyJson(const QByteArray &json, const QString &schemeName,
                                  SchemeStore *store, QString *error = nullptr);
    static bool migrateLegacyFileInPlace(const QString &path,
                                         QString *backupPath = nullptr,
                                         QString *error = nullptr);
};

#endif
