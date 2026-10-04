#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QVariantList>
#include <QtQmlIntegration/QtQmlIntegration>

#include "PixEagleClient.h"
#include "PixEagleVideoItem.h"

class PixEagleTargetController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by PixEagleVideoController")
    Q_PROPERTY(bool tapToTarget READ tapToTarget WRITE setTapToTarget NOTIFY changed)
    Q_PROPERTY(bool selectionEnabled READ selectionEnabled WRITE setSelectionEnabled NOTIFY changed)
    Q_PROPERTY(bool selectionArmed READ selectionArmed NOTIFY changed)
    Q_PROPERTY(bool canSelect READ canSelect NOTIFY changed)
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY changed)
    Q_PROPERTY(bool canConfigure READ canConfigure NOTIFY changed)
    Q_PROPERTY(bool canChangeMode READ canChangeMode NOTIFY changed)
    Q_PROPERTY(QVariantList selectionModes READ selectionModes NOTIFY changed)
    Q_PROPERTY(QString selectedSelectionMode READ selectedSelectionMode NOTIFY changed)
    Q_PROPERTY(bool supportsPoint READ supportsPoint NOTIFY changed)
    Q_PROPERTY(bool supportsRectangle READ supportsRectangle NOTIFY changed)
    Q_PROPERTY(bool trackingActive READ trackingActive NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool smartMode READ smartMode NOTIFY changed)
    Q_PROPERTY(bool externalTracker READ externalTracker NOTIFY changed)
    Q_PROPERTY(QString trackingState READ trackingState NOTIFY changed)
    Q_PROPERTY(QString actionNotice READ actionNotice NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString instructionText READ instructionText NOTIFY changed)
    Q_PROPERTY(QString modeUnavailableReason READ modeUnavailableReason NOTIFY changed)
    Q_PROPERTY(QVariantList targetEngines READ targetEngines NOTIFY changed)
    Q_PROPERTY(QString targetEngine READ targetEngine NOTIFY changed)
    Q_PROPERTY(QString savedEngine READ savedEngine NOTIFY changed)
    Q_PROPERTY(QVariantList trackerChoices READ trackerChoices NOTIFY changed)
    Q_PROPERTY(QVariantList unavailableTrackers READ unavailableTrackers NOTIFY changed)
    Q_PROPERTY(int trackerIndex READ trackerIndex NOTIFY changed)
    Q_PROPERTY(QVariantList modelChoices READ modelChoices NOTIFY changed)
    Q_PROPERTY(QString configuredModelId READ configuredModelId NOTIFY changed)
    Q_PROPERTY(QString modelStatusText READ modelStatusText NOTIFY changed)
    Q_PROPERTY(QString modelFeedbackText READ modelFeedbackText NOTIFY changed)
    Q_PROPERTY(bool canSelectModel READ canSelectModel NOTIFY changed)

public:
    explicit PixEagleTargetController(QObject* parent = nullptr);
    void setClient(PixEagleClient* client);
    void setSurface(PixEagleVideoItem* surface);

    bool tapToTarget() const { return _tapToTarget; }

    void setTapToTarget(bool enabled);

    bool selectionEnabled() const { return _selectionEnabled; }

    void setSelectionEnabled(bool enabled);
    bool selectionArmed() const;

    bool canSelect() const;
    bool canCancel() const;
    bool canConfigure() const;
    bool canChangeMode() const;
    QVariantList selectionModes() const;
    QString selectedSelectionMode() const;
    bool supportsPoint() const;
    bool supportsRectangle() const;
    bool trackingActive() const;
    bool busy() const;
    bool smartMode() const;

    bool externalTracker() const { return _external(); }

    QString statusText() const;
    QString trackingState() const;

    QString actionNotice() const { return _notice; }

    QString instructionText() const;
    QString modeUnavailableReason() const;
    QVariantList targetEngines() const;

    QString targetEngine() const { return _external() ? QStringLiteral("camera") : QStringLiteral("local"); }

    QString savedEngine() const;

    QVariantList trackerChoices() const;
    QVariantList unavailableTrackers() const;
    int trackerIndex() const;
    QVariantList modelChoices() const;
    QString configuredModelId() const;
    QString modelStatusText() const;
    QString modelFeedbackText() const;
    bool canSelectModel() const;

    Q_INVOKABLE void armSelection();
    Q_INVOKABLE void cancelGesture();
    Q_INVOKABLE void cancelPointerGesture();
    Q_INVOKABLE bool beginGesture(QQuickItem* inputItem, qreal x, qreal y);
    Q_INVOKABLE void finishGesture(qreal x, qreal y, bool rectangle);
    Q_INVOKABLE QString captureControlContext();
    Q_INVOKABLE void cancelTracking(const QString& token);
    Q_INVOKABLE void selectSelectionMode(const QString& mode, const QString& token);
    Q_INVOKABLE void setSmartMode(bool enabled, const QString& token);
    Q_INVOKABLE void selectTargetEngine(const QString& engine, const QString& token, bool persist = false);
    Q_INVOKABLE void selectTracker(int index, const QString& token);
    Q_INVOKABLE void setModelsRequested(bool requested);
    Q_INVOKABLE void refreshModels();
    Q_INVOKABLE QString captureModelContext();
    Q_INVOKABLE void selectModel(const QString& modelId, const QString& token);

signals:
    void changed();
    void gestureInvalidated();

private:
    bool _allowed(const QString& action) const;
    bool _external() const;
    void _stateChanged();
    void _submit(const QString& action, const QJsonObject& payload, const QJsonObject& guard);
    void _clearGesture();
    QJsonObject _takeControlGuard(const QString& token);

    QPointer<PixEagleClient> _client;
    QPointer<PixEagleVideoItem> _surface;
    QPointer<PixEagleClient> _controlClient;
    QJsonObject _controlGuard;
    QString _controlToken;
    QJsonObject _gestureGuard;
    QString _gestureToken;
    QJsonObject _selectionGuard;
    QString _notice;
    QString _modelNotice;
    QString _modelControlToken;
    QString _modelControlGeneration;
    QJsonObject _modelControlGuard;
    QPointer<PixEagleClient> _modelControlClient;
    bool _modelsRequested = false;
    bool _tapToTarget = true;
    bool _selectionEnabled = true;
    bool _armed = false;
};
