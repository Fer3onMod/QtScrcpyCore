#include "schemestore.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <cmath>

namespace {
void setError(QString *error, const QString &message)
{
    if (error) *error = message;
}

bool readLegacyPoint(const QJsonObject &object, const QString &name,
                     QPointF *point, QString *error)
{
    const QJsonValue value = object.value(name);
    if (!value.isObject()) {
        setError(error, QStringLiteral("Legacy field %1 must be an object.").arg(name));
        return false;
    }
    const QJsonObject coordinates = value.toObject();
    const QJsonValue x = coordinates.value(QStringLiteral("x"));
    const QJsonValue y = coordinates.value(QStringLiteral("y"));
    if (!x.isDouble() || !y.isDouble() || !std::isfinite(x.toDouble())
        || !std::isfinite(y.toDouble()) || x.toDouble() < 0.0 || x.toDouble() > 1.0
        || y.toDouble() < 0.0 || y.toDouble() > 1.0) {
        setError(error, QStringLiteral("Legacy field %1 must contain normalized x/y coordinates.").arg(name));
        return false;
    }
    *point = QPointF(x.toDouble(), y.toDouble());
    return true;
}

bool legacyBinding(const QJsonObject &object, const QString &field,
                   KeyBinding *binding, QString *error)
{
    if (!object.value(field).isString()) {
        setError(error, QStringLiteral("Legacy field %1 must be a key name.").arg(field));
        return false;
    }
    return KeyBinding::fromLegacyCode(object.value(field).toString(), binding, error);
}

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

std::shared_ptr<Widget> makeBoundWidget(const QString &code, const QPointF &position,
                                        const QString &label, QString *error)
{
    KeyBinding binding;
    if (!KeyBinding::fromLegacyCode(code, &binding, error)) return std::shared_ptr<Widget>();
    if (code.startsWith(QStringLiteral("LeftButton"))
        || code.startsWith(QStringLiteral("RightButton"))
        || code.startsWith(QStringLiteral("MiddleButton"))
        || code.startsWith(QStringLiteral("XButton"))) {
        std::shared_ptr<FireWidget> widget(new FireWidget);
        widget->binding = binding;
        widget->position = position;
        widget->label = label;
        widget->id = newId();
        return widget;
    }
    std::shared_ptr<KeyWidget> widget(new KeyWidget);
    widget->binding = binding;
    widget->position = position;
    widget->label = label;
    widget->id = newId();
    return widget;
}

bool writeAtomically(const QString &path, const QByteArray &contents, QString *error)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Could not open %1 for atomic writing: %2")
                            .arg(path, file.errorString()));
        return false;
    }
    if (file.write(contents) != contents.size()) {
        setError(error, QStringLiteral("Could not write %1: %2").arg(path, file.errorString()));
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        setError(error, QStringLiteral("Could not commit %1: %2").arg(path, file.errorString()));
        return false;
    }
    return true;
}

bool readLegacyNode(const QJsonObject &node, Scheme *scheme, QString *error)
{
    if (!node.value(QStringLiteral("type")).isString()) {
        setError(error, QStringLiteral("Legacy keyMapNodes entries require a string type field."));
        return false;
    }
    const QString type = node.value(QStringLiteral("type")).toString();
    if (type != QStringLiteral("KMT_STEER_WHEEL")
        && !node.value(QStringLiteral("key")).isString()) {
        setError(error, QStringLiteral("Legacy %1 entries require a string key field.").arg(type));
        return false;
    }
    const QString key = node.value(QStringLiteral("key")).toString();
    const QString label = node.value(QStringLiteral("comment")).toString();

    if (type == QStringLiteral("KMT_CLICK") || type == QStringLiteral("KMT_CLICK_TWICE")) {
        QPointF position;
        if (!readLegacyPoint(node, QStringLiteral("pos"), &position, error)) return false;
        std::shared_ptr<Widget> base = makeBoundWidget(key, position, label, error);
        if (!base) return false;
        std::shared_ptr<KeyWidget> keyWidget = std::dynamic_pointer_cast<KeyWidget>(base);
        std::shared_ptr<FireWidget> fireWidget = std::dynamic_pointer_cast<FireWidget>(base);
        const QJsonValue release = node.value(QStringLiteral("switchMap"));
        if (!release.isUndefined() && !release.isBool()) {
            setError(error, QStringLiteral("Legacy switchMap must be a boolean."));
            return false;
        }
        if (keyWidget) {
            keyWidget->tapCount = type == QStringLiteral("KMT_CLICK_TWICE") ? 2 : 1;
            keyWidget->releaseMouse = release.toBool(false);
            const QJsonValue androidKey = node.value(QStringLiteral("androidKey"));
            if (!androidKey.isUndefined()) {
                if (!androidKey.isDouble() || std::floor(androidKey.toDouble()) != androidKey.toDouble()
                    || androidKey.toDouble() < 0.0 || androidKey.toDouble() > 1000.0) {
                    setError(error, QStringLiteral("Legacy androidKey is outside the supported range."));
                    return false;
                }
                keyWidget->androidKey = static_cast<int>(androidKey.toDouble());
            }
        } else if (fireWidget) {
            fireWidget->tapCount = type == QStringLiteral("KMT_CLICK_TWICE") ? 2 : 1;
            fireWidget->releaseMouse = release.toBool(false);
            const QJsonValue androidKey = node.value(QStringLiteral("androidKey"));
            if (!androidKey.isUndefined()) {
                if (!androidKey.isDouble() || std::floor(androidKey.toDouble()) != androidKey.toDouble()
                    || androidKey.toDouble() < 0.0 || androidKey.toDouble() > 1000.0) {
                    setError(error, QStringLiteral("Legacy androidKey is outside the supported range."));
                    return false;
                }
                fireWidget->androidKey = static_cast<int>(androidKey.toDouble());
            }
        }
        scheme->widgets.append(base);
        return true;
    }

    if (type == QStringLiteral("KMT_CLICK_MULTI")) {
        KeyBinding binding;
        if (!KeyBinding::fromLegacyCode(key, &binding, error)) return false;
        const QJsonValue clickNodesValue = node.value(QStringLiteral("clickNodes"));
        if (!clickNodesValue.isArray() || clickNodesValue.toArray().isEmpty()
            || clickNodesValue.toArray().size() > 100) {
            setError(error, QStringLiteral("Legacy clickNodes must contain between 1 and 100 entries."));
            return false;
        }
        std::shared_ptr<MacroWidget> macro(new MacroWidget);
        macro->id = newId();
        macro->binding = binding;
        macro->label = label;
        const QJsonArray clickNodes = clickNodesValue.toArray();
        for (const QJsonValue &value : clickNodes) {
            if (!value.isObject()) {
                setError(error, QStringLiteral("Each legacy clickNodes entry must be an object."));
                return false;
            }
            const QJsonObject click = value.toObject();
            QPointF position;
            const QJsonValue delay = click.value(QStringLiteral("delay"));
            if (!readLegacyPoint(click, QStringLiteral("pos"), &position, error)
                || !delay.isDouble() || !std::isfinite(delay.toDouble())
                || std::floor(delay.toDouble()) != delay.toDouble()
                || delay.toDouble() < 0.0 || delay.toDouble() > 60000.0) {
                if (error && error->isEmpty())
                    setError(error, QStringLiteral("Legacy click delay must be an integer from 0 to 60000."));
                return false;
            }
            MacroStep step;
            step.action = QStringLiteral("tap");
            step.position = position;
            step.hasPosition = true;
            step.delayMs = static_cast<int>(delay.toDouble());
            macro->steps.append(step);
        }
        scheme->widgets.append(macro);
        return true;
    }

    if (type == QStringLiteral("KMT_STEER_WHEEL")) {
        QPointF center;
        if (!readLegacyPoint(node, QStringLiteral("centerPos"), &center, error)) return false;
        std::shared_ptr<JoystickWidget> joystick(new JoystickWidget);
        joystick->id = newId();
        joystick->position = center;
        joystick->label = label;
        if (!legacyBinding(node, QStringLiteral("upKey"), &joystick->up, error)
            || !legacyBinding(node, QStringLiteral("leftKey"), &joystick->left, error)
            || !legacyBinding(node, QStringLiteral("downKey"), &joystick->down, error)
            || !legacyBinding(node, QStringLiteral("rightKey"), &joystick->right, error)) return false;
        const QStringList names = {QStringLiteral("up"), QStringLiteral("left"),
                                   QStringLiteral("down"), QStringLiteral("right")};
        for (const QString &name : names) {
            const QString field = name + QStringLiteral("Offset");
            const QJsonValue value = node.value(field);
            if (!value.isDouble() || !std::isfinite(value.toDouble())
                || value.toDouble() < 0.0 || value.toDouble() > 1.0) {
                setError(error, QStringLiteral("Legacy joystick %1 must be between 0 and 1.").arg(field));
                return false;
            }
            joystick->directionOffsets.insert(name, value.toDouble());
        }
        joystick->radius = 60.0;
        joystick->speed = 8.0;
        scheme->widgets.append(joystick);
        return true;
    }

    if (type == QStringLiteral("KMT_DRAG")) {
        QPointF start;
        QPointF end;
        if (!readLegacyPoint(node, QStringLiteral("startPos"), &start, error)
            || !readLegacyPoint(node, QStringLiteral("endPos"), &end, error)) return false;
        std::shared_ptr<SlideWidget> slide(new SlideWidget);
        slide->id = newId();
        slide->position = start;
        slide->endPosition = end;
        slide->label = label;
        if (!KeyBinding::fromLegacyCode(key, &slide->binding, error)) return false;
        const QJsonValue delay = node.value(QStringLiteral("startDelay"));
        if (!delay.isUndefined()) {
            if (!delay.isDouble() || std::floor(delay.toDouble()) != delay.toDouble()
                || delay.toDouble() < 0.0 || delay.toDouble() > 60000.0) {
                setError(error, QStringLiteral("Legacy drag startDelay must be an integer from 0 to 60000."));
                return false;
            }
            slide->startDelayMs = static_cast<int>(delay.toDouble());
        }
        const QJsonValue speed = node.value(QStringLiteral("dragSpeed"));
        if (!speed.isUndefined()) {
            if (!speed.isDouble() || !std::isfinite(speed.toDouble())
                || speed.toDouble() < 0.0 || speed.toDouble() > 1.0) {
                setError(error, QStringLiteral("Legacy dragSpeed must be between 0 and 1."));
                return false;
            }
            slide->speed = speed.toDouble();
        }
        scheme->widgets.append(slide);
        return true;
    }

    if (type == QStringLiteral("KMT_ANDROID_KEY")) {
        QPointF origin(0.5, 0.5);
        std::shared_ptr<KeyWidget> widget(new KeyWidget);
        widget->id = newId();
        widget->position = origin;
        widget->label = label;
        if (!KeyBinding::fromLegacyCode(key, &widget->binding, error)) return false;
        const QJsonValue androidKey = node.value(QStringLiteral("androidKey"));
        if (!androidKey.isDouble() || std::floor(androidKey.toDouble()) != androidKey.toDouble()
            || androidKey.toDouble() < 0.0 || androidKey.toDouble() > 1000.0) {
            setError(error, QStringLiteral("Legacy Android-key entry requires a valid androidKey."));
            return false;
        }
        widget->androidKey = static_cast<int>(androidKey.toDouble());
        scheme->widgets.append(widget);
        return true;
    }

    setError(error, QStringLiteral("Unsupported legacy widget type: %1").arg(type));
    return false;
}
}

QByteArray SchemeStore::toJson(QString *error) const
{
    if (error) error->clear();
    QSet<QString> ids;
    QJsonArray schemeArray;
    for (const Scheme &scheme : schemes) {
        if (!scheme.isValid(error)) return QByteArray();
        if (ids.contains(scheme.id)) {
            setError(error, QStringLiteral("Duplicate scheme id: %1").arg(scheme.id));
            return QByteArray();
        }
        ids.insert(scheme.id);
        schemeArray.append(scheme.toJson());
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), CurrentVersion);
    root.insert(QStringLiteral("schemes"), schemeArray);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool SchemeStore::loadJson(const QByteArray &json, QString *error)
{
    if (error) error->clear();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Invalid keymap JSON: %1").arg(parseError.errorString()));
        return false;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("Keymap JSON root must be an object."));
        return false;
    }
    const QJsonObject root = document.object();
    const QJsonValue versionValue = root.value(QStringLiteral("version"));
    if (!versionValue.isDouble() || !std::isfinite(versionValue.toDouble())
        || versionValue.toDouble() != CurrentVersion
        || !root.value(QStringLiteral("schemes")).isArray()) {
        setError(error, QStringLiteral("Keymap document must have version 2 and a schemes array."));
        return false;
    }

    SchemeStore parsed;
    QSet<QString> ids;
    const QJsonArray schemesArray = root.value(QStringLiteral("schemes")).toArray();
    for (const QJsonValue &value : schemesArray) {
        if (!value.isObject()) {
            setError(error, QStringLiteral("Each scheme must be an object."));
            return false;
        }
        Scheme scheme;
        if (!Scheme::fromJson(value.toObject(), &scheme, error)) return false;
        if (ids.contains(scheme.id)) {
            setError(error, QStringLiteral("Duplicate scheme id: %1").arg(scheme.id));
            return false;
        }
        ids.insert(scheme.id);
        parsed.schemes.append(scheme);
    }
    *this = parsed;
    return true;
}

bool SchemeStore::loadFile(const QString &path, QString *error)
{
    if (error) error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Could not open keymap %1: %2").arg(path, file.errorString()));
        return false;
    }
    const QByteArray json = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        setError(error, QStringLiteral("Could not read keymap %1: %2").arg(path, file.errorString()));
        return false;
    }
    return loadJson(json, error);
}

bool SchemeStore::saveFile(const QString &path, QString *error) const
{
    if (error) error->clear();
    const QByteArray json = toJson(error);
    if (json.isEmpty()) return false;
    return writeAtomically(path, json, error);
}

bool SchemeStore::migrateLegacyJson(const QByteArray &json, const QString &schemeName,
                                    SchemeStore *store, QString *error)
{
    if (error) error->clear();
    if (!store || schemeName.trimmed().isEmpty()) {
        setError(error, QStringLiteral("Migration requires a destination store and scheme name."));
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Invalid legacy keymap JSON: %1").arg(parseError.errorString()));
        return false;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("Legacy keymap JSON root must be an object."));
        return false;
    }
    const QJsonObject root = document.object();
    if (root.contains(QStringLiteral("version")) || root.contains(QStringLiteral("schemes"))) {
        setError(error, QStringLiteral("Input is not a legacy keymap document."));
        return false;
    }
    if (root.contains(QStringLiteral("switchKey"))
        && !root.value(QStringLiteral("switchKey")).isString()) {
        setError(error, QStringLiteral("Legacy switchKey must be a string."));
        return false;
    }
    if (root.contains(QStringLiteral("keyMapNodes"))
        && !root.value(QStringLiteral("keyMapNodes")).isArray()) {
        setError(error, QStringLiteral("Legacy keyMapNodes must be an array."));
        return false;
    }

    Scheme scheme;
    scheme.id = newId();
    scheme.name = schemeName.trimmed();
    scheme.type = QStringLiteral("local");
    const QString legacySwitchKey = root.value(QStringLiteral("switchKey")).toString(QStringLiteral("Key_QuoteLeft"));

    if (root.contains(QStringLiteral("mouseMoveMap"))) {
        const QJsonValue mouseMapValue = root.value(QStringLiteral("mouseMoveMap"));
        if (!mouseMapValue.isObject()) {
            setError(error, QStringLiteral("Legacy mouseMoveMap must be an object."));
            return false;
        }
        const QJsonObject mouseMap = mouseMapValue.toObject();
        QPointF start;
        if (!readLegacyPoint(mouseMap, QStringLiteral("startPos"), &start, error)) return false;
        std::shared_ptr<AimWidget> aim(new AimWidget);
        aim->id = newId();
        aim->position = start;
        if (!KeyBinding::fromLegacyCode(legacySwitchKey, &aim->toggleKey, error)) return false;

        double ratio = 0.0;
        double legacyRatioX = 0.0;
        double legacyRatioY = 0.0;
        const QJsonValue ratioValue = mouseMap.value(QStringLiteral("speedRatio"));
        const QJsonValue xValue = mouseMap.value(QStringLiteral("speedRatioX"));
        const QJsonValue yValue = mouseMap.value(QStringLiteral("speedRatioY"));
        if (ratioValue.isUndefined() && xValue.isUndefined() && yValue.isUndefined()) {
            setError(error, QStringLiteral("Legacy mouseMoveMap has no sensitivity ratio."));
            return false;
        }
        if (!ratioValue.isUndefined()
            && (!ratioValue.isDouble() || !std::isfinite(ratioValue.toDouble())
                || ratioValue.toDouble() < 0.001 || ratioValue.toDouble() > 10000.0)) {
            setError(error, QStringLiteral("Legacy speedRatio must be a finite number of at least 0.001."));
            return false;
        }
        if (!xValue.isUndefined()
            && (!xValue.isDouble() || !std::isfinite(xValue.toDouble())
                || xValue.toDouble() < 0.001 || xValue.toDouble() > 10000.0)) {
            setError(error, QStringLiteral("Legacy speedRatioX must be a finite number of at least 0.001."));
            return false;
        }
        if (!yValue.isUndefined()
            && (!yValue.isDouble() || !std::isfinite(yValue.toDouble())
                || yValue.toDouble() < 0.001 || yValue.toDouble() > 10000.0)) {
            setError(error, QStringLiteral("Legacy speedRatioY must be a finite number of at least 0.001."));
            return false;
        }
        ratio = ratioValue.toDouble(1.0);
        legacyRatioX = xValue.toDouble(ratio);
        legacyRatioY = yValue.toDouble(ratio > 0.0 ? ratio / 2.25 : 1.0);
        if (!xValue.isUndefined() && yValue.isUndefined() && ratioValue.isUndefined())
            legacyRatioY = legacyRatioX / 2.25;
        if (!yValue.isUndefined() && xValue.isUndefined() && ratioValue.isUndefined())
            legacyRatioX = legacyRatioY * 2.25;
        aim->sensX = 1.0 / legacyRatioX;
        aim->sensY = 1.0 / legacyRatioY;
        if (!aim->isValid(error)) return false;
        scheme.widgets.append(aim);

        if (mouseMap.contains(QStringLiteral("smallEyes"))) {
            if (!mouseMap.value(QStringLiteral("smallEyes")).isObject()) {
                setError(error, QStringLiteral("Legacy smallEyes must be an object."));
                return false;
            }
            const QJsonObject smallEyes = mouseMap.value(QStringLiteral("smallEyes")).toObject();
            if (smallEyes.value(QStringLiteral("type")).toString() != QStringLiteral("KMT_CLICK")) {
                setError(error, QStringLiteral("Only legacy KMT_CLICK smallEyes mappings can be migrated."));
                return false;
            }
            QPointF smallPosition;
            KeyBinding smallKey;
            if (!readLegacyPoint(smallEyes, QStringLiteral("pos"), &smallPosition, error)
                || !legacyBinding(smallEyes, QStringLiteral("key"), &smallKey, error)) return false;
            std::shared_ptr<AimWidget> smallAim(new AimWidget);
            smallAim->id = newId();
            smallAim->position = smallPosition;
            smallAim->toggleKey = smallKey;
            smallAim->sensX = aim->sensX;
            smallAim->sensY = aim->sensY;
            smallAim->label = smallEyes.value(QStringLiteral("comment")).toString();
            scheme.widgets.append(smallAim);
        }
    } else if (!legacySwitchKey.isEmpty()) {
        KeyBinding activation;
        if (!KeyBinding::fromLegacyCode(legacySwitchKey, &activation, error)) return false;
        scheme.activationKey = activation.code;
    }

    const QJsonArray nodes = root.value(QStringLiteral("keyMapNodes")).toArray();
    for (const QJsonValue &value : nodes) {
        if (!value.isObject() || !readLegacyNode(value.toObject(), &scheme, error)) return false;
    }
    if (!scheme.isValid(error)) return false;

    SchemeStore parsed;
    parsed.schemes.append(scheme);
    *store = parsed;
    return true;
}

bool SchemeStore::migrateLegacyFileInPlace(const QString &path, QString *backupPath,
                                           QString *error)
{
    if (error) error->clear();
    QFile source(path);
    if (!source.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Could not open legacy keymap %1: %2").arg(path, source.errorString()));
        return false;
    }
    const QByteArray original = source.readAll();
    if (source.error() != QFileDevice::NoError) {
        setError(error, QStringLiteral("Could not read legacy keymap %1: %2").arg(path, source.errorString()));
        return false;
    }
    source.close();

    SchemeStore migrated;
    if (!migrateLegacyJson(original, QFileInfo(path).completeBaseName(), &migrated, error)) return false;
    const QByteArray upgraded = migrated.toJson(error);
    if (upgraded.isEmpty()) return false;

    const QString backup = path + QStringLiteral(".v1.bak");
    if (QFileInfo::exists(backup)) {
        setError(error, QStringLiteral("Migration backup already exists: %1").arg(backup));
        return false;
    }
    if (!writeAtomically(backup, original, error)) return false;
    if (!writeAtomically(path, upgraded, error)) return false;
    if (backupPath) *backupPath = backup;
    return true;
}
