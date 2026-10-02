#ifndef KEYMAP_MODEL_WIDGET_H
#define KEYMAP_MODEL_WIDGET_H

#include <QJsonObject>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QVector>

#include <memory>

#include "keybinding.h"

class Widget
{
public:
    virtual ~Widget() {}

    QString id;
    QPointF position;
    double size = 40.0;
    int z = 0;
    bool enabled = true;
    QString label;

    virtual QString type() const = 0;
    virtual QJsonObject toJson() const;
    virtual bool isValid(QString *error = nullptr) const;
    static std::shared_ptr<Widget> fromJson(const QJsonObject &json, QString *error = nullptr);

protected:
    virtual void writeSpecific(QJsonObject *json) const = 0;
    virtual bool readSpecific(const QJsonObject &json, QString *error) = 0;
};

class BoundWidget : public Widget
{
public:
    KeyBinding binding;

    QJsonObject toJson() const override;
    bool isValid(QString *error = nullptr) const override;

protected:
    void writeBinding(QJsonObject *json) const;
    bool readBinding(const QJsonObject &json, QString *error);
};

class KeyWidget : public BoundWidget
{
public:
    int tapCount = 1;
    bool releaseMouse = false;
    int androidKey = -1;

    QString type() const override { return QStringLiteral("key"); }
    bool isValid(QString *error = nullptr) const override;

protected:
    void writeSpecific(QJsonObject *json) const override;
    bool readSpecific(const QJsonObject &json, QString *error) override;
};

class JoystickWidget : public Widget
{
public:
    KeyBinding up;
    KeyBinding left;
    KeyBinding down;
    KeyBinding right;
    double radius = 60.0;
    double speed = 8.0;
    double upOffset = -1.0;
    double leftOffset = -1.0;
    double downOffset = -1.0;
    double rightOffset = -1.0;
    QMap<QString, double> directionOffsets;

    QString type() const override { return QStringLiteral("joystick"); }
    bool isValid(QString *error = nullptr) const override;

protected:
    void writeSpecific(QJsonObject *json) const override;
    bool readSpecific(const QJsonObject &json, QString *error) override;
};

class AimWidget : public Widget
{
public:
    KeyBinding toggleKey;
    double sensX = 25.0;
    double sensY = 25.0;

    QString type() const override { return QStringLiteral("aim"); }
    bool isValid(QString *error = nullptr) const override;

protected:
    void writeSpecific(QJsonObject *json) const override;
    bool readSpecific(const QJsonObject &json, QString *error) override;
};

class FireWidget : public BoundWidget
{
public:
    double radius = 24.0;
    int tapCount = 1;
    bool releaseMouse = false;
    int androidKey = -1;

    QString type() const override { return QStringLiteral("fire"); }
    bool isValid(QString *error = nullptr) const override;

protected:
    void writeSpecific(QJsonObject *json) const override;
    bool readSpecific(const QJsonObject &json, QString *error) override;
};

class SlideWidget : public BoundWidget
{
public:
    QPointF endPosition;
    int startDelayMs = 0;
    double speed = 1.0;

    QString type() const override { return QStringLiteral("slide"); }
    bool isValid(QString *error = nullptr) const override;

protected:
    void writeSpecific(QJsonObject *json) const override;
    bool readSpecific(const QJsonObject &json, QString *error) override;
};

struct MacroStep
{
    QString action;
    KeyBinding binding;
    QPointF position;
    bool hasPosition = false;
    int delayMs = 0;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &json, MacroStep *step, QString *error = nullptr);
};

class MacroWidget : public BoundWidget
{
public:
    QVector<MacroStep> steps;

    QString type() const override { return QStringLiteral("macro"); }
    bool isValid(QString *error = nullptr) const override;

protected:
    void writeSpecific(QJsonObject *json) const override;
    bool readSpecific(const QJsonObject &json, QString *error) override;
};

class ClickRepeatWidget : public BoundWidget
{
public:
    int intervalMs = 100;

    QString type() const override { return QStringLiteral("clickRepeat"); }
    bool isValid(QString *error = nullptr) const override;

protected:
    void writeSpecific(QJsonObject *json) const override;
    bool readSpecific(const QJsonObject &json, QString *error) override;
};

class ObservedWidget : public BoundWidget
{
public:
    QString type() const override { return QStringLiteral("observed"); }

protected:
    void writeSpecific(QJsonObject *) const override {}
    bool readSpecific(const QJsonObject &json, QString *error) override
    {
        return readBinding(json, error);
    }
};

class ViewAngleWidget : public BoundWidget
{
public:
    double sensX = 25.0;
    double sensY = 25.0;

    QString type() const override { return QStringLiteral("viewAngle"); }
    bool isValid(QString *error = nullptr) const override;

protected:
    void writeSpecific(QJsonObject *json) const override;
    bool readSpecific(const QJsonObject &json, QString *error) override;
};

class RightMouseMoveWidget : public BoundWidget
{
public:
    double sensX = 25.0;
    double sensY = 25.0;

    QString type() const override { return QStringLiteral("rightMouseMove"); }
    bool isValid(QString *error = nullptr) const override;

protected:
    void writeSpecific(QJsonObject *json) const override;
    bool readSpecific(const QJsonObject &json, QString *error) override;
};

class IntelligentCastWidget : public BoundWidget
{
public:
    QString type() const override { return QStringLiteral("intelligentCast"); }

protected:
    void writeSpecific(QJsonObject *) const override {}
    bool readSpecific(const QJsonObject &json, QString *error) override
    {
        return readBinding(json, error);
    }
};

class CancelCastWidget : public BoundWidget
{
public:
    QString type() const override { return QStringLiteral("cancelCast"); }

protected:
    void writeSpecific(QJsonObject *) const override {}
    bool readSpecific(const QJsonObject &json, QString *error) override
    {
        return readBinding(json, error);
    }
};

class VisionExtensionWidget : public BoundWidget
{
public:
    QString type() const override { return QStringLiteral("visionExtension"); }

protected:
    void writeSpecific(QJsonObject *) const override {}
    bool readSpecific(const QJsonObject &json, QString *error) override
    {
        return readBinding(json, error);
    }
};

#endif
