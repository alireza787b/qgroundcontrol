#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QVariantList>
#include <QtQmlIntegration/QtQmlIntegration>

class QNetworkAccessManager;
class QNetworkReply;

/// One aircraft's authenticated companion connection. Passwords never enter QSettings.
class PixEagleClient : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Connections are owned by PixEagleManager")
    Q_PROPERTY(QString endpoint READ endpoint WRITE setEndpoint NOTIFY endpointChanged)
    Q_PROPERTY(QString defaultEndpoint READ defaultEndpoint CONSTANT)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY changed)
    Q_PROPERTY(bool associationVerified READ associationVerified NOTIFY changed)
    Q_PROPERTY(bool canVerify READ canVerify NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString signedInAs READ signedInAs NOTIFY changed)
    Q_PROPERTY(bool rememberSignIn READ rememberSignIn NOTIFY changed)
    Q_PROPERTY(QString credentialStatus READ credentialStatus NOTIFY changed)
    Q_PROPERTY(QString vehicleLabel READ vehicleLabel NOTIFY changed)
    Q_PROPERTY(QString diagnostics READ diagnostics NOTIFY changed)
    Q_PROPERTY(QString runtimeStatusText READ runtimeStatusText NOTIFY changed)
    Q_PROPERTY(bool companionOnly READ companionOnly CONSTANT)
    Q_PROPERTY(QVariantList followerChoices READ followerChoices NOTIFY followingChanged)
    Q_PROPERTY(QString selectedFollower READ selectedFollower NOTIFY followingChanged)
    Q_PROPERTY(QString followingStatusText READ followingStatusText NOTIFY followingChanged)
    Q_PROPERTY(QString followingSummaryText READ followingSummaryText NOTIFY followingChanged)
    Q_PROPERTY(QString followingState READ followingState NOTIFY followingChanged)
    Q_PROPERTY(QString followingActionError READ followingActionError NOTIFY followingChanged)
    Q_PROPERTY(bool followingActive READ followingActive NOTIFY followingChanged)
    Q_PROPERTY(bool followerTestActive READ followerTestActive NOTIFY followingChanged)
    Q_PROPERTY(bool canStartFollowing READ canStartFollowing NOTIFY followingChanged)
    Q_PROPERTY(bool canStopFollowing READ canStopFollowing NOTIFY followingChanged)
    Q_PROPERTY(bool canSelectFollower READ canSelectFollower NOTIFY followingChanged)
    Q_PROPERTY(bool followingActionPending READ followingActionPending NOTIFY followingChanged)
    Q_PROPERTY(bool configAvailable READ configAvailable NOTIFY configChanged)
    Q_PROPERTY(bool configFresh READ configFresh NOTIFY configChanged)
    Q_PROPERTY(bool configBusy READ configBusy NOTIFY configChanged)
    Q_PROPERTY(bool canSetOsd READ canSetOsd NOTIFY configChanged)
    Q_PROPERTY(bool osdEnabled READ osdEnabled NOTIFY configChanged)
    Q_PROPERTY(QString osdStatusText READ osdStatusText NOTIFY configChanged)
    Q_PROPERTY(bool canApplyConfig READ canApplyConfig NOTIFY configChanged)
    Q_PROPERTY(bool canRestartBackend READ canRestartBackend NOTIFY configChanged)
    Q_PROPERTY(bool backendRestarting READ backendRestarting NOTIFY configChanged)
    Q_PROPERTY(QString backendRestartStatus READ backendRestartStatus NOTIFY configChanged)
    Q_PROPERTY(QString configStatusText READ configStatusText NOTIFY configChanged)
    Q_PROPERTY(QString configActionError READ configActionError NOTIFY configChanged)
    Q_PROPERTY(QVariantList pendingConfigChanges READ pendingConfigChanges NOTIFY configChanged)
    Q_PROPERTY(bool safetyAvailable READ safetyAvailable NOTIFY safetyChanged)
    Q_PROPERTY(bool safetyFresh READ safetyFresh NOTIFY safetyChanged)
    Q_PROPERTY(bool safetyActive READ safetyActive NOTIFY safetyChanged)
    Q_PROPERTY(bool safetyFollowerTest READ safetyFollowerTest NOTIFY safetyChanged)
    Q_PROPERTY(bool safetyBusy READ safetyBusy NOTIFY safetyChanged)
    Q_PROPERTY(bool canSetSafety READ canSetSafety NOTIFY safetyChanged)
    Q_PROPERTY(QString safetyStatusText READ safetyStatusText NOTIFY safetyChanged)

    Q_PROPERTY(bool cameraAvailable READ cameraAvailable NOTIFY cameraChanged)
    Q_PROPERTY(bool cameraFresh READ cameraFresh NOTIFY cameraChanged)
    Q_PROPERTY(QVariantMap cameraStatus READ cameraStatus NOTIFY cameraChanged)
    Q_PROPERTY(bool cameraActionPending READ cameraActionPending NOTIFY cameraChanged)
    Q_PROPERTY(bool cameraCanStop READ cameraCanStop NOTIFY cameraChanged)
    Q_PROPERTY(QString cameraError READ cameraError NOTIFY cameraChanged)
    Q_PROPERTY(QString cameraManualState READ cameraManualState NOTIFY cameraChanged)

public:
    explicit PixEagleClient(QObject* parent = nullptr, bool companionOnly = false);
    ~PixEagleClient() override;

    QString defaultEndpoint() const { return QStringLiteral("http://127.0.0.1:5077"); }

    QString endpoint() const { return _endpoint.toString(); }

    void setEndpoint(const QString& endpoint);

    bool enabled() const { return _enabled; }

    void setEnabled(bool enabled);

    bool busy() const { return !_reply.isNull(); }

    bool authenticated() const { return _authenticated; }

    bool associationVerified() const;
    bool canVerify() const;
    QString statusText() const;

    QString signedInAs() const { return _signedInAs; }

    bool rememberSignIn() const;

    QString credentialStatus() const { return _credentialStatus; }

    QString vehicleLabel() const;
    QString diagnostics() const;
    QString runtimeStatusText() const;

    QJsonObject connectionContext() const { return _context; }

    quint64 sessionGeneration() const { return _generation; }

    bool mediaAvailable() const;

    bool companionOnly() const { return _companionOnly; }

    QVariantList followerChoices() const;
    QString selectedFollower() const;
    QString followingStatusText() const;
    QString followingSummaryText() const;
    QString followingState() const;

    QString followingActionError() const { return _followingActionError; }

    bool followingActive() const;
    bool followerTestActive() const;
    bool canStartFollowing() const;
    bool canStopFollowing() const;
    bool canSelectFollower() const;

    bool followingActionPending() const { return !_followingActionReply.isNull(); }

    Q_INVOKABLE void refreshFollowing();
    Q_INVOKABLE QString captureFollowingContext() const;
    Q_INVOKABLE QString captureFollowingStop() const;
    Q_INVOKABLE bool selectFollower(const QString& mode, const QString& context);
    Q_INVOKABLE bool startFollowing(const QString& context);
    Q_INVOKABLE bool stopFollowing(const QString& context);

    QUrl mediaUrl() const;
    QByteArray mediaCookie() const;
    QString mediaOrigin() const;

    QJsonObject targetState() const { return _targetState; }

    QJsonObject targetCatalog() const { return _targetCatalog; }

    QJsonObject targetGuard() const;
    bool targetStateFresh() const;

    bool targetMutationPending() const { return !_targetActionReply.isNull(); }

    bool targetWriteAllowed() const;
    void refreshTargetState();
    bool submitTargetAction(const QString& action, const QJsonObject& payload, const QJsonObject& capturedGuard);

    QJsonObject modelInventory() const { return _modelInventory; }

    bool modelStateFresh() const;
    bool modelSelectionAllowed() const;
    void setModelsRequested(bool requested);
    void refreshModels();
    bool submitModelSelection(const QString& modelId, const QJsonObject& capturedTargetGuard,
                              const QString& modelGeneration);

    QString instanceId() const { return _context.value("instance_id").toString(); }

    QString aircraftUid() const { return _aircraftUid; }

    void setVehicleIdentity(int systemId, const QString& aircraftUid, bool online);
    void observeAircraftUid(const QString& aircraftUid);
    void setDuplicateAssociation(bool duplicate, const QString& otherVehicle = {});
    void setAutoVerifySingleVehicle(bool enabled);

    Q_INVOKABLE void signIn(const QString& username, const QString& password);
    Q_INVOKABLE void signInAt(const QString& endpoint, const QString& username, const QString& password);
    Q_INVOKABLE void signInAtRemembered(const QString& endpoint, const QString& username, const QString& password);
    Q_INVOKABLE void setRememberSignIn(bool remember);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void verifyVehicle();
    Q_INVOKABLE void refresh();

    bool configAvailable() const;
    bool configFresh() const;

    bool configBusy() const { return !_configActionReply.isNull(); }

    bool canSetOsd() const;
    bool osdEnabled() const;
    QString osdStatusText() const;
    bool canApplyConfig() const;
    bool canRestartBackend() const;

    bool backendRestarting() const { return _restartAge.isValid(); }

    QString backendRestartStatus() const;
    QString configStatusText() const;

    QString configActionError() const { return _configActionError; }

    QVariantList pendingConfigChanges() const;
    Q_INVOKABLE void setConfigRequested(bool requested);
    Q_INVOKABLE void refreshConfig();
    Q_INVOKABLE QString captureConfigContext() const;
    Q_INVOKABLE bool setOsdEnabled(bool enabled, const QString& context);
    Q_INVOKABLE bool applyConfig(const QString& context);
    Q_INVOKABLE bool restartBackend(const QString& context);

    bool safetyAvailable() const;
    bool safetyFresh() const;
    bool safetyActive() const;
    bool safetyFollowerTest() const;

    bool safetyBusy() const { return !_safetyActionReply.isNull(); }

    bool canSetSafety() const;
    QString safetyStatusText() const;
    Q_INVOKABLE void refreshSafety();
    Q_INVOKABLE QString captureSafetyContext() const;
    Q_INVOKABLE bool setSafetyActive(bool enabled, const QString& context);

    bool cameraAvailable() const;
    bool cameraFresh() const;
    QVariantMap cameraStatus() const;
    bool cameraActionPending() const;
    bool cameraCanStop() const;

    QString cameraError() const { return _cameraError; }

    QString cameraManualState() const { return _cameraManualState; }

    Q_INVOKABLE void setCameraRequested(bool requested);
    Q_INVOKABLE void refreshCamera();
    Q_INVOKABLE QString captureCameraContext() const;
    Q_INVOKABLE bool cameraStep(const QString& operation, int direction, const QString& context);
    Q_INVOKABLE bool beginCameraManual(const QString& operation, double value, const QString& context);
    Q_INVOKABLE bool updateCameraManual(const QString& operation, double value, const QString& context);
    Q_INVOKABLE bool stopCamera(const QString& context, bool acknowledgeError = false);

    static bool validateEndpoint(const QString& text, QUrl* result = nullptr);

signals:
    void changed();
    void endpointChanged();
    void contextChanged();
    void targetChanged();
    void modelsChanged();
    void targetActionFinished(const QString& action, const QString& outcome, const QString& message);
    void followingChanged();
    void configChanged();
    void safetyChanged();
    void cameraChanged();

private:
    enum class Request
    {
        Login,
        Logout,
        Context,
        Verify,
        Confirm
    };
    void _resetSession();
    QString _credentialKey() const;
    void _loadRememberedSignIn();
    void _saveRememberedSignIn();
    void _deleteRememberedSignIn(const QString& key);
    void _request(Request kind, const QJsonObject& body = {});
    void _finished(QNetworkReply* reply, Request kind, quint64 generation);
    bool _acceptSession(const QJsonObject& data);
    bool _acceptContext(const QJsonObject& data);
    void _maybeAutoVerify();
    bool _identitiesMatch() const;
    bool _identitiesReadyForVerification() const;
    QString _contextKey() const;
    void _clearContext();
    void _refreshRuntimeStatus();
    bool _readOnlyReady() const;
    bool _sessionReady() const;
    bool _targetReadReady() const;
    bool _modelReadReady() const;
    bool _mutationDestinationReady() const;
    QString _targetContextKey() const;
    void _clearTargetState();
    void _readTargetResource(bool catalog);
    bool _acceptTargetState(const QJsonObject& data);
    void _clearModelState();
    bool _acceptModelInventory(const QJsonObject& data);
    bool _submitAction(const QString& action, QJsonObject body, const QJsonObject& capturedGuard, int timeoutMs);
    void _finishTargetAction(QNetworkReply* reply, const QString& action, quint64 generation,
                             const QString& contextKey);
    bool _followingReadReady() const;
    bool _followingFresh() const;
    bool _acceptFollowing(const QJsonObject& data);
    void _clearFollowingState();
    QJsonObject _lastFollowingHandoff() const;
    bool _postFollowingAction(const QString& action, QJsonObject body, int timeoutMs);
    void _clearConfigState();
    void _clearSafetyState();
    bool _acceptConfigState(const QJsonObject& data);
    bool _postConfigAction(const QString& action, QJsonObject body, const QString& context);

    void _initializeCameraState();
    void _clearCameraState(bool preserveStop = false);
    bool _stopCameraBeforeReset();
    bool _acceptCameraStatus(const QJsonObject& data);
    bool _postCameraAction(QJsonObject body, bool stop);
    void _renewCameraManual();
    bool _validCameraIntent(const QString& operation, double value) const;
    QPointer<QNetworkReply> _cameraReply;
    QPointer<QNetworkReply> _cameraActionReply;
    QPointer<QNetworkReply> _cameraStopReply;
    QTimer _cameraExpiryTimer;
    QTimer _cameraRenewTimer;
    QElapsedTimer _cameraInputAge;
    QJsonObject _cameraIntent;
    QString _cameraGestureId;
    QString _cameraStopGestureId;
    QString _cameraManualState;
    qint64 _cameraSequence = 0;
    quint64 _cameraCommandGeneration = 0;
    bool _cameraBeginAccepted = false;
    QElapsedTimer _cameraAge;
    QJsonObject _cameraStatus;
    QJsonObject _cameraCapturedGuard;
    QString _cameraCapturedContext;
    QString _cameraError;
    QString _cameraClientId;
    bool _cameraRequested = false;

    QNetworkAccessManager* _network = nullptr;
    QPointer<QNetworkReply> _reply;
    QPointer<QNetworkReply> _statusReply;
    QPointer<QNetworkReply> _targetStateReply;
    QPointer<QNetworkReply> _targetCatalogReply;
    QPointer<QNetworkReply> _targetActionReply;
    QPointer<QNetworkReply> _modelsReply;
    QPointer<QNetworkReply> _followingReply;
    QPointer<QNetworkReply> _followingActionReply;
    QPointer<QNetworkReply> _configReply;
    QPointer<QNetworkReply> _configActionReply;
    QPointer<QNetworkReply> _safetyReply;
    QPointer<QNetworkReply> _safetyActionReply;
    QTimer _safetyExpiryTimer;
    QElapsedTimer _safetyAge;
    QJsonObject _safetyState;
    QString _safetyNotice;
    QTimer _configExpiryTimer;
    QElapsedTimer _configAge;
    QJsonObject _configState;
    QString _configActionError;
    bool _configRequested = false;
    QTimer _restartRecoveryTimer;
    QElapsedTimer _restartAge;
    QUrl _restartEndpoint;
    QString _restartInstance;
    QString _restartRuntime;
    QString _restartResult;
    bool _restartSignInAttempted = false;
    int _restartSignInAttempts = 0;
    qint64 _restartNextSignInMs = 0;
    void _recoverBackendRestart();
    QTimer _pollTimer;
    QTimer _expiryTimer;
    QTimer _statusExpiryTimer;
    QTimer _targetExpiryTimer;
    QTimer _modelExpiryTimer;
    QTimer _followingExpiryTimer;
    QElapsedTimer _requestTimer;
    QElapsedTimer _targetStateAge;
    QElapsedTimer _modelStateAge;
    QElapsedTimer _followingAge;
    QUrl _endpoint{defaultEndpoint()};
    QJsonObject _context;
    QJsonObject _runtimeStatus;
    QJsonObject _targetState;
    QJsonObject _targetCatalog;
    QJsonObject _modelInventory;
    QJsonObject _followingStatus;
    QByteArray _csrfHeader;
    QByteArray _csrfToken;
    QString _signedInAs;
    QString _credentialStatus;
    QString _pendingCredentialUsername;
    QString _pendingCredentialPassword;
    quint64 _credentialDecisionGeneration = 0;
    QString _error;
    QString _verifiedKey;
    QString _verificationReason;
    QString _pendingVerificationKey;
    QString _autoVerificationAttemptedKey;
    QString _aircraftUid;
    QString _observedUid;
    QString _conflictingVehicle;
    QString _targetActionName;
    QString _targetActionId;
    QString _targetActionModelId;
    QString _followingActionId;
    QString _followingActionName;
    QString _followingNotice;
    QString _followingActionError;
    int _systemId = 0;
    quint64 _generation = 0;
    bool _enabled = false;
    bool _authenticated = false;
    bool _online = false;
    bool _identityConflict = false;
    bool _duplicateAssociation = false;
    bool _autoVerifySingleVehicle = false;
    bool _autoVerificationPending = false;
    bool _modelsRequested = false;
    const bool _companionOnly;
};
