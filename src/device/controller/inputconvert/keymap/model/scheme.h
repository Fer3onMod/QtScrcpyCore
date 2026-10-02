#ifndef KEYMAP_MODEL_SCHEME_H
#define KEYMAP_MODEL_SCHEME_H

#include <QJsonObject>
#include <QString>
#include <QVector>

#include <memory>

#include "widget.h"

class Scheme
{
public:
    QString id;
    QString name;
    QString type = QStringLiteral("local");
    QString activationKey;
    QVector<std::shared_ptr<Widget> > widgets;

    QJsonObject toJson() const;
    bool isValid(QString *error = nullptr) const;
    static bool fromJson(const QJsonObject &json, Scheme *scheme, QString *error = nullptr);
};

#endif
