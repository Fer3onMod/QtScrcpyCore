#include "scheme.h"

#include <QJsonArray>
#include <QSet>

QJsonObject Scheme::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("id"), id);
    json.insert(QStringLiteral("name"), name);
    json.insert(QStringLiteral("type"), type);
    if (!activationKey.isEmpty()) {
        json.insert(QStringLiteral("activationKey"), activationKey);
    }
    QJsonArray widgetArray;
    for (const std::shared_ptr<Widget> &widget : widgets) {
        if (widget) widgetArray.append(widget->toJson());
    }
    json.insert(QStringLiteral("widgets"), widgetArray);
    return json;
}

bool Scheme::isValid(QString *error) const
{
    if (error) error->clear();
    if (id.trimmed().isEmpty() || name.trimmed().isEmpty()
        || (type != QStringLiteral("official")
            && type != QStringLiteral("local")
            && type != QStringLiteral("cloud"))) {
        if (error) *error = QStringLiteral("Scheme requires an id, name, and valid type.");
        return false;
    }
    if (!activationKey.isEmpty()) {
        KeyBinding binding;
        binding.code = activationKey;
        if (!binding.isValid(error)) return false;
    }

    QSet<QString> ids;
    for (const std::shared_ptr<Widget> &widget : widgets) {
        if (!widget || !widget->isValid(error)) return false;
        if (ids.contains(widget->id)) {
            if (error) *error = QStringLiteral("Duplicate widget id: %1").arg(widget->id);
            return false;
        }
        ids.insert(widget->id);
    }
    return true;
}

bool Scheme::fromJson(const QJsonObject &json, Scheme *scheme, QString *error)
{
    if (error) error->clear();
    if (!scheme || !json.value(QStringLiteral("id")).isString()
        || !json.value(QStringLiteral("name")).isString()
        || !json.value(QStringLiteral("type")).isString()
        || !json.value(QStringLiteral("widgets")).isArray()) {
        if (error) *error = QStringLiteral("Scheme requires string id/name/type and a widgets array.");
        return false;
    }

    Scheme parsed;
    parsed.id = json.value(QStringLiteral("id")).toString();
    parsed.name = json.value(QStringLiteral("name")).toString();
    parsed.type = json.value(QStringLiteral("type")).toString();
    if (json.contains(QStringLiteral("activationKey"))) {
        if (!json.value(QStringLiteral("activationKey")).isString()) {
            if (error) *error = QStringLiteral("Scheme activationKey must be a string.");
            return false;
        }
        parsed.activationKey = json.value(QStringLiteral("activationKey")).toString();
    }

    const QJsonArray widgetsArray = json.value(QStringLiteral("widgets")).toArray();
    for (const QJsonValue &value : widgetsArray) {
        if (!value.isObject()) {
            if (error) *error = QStringLiteral("Each scheme widget must be an object.");
            return false;
        }
        std::shared_ptr<Widget> widget = Widget::fromJson(value.toObject(), error);
        if (!widget) return false;
        parsed.widgets.append(widget);
    }
    if (!parsed.isValid(error)) return false;
    *scheme = parsed;
    return true;
}
