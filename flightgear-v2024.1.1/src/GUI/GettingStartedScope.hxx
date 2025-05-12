#pragma once

#include <QObject>
#include <QQmlEngine>

class GettingStartedTipsController;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
Q_MOC_INCLUDE("GettingStartedTipsController.hxx")
#endif
class GettingStartedScopeAttached : public QObject
{
    Q_OBJECT

    Q_PROPERTY(GettingStartedTipsController* controller READ controller WRITE setController NOTIFY controllerChanged)
public:
    GettingStartedScopeAttached(QObject* parent);

    GettingStartedTipsController* controller() const
    {
        return _controller;
    }

public slots:
    void setController(GettingStartedTipsController* controller);

signals:
    void controllerChanged();

private:
    GettingStartedTipsController* _controller = nullptr;
};

class GettingStartedScope : public QObject
{
    Q_OBJECT

public:
    explicit GettingStartedScope(QObject *parent = nullptr);

     static GettingStartedScopeAttached* qmlAttachedProperties(QObject *object);
signals:

};

QML_DECLARE_TYPEINFO(GettingStartedScope, QML_HAS_ATTACHED_PROPERTIES)
