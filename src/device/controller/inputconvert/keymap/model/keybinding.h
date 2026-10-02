#ifndef KEYMAP_MODEL_KEYBINDING_H
#define KEYMAP_MODEL_KEYBINDING_H

#include <QJsonObject>
#include <QStringList>

class KeyBinding
{
public:
    QString code;
    QStringList modifiers;

    bool isValid(QString *error = nullptr) const;
    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &json, KeyBinding *binding, QString *error = nullptr);
    static bool fromLegacyCode(const QString &legacyCode, KeyBinding *binding, QString *error = nullptr);
};

#endif
