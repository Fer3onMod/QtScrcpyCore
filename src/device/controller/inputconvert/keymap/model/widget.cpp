#include "widget.h"

#include <QJsonArray>
#include <QJsonValue>
#include <cmath>

namespace {
bool numberInRange(const QJsonObject &json, const QString &name, double minimum,
                   double maximum, double *value, QString *error)
{
    const QJsonValue jsonValue = json.value(name);
    if (!jsonValue.isDouble() || !std::isfinite(jsonValue.toDouble())
        || jsonValue.toDouble() < minimum || jsonValue.toDouble() > maximum) {
        if (error) {
            *error = QStringLiteral("%1 must be a number in [%2, %3].")
                         .arg(name).arg(minimum).arg(maximum);
        }
        return false;
    }
    *value = jsonValue.toDouble();
    return true;
}

bool integerInRange(const QJsonObject &json, const QString &name, int minimum,
                    int maximum, int *value, QString *error)
{
    double number = 0.0;
    if (!numberInRange(json, name, minimum, maximum, &number, error)) return false;
    if (std::floor(number) != number) {
        if (error) *error = QStringLiteral("%1 must be an integer.").arg(name);
        return false;
    }
    *value = static_cast<int>(number);
    return true;
}

bool requiredString(const QJsonObject &json, const QString &name, QString *value,
                    QString *error)
{
    if (!json.value(name).isString() || json.value(name).toString().isEmpty()) {
        if (error) {
            *error = QStringLiteral("%1 must be a non-empty string.").arg(name);
        }
        return false;
    }
    if (value) *value = json.value(name).toString();
    return true;
}

bool readPoint(const QJsonObject &json, const QString &xName, const QString &yName,
               QPointF *point, QString *error)
{
    double x = 0.0;
    double y = 0.0;
    if (!numberInRange(json, xName, 0.0, 1.0, &x, error)
        || !numberInRange(json, yName, 0.0, 1.0, &y, error)) {
        return false;
    }
    *point = QPointF(x, y);
    return true;
}

bool readBindingFields(const QJsonObject &json, const QString &codeName,
                       const QString &modifiersName, KeyBinding *binding,
                       QString *error)
{
    if (!json.value(codeName).isString()) {
        if (error) {
            *error = QStringLiteral("%1 must be a string key code.").arg(codeName);
        }
        return false;
    }
    QJsonObject bindingJson;
    bindingJson.insert(QStringLiteral("code"), json.value(codeName));
    if (json.contains(modifiersName)) {
        bindingJson.insert(QStringLiteral("modifiers"), json.value(modifiersName));
    }
    return KeyBinding::fromJson(bindingJson, binding, error);
}

bool readJoystickBinding(const QJsonObject &json, const QString &name,
                         KeyBinding *binding, QString *error)
{
    const QJsonValue value = json.value(name);
    if (value.isString()) {
        binding->code = value.toString();
        return binding->isValid(error);
    }
    if (value.isObject()) {
        return KeyBinding::fromJson(value.toObject(), binding, error);
    }
    if (error) *error = QStringLiteral("Joystick direction %1 must be a key code or binding object.").arg(name);
    return false;
}

void writeBindingFields(QJsonObject *json, const QString &codeName,
                        const QString &modifiersName, const KeyBinding &binding)
{
    json->insert(codeName, binding.code);
    if (!binding.modifiers.isEmpty()) {
        QJsonArray modifiers;
        for (const QString &modifier : binding.modifiers) {
            modifiers.append(modifier);
        }
        json->insert(modifiersName, modifiers);
    }
}

bool readSensitivity(const QJsonObject &json, double *x, double *y, QString *error)
{
    return numberInRange(json, QStringLiteral("sensX"), 0.001, 10000.0, x, error)
        && numberInRange(json, QStringLiteral("sensY"), 0.001, 10000.0, y, error);
}

void writeSensitivity(QJsonObject *json, double x, double y)
{
    json->insert(QStringLiteral("sensX"), x);
    json->insert(QStringLiteral("sensY"), y);
}
}

QJsonObject Widget::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("id"), id);
    json.insert(QStringLiteral("type"), type());
    json.insert(QStringLiteral("x"), position.x());
    json.insert(QStringLiteral("y"), position.y());
    json.insert(QStringLiteral("size"), size);
    json.insert(QStringLiteral("z"), z);
    json.insert(QStringLiteral("enabled"), enabled);
    if (!label.isEmpty()) {
        json.insert(QStringLiteral("label"), label);
    }
    writeSpecific(&json);
    return json;
}

bool Widget::isValid(QString *error) const
{
    if (error) error->clear();
    if (id.isEmpty() || !std::isfinite(position.x()) || !std::isfinite(position.y())
        || position.x() < 0.0 || position.x() > 1.0
        || position.y() < 0.0 || position.y() > 1.0) {
        if (error) {
            *error = QStringLiteral("Widget id and normalized position are required.");
        }
        return false;
    }
    if (!std::isfinite(size) || size < 1.0 || size > 512.0
        || z < -10000 || z > 10000) {
        if (error) {
            *error = QStringLiteral("Widget size or z value is out of range.");
        }
        return false;
    }
    return true;
}

std::shared_ptr<Widget> Widget::fromJson(const QJsonObject &json, QString *error)
{
    if (error) error->clear();
    if (!json.value(QStringLiteral("id")).isString()
        || json.value(QStringLiteral("id")).toString().trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Widget id must be a non-empty string.");
        return std::shared_ptr<Widget>();
    }
    if (!json.value(QStringLiteral("type")).isString()) {
        if (error) {
            *error = QStringLiteral("Widget type must be a string.");
        }
        return std::shared_ptr<Widget>();
    }
    const QString widgetType = json.value(QStringLiteral("type")).toString();

    std::shared_ptr<Widget> widget;
    if (widgetType == QStringLiteral("key")) widget.reset(new KeyWidget);
    else if (widgetType == QStringLiteral("joystick")) widget.reset(new JoystickWidget);
    else if (widgetType == QStringLiteral("aim")) widget.reset(new AimWidget);
    else if (widgetType == QStringLiteral("fire")) widget.reset(new FireWidget);
    else if (widgetType == QStringLiteral("slide")) widget.reset(new SlideWidget);
    else if (widgetType == QStringLiteral("macro")) widget.reset(new MacroWidget);
    else if (widgetType == QStringLiteral("clickRepeat")) widget.reset(new ClickRepeatWidget);
    else if (widgetType == QStringLiteral("observed")) widget.reset(new ObservedWidget);
    else if (widgetType == QStringLiteral("viewAngle")) widget.reset(new ViewAngleWidget);
    else if (widgetType == QStringLiteral("rightMouseMove")) widget.reset(new RightMouseMoveWidget);
    else if (widgetType == QStringLiteral("intelligentCast")) widget.reset(new IntelligentCastWidget);
    else if (widgetType == QStringLiteral("cancelCast")) widget.reset(new CancelCastWidget);
    else if (widgetType == QStringLiteral("visionExtension")) widget.reset(new VisionExtensionWidget);
    else {
        if (error) {
            *error = QStringLiteral("Unsupported widget type: %1").arg(widgetType);
        }
        return std::shared_ptr<Widget>();
    }

    widget->id = json.value(QStringLiteral("id")).toString();
    if (!readPoint(json, QStringLiteral("x"), QStringLiteral("y"),
                   &widget->position, error)) {
        return std::shared_ptr<Widget>();
    }
    if (json.contains(QStringLiteral("size"))
        && !numberInRange(json, QStringLiteral("size"), 1.0, 512.0,
                          &widget->size, error)) {
        return std::shared_ptr<Widget>();
    }
    if (json.contains(QStringLiteral("z"))) {
        if (!integerInRange(json, QStringLiteral("z"), -10000, 10000,
                            &widget->z, error)) {
            return std::shared_ptr<Widget>();
        }
    }
    if (json.contains(QStringLiteral("enabled"))) {
        if (!json.value(QStringLiteral("enabled")).isBool()) {
            if (error) *error = QStringLiteral("Widget enabled must be a boolean.");
            return std::shared_ptr<Widget>();
        }
        widget->enabled = json.value(QStringLiteral("enabled")).toBool();
    }
    if (json.contains(QStringLiteral("label"))) {
        if (!json.value(QStringLiteral("label")).isString()) {
            if (error) *error = QStringLiteral("Widget label must be a string.");
            return std::shared_ptr<Widget>();
        }
        widget->label = json.value(QStringLiteral("label")).toString();
    }
    if (!widget->readSpecific(json, error) || !widget->isValid(error)) {
        return std::shared_ptr<Widget>();
    }
    return widget;
}

QJsonObject BoundWidget::toJson() const
{
    QJsonObject json = Widget::toJson();
    writeBindingFields(&json, QStringLiteral("code"), QStringLiteral("modifiers"), binding);
    return json;
}

bool BoundWidget::isValid(QString *error) const
{
    return Widget::isValid(error) && binding.isValid(error);
}

void BoundWidget::writeBinding(QJsonObject *json) const
{
    writeBindingFields(json, QStringLiteral("code"), QStringLiteral("modifiers"), binding);
}

bool BoundWidget::readBinding(const QJsonObject &json, QString *error)
{
    return readBindingFields(json, QStringLiteral("code"), QStringLiteral("modifiers"),
                             &binding, error);
}

void KeyWidget::writeSpecific(QJsonObject *json) const
{
    json->insert(QStringLiteral("tapCount"), tapCount);
    json->insert(QStringLiteral("releaseMouse"), releaseMouse);
    json->insert(QStringLiteral("smart"), smart);
    if (androidKey >= 0) json->insert(QStringLiteral("androidKey"), androidKey);
}

bool KeyWidget::readSpecific(const QJsonObject &json, QString *error)
{
    if (!readBinding(json, error)) return false;
    if (json.contains(QStringLiteral("tapCount"))
        && !integerInRange(json, QStringLiteral("tapCount"), 1, 100, &tapCount, error)) return false;
    if (json.contains(QStringLiteral("releaseMouse"))) {
        if (!json.value(QStringLiteral("releaseMouse")).isBool()) {
            if (error) *error = QStringLiteral("releaseMouse must be a boolean.");
            return false;
        }
        releaseMouse = json.value(QStringLiteral("releaseMouse")).toBool();
    }
    if (json.contains(QStringLiteral("androidKey"))
        && !integerInRange(json, QStringLiteral("androidKey"), 0, 1000, &androidKey, error)) return false;
    if (json.contains(QStringLiteral("smart"))) {
        if (!json.value(QStringLiteral("smart")).isBool()) {
            if (error) *error = QStringLiteral("smart must be a boolean.");
            return false;
        }
        smart = json.value(QStringLiteral("smart")).toBool();
    }
    return true;
}

bool KeyWidget::isValid(QString *error) const
{
    if (!BoundWidget::isValid(error)) return false;
    if (tapCount < 1 || tapCount > 100 || androidKey < -1 || androidKey > 1000) {
        if (error) *error = QStringLiteral("Key widget tapCount or androidKey is out of range.");
        return false;
    }
    return true;
}

void JoystickWidget::writeSpecific(QJsonObject *json) const
{
    QJsonObject keys;
    const KeyBinding *bindings[] = {&up, &left, &down, &right};
    const QString names[] = {QStringLiteral("up"), QStringLiteral("left"),
                             QStringLiteral("down"), QStringLiteral("right")};
    for (int i = 0; i < 4; ++i) {
        if (bindings[i]->modifiers.isEmpty())
            keys.insert(names[i], bindings[i]->code);
        else
            keys.insert(names[i], bindings[i]->toJson());
    }
    json->insert(QStringLiteral("keys"), keys);
    json->insert(QStringLiteral("radius"), radius);
    json->insert(QStringLiteral("speed"), speed);
    QJsonObject offsets;
    for (QMap<QString, double>::const_iterator it = directionOffsets.constBegin();
         it != directionOffsets.constEnd(); ++it) {
        offsets.insert(it.key(), it.value());
    }
    if (upOffset >= 0.0) offsets.insert(QStringLiteral("up"), upOffset);
    if (leftOffset >= 0.0) offsets.insert(QStringLiteral("left"), leftOffset);
    if (downOffset >= 0.0) offsets.insert(QStringLiteral("down"), downOffset);
    if (rightOffset >= 0.0) offsets.insert(QStringLiteral("right"), rightOffset);
    if (!offsets.isEmpty()) json->insert(QStringLiteral("directionOffsets"), offsets);
}

bool JoystickWidget::readSpecific(const QJsonObject &json, QString *error)
{
    if (!json.value(QStringLiteral("keys")).isObject()) {
        if (error) *error = QStringLiteral("Joystick keys must be an object.");
        return false;
    }
    const QJsonObject keys = json.value(QStringLiteral("keys")).toObject();
    if (!readJoystickBinding(keys, QStringLiteral("up"), &up, error)
        || !readJoystickBinding(keys, QStringLiteral("left"), &left, error)
        || !readJoystickBinding(keys, QStringLiteral("down"), &down, error)
        || !readJoystickBinding(keys, QStringLiteral("right"), &right, error)
        || !numberInRange(json, QStringLiteral("radius"), 1.0, 512.0, &radius, error)
        || !numberInRange(json, QStringLiteral("speed"), 0.1, 100.0, &speed, error)) {
        return false;
    }
    if (json.contains(QStringLiteral("directionOffsets"))) {
        if (!json.value(QStringLiteral("directionOffsets")).isObject()) {
            if (error) *error = QStringLiteral("directionOffsets must be an object.");
            return false;
        }
        const QJsonObject offsets = json.value(QStringLiteral("directionOffsets")).toObject();
        if (offsets.contains(QStringLiteral("up"))
            && !numberInRange(offsets, QStringLiteral("up"), 0.0, 1.0, &upOffset, error)) return false;
        if (offsets.contains(QStringLiteral("left"))
            && !numberInRange(offsets, QStringLiteral("left"), 0.0, 1.0, &leftOffset, error)) return false;
        if (offsets.contains(QStringLiteral("down"))
            && !numberInRange(offsets, QStringLiteral("down"), 0.0, 1.0, &downOffset, error)) return false;
        if (offsets.contains(QStringLiteral("right"))
            && !numberInRange(offsets, QStringLiteral("right"), 0.0, 1.0, &rightOffset, error)) return false;
        const QStringList knownDirections = {QStringLiteral("up"), QStringLiteral("left"),
                                             QStringLiteral("down"), QStringLiteral("right")};
        for (const QString &direction : offsets.keys()) {
            if (!knownDirections.contains(direction)) {
                if (error) *error = QStringLiteral("Unsupported joystick direction: %1").arg(direction);
                return false;
            }
            directionOffsets.insert(direction, offsets.value(direction).toDouble());
        }
    }
    return true;
}

bool JoystickWidget::isValid(QString *error) const
{
    if (!Widget::isValid(error) || !up.isValid(error) || !left.isValid(error)
        || !down.isValid(error) || !right.isValid(error)) {
        return false;
    }
    if (!std::isfinite(radius) || radius < 1.0 || radius > 512.0
        || !std::isfinite(speed) || speed < 0.1 || speed > 100.0) {
        if (error) *error = QStringLiteral("Joystick radius or speed is out of range.");
        return false;
    }
    const QStringList directions = {QStringLiteral("up"), QStringLiteral("left"),
                                    QStringLiteral("down"), QStringLiteral("right")};
    for (const QString &direction : directions) {
        if (directionOffsets.contains(direction)) {
            const double offset = directionOffsets.value(direction);
            if (!std::isfinite(offset) || offset < 0.0 || offset > 1.0) {
                if (error) *error = QStringLiteral("Joystick direction offset is outside [0, 1].");
                return false;
            }
        }
    }
    return true;
}

void AimWidget::writeSpecific(QJsonObject *json) const
{
    writeBindingFields(json, QStringLiteral("toggleKey"), QStringLiteral("toggleModifiers"), toggleKey);
    writeSensitivity(json, sensX, sensY);
}

bool AimWidget::readSpecific(const QJsonObject &json, QString *error)
{
    return readBindingFields(json, QStringLiteral("toggleKey"), QStringLiteral("toggleModifiers"),
                             &toggleKey, error)
        && numberInRange(json, QStringLiteral("sensX"), 0.0001, 10000.0, &sensX, error)
        && numberInRange(json, QStringLiteral("sensY"), 0.0001, 10000.0, &sensY, error);
}

bool AimWidget::isValid(QString *error) const
{
    if (!Widget::isValid(error) || !toggleKey.isValid(error)) return false;
    if (!std::isfinite(sensX) || sensX < 0.0001 || sensX > 10000.0
        || !std::isfinite(sensY) || sensY < 0.0001 || sensY > 10000.0) {
        if (error) *error = QStringLiteral("Aim sensitivity must be within [0.0001, 10000].");
        return false;
    }
    return true;
}

void FireWidget::writeSpecific(QJsonObject *json) const
{
    json->insert(QStringLiteral("radius"), radius);
    json->insert(QStringLiteral("tapCount"), tapCount);
    json->insert(QStringLiteral("releaseMouse"), releaseMouse);
    if (androidKey >= 0) json->insert(QStringLiteral("androidKey"), androidKey);
}

bool FireWidget::readSpecific(const QJsonObject &json, QString *error)
{
    if (!readBinding(json, error)
        || !numberInRange(json, QStringLiteral("radius"), 1.0, 512.0, &radius, error)) return false;
    if (json.contains(QStringLiteral("tapCount"))
        && !integerInRange(json, QStringLiteral("tapCount"), 1, 100, &tapCount, error)) return false;
    if (json.contains(QStringLiteral("releaseMouse"))) {
        if (!json.value(QStringLiteral("releaseMouse")).isBool()) {
            if (error) *error = QStringLiteral("releaseMouse must be a boolean.");
            return false;
        }
        releaseMouse = json.value(QStringLiteral("releaseMouse")).toBool();
    }
    if (json.contains(QStringLiteral("androidKey"))
        && !integerInRange(json, QStringLiteral("androidKey"), 0, 1000, &androidKey, error)) return false;
    return true;
}

bool FireWidget::isValid(QString *error) const
{
    if (!BoundWidget::isValid(error)) return false;
    if (!std::isfinite(radius) || radius < 1.0 || radius > 512.0
        || tapCount < 1 || tapCount > 100 || androidKey < -1 || androidKey > 1000) {
        if (error) *error = QStringLiteral("Fire widget radius, tapCount, or androidKey is out of range.");
        return false;
    }
    return true;
}

void SlideWidget::writeSpecific(QJsonObject *json) const
{
    json->insert(QStringLiteral("endX"), endPosition.x());
    json->insert(QStringLiteral("endY"), endPosition.y());
    json->insert(QStringLiteral("startDelayMs"), startDelayMs);
    json->insert(QStringLiteral("speed"), speed);
}

bool SlideWidget::readSpecific(const QJsonObject &json, QString *error)
{
    double x = 0.0;
    double y = 0.0;
    if (!readBinding(json, error)
        || !numberInRange(json, QStringLiteral("endX"), 0.0, 1.0, &x, error)
        || !numberInRange(json, QStringLiteral("endY"), 0.0, 1.0, &y, error)) return false;
    endPosition = QPointF(x, y);
    if (json.contains(QStringLiteral("startDelayMs"))
        && !integerInRange(json, QStringLiteral("startDelayMs"), 0, 60000, &startDelayMs, error)) return false;
    if (json.contains(QStringLiteral("speed"))
        && !numberInRange(json, QStringLiteral("speed"), 0.0, 1.0, &speed, error)) return false;
    return true;
}

bool SlideWidget::isValid(QString *error) const
{
    if (!BoundWidget::isValid(error)) return false;
    if (!std::isfinite(endPosition.x()) || endPosition.x() < 0.0 || endPosition.x() > 1.0
        || !std::isfinite(endPosition.y()) || endPosition.y() < 0.0 || endPosition.y() > 1.0
        || startDelayMs < 0 || startDelayMs > 60000
        || !std::isfinite(speed) || speed < 0.0 || speed > 1.0) {
        if (error) *error = QStringLiteral("Slide endpoint, delay, or speed is out of range.");
        return false;
    }
    return true;
}

QJsonObject MacroStep::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("action"), action);
    json.insert(QStringLiteral("delayMs"), delayMs);
    if (action == QStringLiteral("tap")) {
        json.insert(QStringLiteral("x"), position.x());
        json.insert(QStringLiteral("y"), position.y());
    } else {
        json.insert(QStringLiteral("code"), binding.code);
        if (!binding.modifiers.isEmpty()) {
            QJsonArray modifiers;
            for (const QString &modifier : binding.modifiers) modifiers.append(modifier);
            json.insert(QStringLiteral("modifiers"), modifiers);
        }
    }
    return json;
}

bool MacroStep::fromJson(const QJsonObject &json, MacroStep *step, QString *error)
{
    if (!step) {
        if (error) *error = QStringLiteral("Macro step output is null.");
        return false;
    }
    QString actionValue;
    int delay = 0;
    if (!requiredString(json, QStringLiteral("action"), &actionValue, error)
        || !integerInRange(json, QStringLiteral("delayMs"), 0, 60000, &delay, error)) return false;
    MacroStep parsed;
    parsed.action = actionValue;
    parsed.delayMs = delay;
    if (parsed.action == QStringLiteral("tap")) {
        if (!readPoint(json, QStringLiteral("x"), QStringLiteral("y"), &parsed.position, error)) return false;
        parsed.hasPosition = true;
    } else if (parsed.action == QStringLiteral("keyDown") || parsed.action == QStringLiteral("keyUp")) {
        if (!readBindingFields(json, QStringLiteral("code"), QStringLiteral("modifiers"),
                               &parsed.binding, error)) return false;
    } else {
        if (error) *error = QStringLiteral("Unsupported macro action: %1").arg(parsed.action);
        return false;
    }
    *step = parsed;
    return true;
}

void MacroWidget::writeSpecific(QJsonObject *json) const
{
    writeBinding(json);
    QJsonArray array;
    for (const MacroStep &step : steps) array.append(step.toJson());
    json->insert(QStringLiteral("steps"), array);
}

bool MacroWidget::readSpecific(const QJsonObject &json, QString *error)
{
    if (!readBinding(json, error) || !json.value(QStringLiteral("steps")).isArray()) {
        if (error && error->isEmpty()) *error = QStringLiteral("Macro steps must be an array.");
        return false;
    }
    const QJsonArray array = json.value(QStringLiteral("steps")).toArray();
    if (array.isEmpty() || array.size() > 100) {
        if (error) *error = QStringLiteral("Macro must contain between 1 and 100 steps.");
        return false;
    }
    for (const QJsonValue &value : array) {
        MacroStep step;
        if (!value.isObject() || !MacroStep::fromJson(value.toObject(), &step, error)) return false;
        steps.append(step);
    }
    return true;
}

bool MacroWidget::isValid(QString *error) const
{
    if (!BoundWidget::isValid(error) || steps.isEmpty() || steps.size() > 100) {
        if (error && error->isEmpty()) *error = QStringLiteral("Macro must contain between 1 and 100 steps.");
        return false;
    }
    for (const MacroStep &step : steps) {
        if (step.delayMs < 0 || step.delayMs > 60000
            || (step.action != QStringLiteral("tap")
                && step.action != QStringLiteral("keyDown")
                && step.action != QStringLiteral("keyUp"))
            || (step.action == QStringLiteral("tap")
                && (!step.hasPosition || step.position.x() < 0.0 || step.position.x() > 1.0
                    || step.position.y() < 0.0 || step.position.y() > 1.0))
            || ((step.action == QStringLiteral("keyDown") || step.action == QStringLiteral("keyUp"))
                && !step.binding.isValid(error))) {
            if (error && error->isEmpty()) *error = QStringLiteral("Invalid macro step.");
            return false;
        }
    }
    return true;
}

void ClickRepeatWidget::writeSpecific(QJsonObject *json) const
{
    json->insert(QStringLiteral("intervalMs"), intervalMs);
}

bool ClickRepeatWidget::readSpecific(const QJsonObject &json, QString *error)
{
    return readBinding(json, error)
        && integerInRange(json, QStringLiteral("intervalMs"), 10, 60000, &intervalMs, error);
}

bool ClickRepeatWidget::isValid(QString *error) const
{
    if (!BoundWidget::isValid(error)) return false;
    if (intervalMs < 10 || intervalMs > 60000) {
        if (error) *error = QStringLiteral("Click repeat interval must be between 10 and 60000 ms.");
        return false;
    }
    return true;
}

void ViewAngleWidget::writeSpecific(QJsonObject *json) const
{
    writeSensitivity(json, sensX, sensY);
}

bool ViewAngleWidget::readSpecific(const QJsonObject &json, QString *error)
{
    return readBinding(json, error) && readSensitivity(json, &sensX, &sensY, error);
}

bool ViewAngleWidget::isValid(QString *error) const
{
    if (!BoundWidget::isValid(error)) return false;
    if (!std::isfinite(sensX) || sensX < 0.001 || sensX > 10000.0
        || !std::isfinite(sensY) || sensY < 0.001 || sensY > 10000.0) {
        if (error) *error = QStringLiteral("View-angle sensitivity must be within [0.001, 10000].");
        return false;
    }
    return true;
}

void RightMouseMoveWidget::writeSpecific(QJsonObject *json) const
{
    writeSensitivity(json, sensX, sensY);
}

bool RightMouseMoveWidget::readSpecific(const QJsonObject &json, QString *error)
{
    return readBinding(json, error) && readSensitivity(json, &sensX, &sensY, error);
}

bool RightMouseMoveWidget::isValid(QString *error) const
{
    if (!BoundWidget::isValid(error)) return false;
    if (!std::isfinite(sensX) || sensX < 0.001 || sensX > 10000.0
        || !std::isfinite(sensY) || sensY < 0.001 || sensY > 10000.0) {
        if (error) *error = QStringLiteral("Mouse-move sensitivity must be within [0.001, 10000].");
        return false;
    }
    return true;
}
