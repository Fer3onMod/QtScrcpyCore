#include "keybinding.h"

#include <QJsonArray>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

namespace {
const QRegularExpression &codePattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^(Key[A-Z]|Digit[0-9]|F([1-9]|1[0-2])|"
                       "Arrow(Up|Down|Left|Right)|Backquote|Minus|Equal|"
                       "BracketLeft|BracketRight|Backslash|Semicolon|Quote|"
                       "Comma|Period|Slash|Space|Escape|Tab|Enter|Backspace|"
                       "Delete|Insert|Home|End|PageUp|PageDown|"
                       "Shift(Left|Right)|Control(Left|Right)|Alt(Left|Right)|"
                       "Meta(Left|Right)|Numpad([0-9]|Add|Subtract|Multiply|"
                       "Divide|Decimal|Enter)|Mouse(Left|Right|Middle|X1|X2))$"));
    return pattern;
}

QString legacyQtKey(QString value)
{
    if (value.startsWith(QStringLiteral("Key_"))) {
        value.remove(0, 4);
        if (value.size() == 1 && value.at(0).isLetter()) {
            return QStringLiteral("Key") + value.toUpper();
        }
        if (value.size() == 1 && value.at(0).isDigit()) {
            return QStringLiteral("Digit") + value;
        }
        if (value.startsWith(QStringLiteral("F")) && value.mid(1).toInt() >= 1
            && value.mid(1).toInt() <= 12) {
            return value;
        }
        static const QMap<QString, QString> names = {
            {QStringLiteral("QuoteLeft"), QStringLiteral("Backquote")},
            {QStringLiteral("Equal"), QStringLiteral("Equal")},
            {QStringLiteral("Minus"), QStringLiteral("Minus")},
            {QStringLiteral("BracketLeft"), QStringLiteral("BracketLeft")},
            {QStringLiteral("BracketRight"), QStringLiteral("BracketRight")},
            {QStringLiteral("Backslash"), QStringLiteral("Backslash")},
            {QStringLiteral("Semicolon"), QStringLiteral("Semicolon")},
            {QStringLiteral("QuoteDbl"), QStringLiteral("Quote")},
            {QStringLiteral("Comma"), QStringLiteral("Comma")},
            {QStringLiteral("Period"), QStringLiteral("Period")},
            {QStringLiteral("Slash"), QStringLiteral("Slash")},
            {QStringLiteral("Space"), QStringLiteral("Space")},
            {QStringLiteral("Escape"), QStringLiteral("Escape")},
            {QStringLiteral("Tab"), QStringLiteral("Tab")},
            {QStringLiteral("Return"), QStringLiteral("Enter")},
            {QStringLiteral("Enter"), QStringLiteral("Enter")},
            {QStringLiteral("Backspace"), QStringLiteral("Backspace")},
            {QStringLiteral("Delete"), QStringLiteral("Delete")},
            {QStringLiteral("Insert"), QStringLiteral("Insert")},
            {QStringLiteral("Home"), QStringLiteral("Home")},
            {QStringLiteral("End"), QStringLiteral("End")},
            {QStringLiteral("PageUp"), QStringLiteral("PageUp")},
            {QStringLiteral("PageDown"), QStringLiteral("PageDown")},
            {QStringLiteral("Up"), QStringLiteral("ArrowUp")},
            {QStringLiteral("Down"), QStringLiteral("ArrowDown")},
            {QStringLiteral("Left"), QStringLiteral("ArrowLeft")},
            {QStringLiteral("Right"), QStringLiteral("ArrowRight")},
            {QStringLiteral("Shift"), QStringLiteral("ShiftLeft")},
            {QStringLiteral("Control"), QStringLiteral("ControlLeft")},
            {QStringLiteral("Alt"), QStringLiteral("AltLeft")},
            {QStringLiteral("Meta"), QStringLiteral("MetaLeft")},
        };
        return names.value(value);
    }

    static const QMap<QString, QString> mouseNames = {
        {QStringLiteral("LeftButton"), QStringLiteral("MouseLeft")},
        {QStringLiteral("RightButton"), QStringLiteral("MouseRight")},
        {QStringLiteral("MiddleButton"), QStringLiteral("MouseMiddle")},
        {QStringLiteral("XButton1"), QStringLiteral("MouseX1")},
        {QStringLiteral("XButton2"), QStringLiteral("MouseX2")},
    };
    return mouseNames.value(value);
}
}

bool KeyBinding::isValid(QString *error) const
{
    if (error) error->clear();
    if (!codePattern().match(code).hasMatch()) {
        if (error) {
            *error = QStringLiteral("Unsupported key code: %1").arg(code);
        }
        return false;
    }

    static const QStringList allowedModifiers = {
        QStringLiteral("Shift"), QStringLiteral("Control"),
        QStringLiteral("Alt"), QStringLiteral("Meta")
    };
    QSet<QString> seen;
    for (const QString &modifier : modifiers) {
        if (!allowedModifiers.contains(modifier) || seen.contains(modifier)) {
            if (error) {
                *error = QStringLiteral("Invalid or repeated modifier: %1").arg(modifier);
            }
            return false;
        }
        seen.insert(modifier);
    }
    return true;
}

QJsonObject KeyBinding::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("code"), code);
    if (!modifiers.isEmpty()) {
        QJsonArray values;
        for (const QString &modifier : modifiers) {
            values.append(modifier);
        }
        json.insert(QStringLiteral("modifiers"), values);
    }
    return json;
}

bool KeyBinding::fromJson(const QJsonObject &json, KeyBinding *binding, QString *error)
{
    if (error) error->clear();
    if (!binding || !json.value(QStringLiteral("code")).isString()) {
        if (error) {
            *error = QStringLiteral("Key binding requires a string code.");
        }
        return false;
    }

    KeyBinding parsed;
    parsed.code = json.value(QStringLiteral("code")).toString();
    const QJsonValue modifiersValue = json.value(QStringLiteral("modifiers"));
    if (!modifiersValue.isUndefined()) {
        if (!modifiersValue.isArray()) {
            if (error) {
                *error = QStringLiteral("Key binding modifiers must be an array.");
            }
            return false;
        }
        const QJsonArray modifiersArray = modifiersValue.toArray();
        for (const QJsonValue &value : modifiersArray) {
            if (!value.isString()) {
                if (error) {
                    *error = QStringLiteral("Each key modifier must be a string.");
                }
                return false;
            }
            parsed.modifiers.append(value.toString());
        }
    }
    if (!parsed.isValid(error)) {
        return false;
    }
    *binding = parsed;
    return true;
}

bool KeyBinding::fromLegacyCode(const QString &legacyCode, KeyBinding *binding, QString *error)
{
    if (error) error->clear();
    if (!binding) {
        if (error) {
            *error = QStringLiteral("Key binding output is null.");
        }
        return false;
    }

    KeyBinding parsed;
    parsed.code = legacyQtKey(legacyCode);
    if (!parsed.isValid(error)) {
        if (error) *error = QStringLiteral("Unsupported legacy key code: %1").arg(legacyCode);
        return false;
    }
    *binding = parsed;
    return true;
}
