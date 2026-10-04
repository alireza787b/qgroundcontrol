#include "PixEagleTargetController.h"

#include <QtCore/QJsonArray>
#include <QtCore/QUuid>

namespace {
QJsonArray trackerEntries(const QJsonObject& catalog)
{
    auto entries = catalog.value("ui_trackers").toArray();
    const auto builtins = catalog.value("tracker_types").toObject();
    for (const auto& value : builtins) {
        entries.append(value);
    }
    return entries;
}
}  // namespace

PixEagleTargetController::PixEagleTargetController(QObject* parent)
    : QObject(parent)
{}

void PixEagleTargetController::setClient(PixEagleClient* client)
{
    if (_client == client) {
        return;
    }
    cancelGesture();
    if (_client) {
        if (_modelsRequested) {
            _client->setModelsRequested(false);
        }
        disconnect(_client, nullptr, this, nullptr);
    }
    _client = client;
    _controlToken.clear();
    _controlGuard = {};
    _controlClient = nullptr;
    _notice.clear();
    _modelNotice.clear();
    _modelControlToken.clear();
    _modelControlGeneration.clear();
    _modelControlGuard = {};
    _modelControlClient = nullptr;
    _modelsRequested = false;
    if (_client) {
        connect(_client, &PixEagleClient::targetChanged, this, &PixEagleTargetController::_stateChanged);
        connect(_client, &PixEagleClient::modelsChanged, this, &PixEagleTargetController::_stateChanged);
        connect(_client, &PixEagleClient::changed, this, &PixEagleTargetController::_stateChanged);
        connect(_client, &QObject::destroyed, this, &PixEagleTargetController::_stateChanged);
        connect(_client, &PixEagleClient::targetActionFinished, this,
                [this](const QString& action, const QString& outcome, const QString& message) {
                    _notice = outcome == QStringLiteral("accepted") ? QString() : message;
                    if (action == QStringLiteral("model_select")) {
                        _modelNotice = outcome == QStringLiteral("accepted") ? QString() : message;
                    }
                    _stateChanged();
                });
        _client->refreshTargetState();
    }
    emit changed();
}

void PixEagleTargetController::setSurface(PixEagleVideoItem* surface)
{
    if (_surface == surface) {
        return;
    }
    cancelGesture();
    if (_surface) {
        disconnect(_surface, nullptr, this, nullptr);
    }
    _surface = surface;
    if (_surface) {
        connect(_surface, &PixEagleVideoItem::videoSinkChanged, this, &PixEagleTargetController::cancelGesture);
        connect(_surface, &PixEagleVideoItem::selectionChanged, this, &PixEagleTargetController::_stateChanged);
        connect(_surface, &PixEagleVideoItem::selectionInvalidated, this, [this]() {
            if (!_gestureToken.isEmpty()) {
                _notice = _surface && !_surface->selectionError().isEmpty()
                              ? _surface->selectionError()
                              : tr("Selection changed or expired. Select the target again.");
                cancelGesture();
            }
        });
        connect(_surface, &QObject::destroyed, this, &PixEagleTargetController::_stateChanged);
    }
    emit changed();
}

bool PixEagleTargetController::_allowed(const QString& action) const
{
    return _client && _client->targetWriteAllowed() &&
           _client->targetState().value("allowed_actions").toArray().contains(action);
}

bool PixEagleTargetController::_external() const
{
    return _client && _client->targetState().value("mode").toString() == QStringLiteral("external");
}

bool PixEagleTargetController::canSelect() const
{
    return _selectionEnabled && _surface && _surface->selectionReady() && (supportsPoint() || supportsRectangle()) &&
           _allowed(_external() ? QStringLiteral("gimbal_control")
                                : (smartMode() ? QStringLiteral("smart_click") : QStringLiteral("tracking_start")));
}

void PixEagleTargetController::setTapToTarget(bool enabled)
{
    if (_tapToTarget == enabled) {
        return;
    }
    _tapToTarget = enabled;
    cancelGesture();
}

void PixEagleTargetController::setSelectionEnabled(bool enabled)
{
    if (_selectionEnabled == enabled) {
        return;
    }
    _selectionEnabled = enabled;
    cancelGesture();
}

bool PixEagleTargetController::selectionArmed() const
{
    return canSelect() && (_tapToTarget || _armed);
}

bool PixEagleTargetController::canCancel() const
{
    const auto status = _client ? _client->targetState().value("target_status").toString() : QString();
    return _client && !_client->targetState().value("following_active").toBool() &&
           (trackingActive() || status == QStringLiteral("lost") || status == QStringLiteral("acquiring")) &&
           _allowed(_external() ? QStringLiteral("gimbal_control") : QStringLiteral("tracking_stop"));
}

bool PixEagleTargetController::canConfigure() const
{
    return _client && !_client->targetState().value("following_active").toBool() &&
           _allowed(QStringLiteral("tracker_switch"));
}

QVariantList PixEagleTargetController::selectionModes() const
{
    QVariantList result;
    if (!_client || !_client->targetStateFresh()) {
        return result;
    }
    if (_external()) {
        for (const auto& value : _client->targetState().value("external_selection_modes").toArray()) {
            const auto row = value.toObject();
            if (!row.value("id").toString().isEmpty() && !row.value("label").toString().isEmpty() &&
                row.value("point").isBool() && row.value("rectangle").isBool()) {
                result.append(QVariantMap{{"id", row.value("id").toString()},
                                          {"label", row.value("label").toString()},
                                          {"available", true},
                                          {"point", row.value("point").toBool()},
                                          {"rectangle", row.value("rectangle").toBool()}});
            }
        }
    } else {
        const bool smartAvailable = smartMode() || _client->targetState()
                                                       .value("mode_availability")
                                                       .toObject()
                                                       .value("smart")
                                                       .toObject()
                                                       .value("available")
                                                       .toBool();
        result = {
            QVariantMap{
                {"id", "classic"}, {"label", tr("Classic")}, {"available", true}, {"point", true}, {"rectangle", true}},
            QVariantMap{{"id", "smart"},
                        {"label", tr("Smart")},
                        {"available", smartAvailable},
                        {"point", true},
                        {"rectangle", false}}};
    }
    return result;
}

QString PixEagleTargetController::selectedSelectionMode() const
{
    return _external() ? _client->targetState().value("external_selection_mode").toString()
                       : (smartMode() ? QStringLiteral("smart") : QStringLiteral("classic"));
}

bool PixEagleTargetController::canChangeMode() const
{
    if (!_client || _client->targetState().value("following_active").toBool() ||
        !_allowed(_external() ? QStringLiteral("gimbal_control") : QStringLiteral("smart_mode_toggle"))) {
        return false;
    }
    for (const auto& value : selectionModes()) {
        const auto row = value.toMap();
        if (row.value("available").toBool() && row.value("id").toString() != selectedSelectionMode()) {
            return true;
        }
    }
    return false;
}

bool PixEagleTargetController::supportsPoint() const
{
    if (!_external()) {
        return true;
    }
    for (const auto& value : selectionModes()) {
        const auto row = value.toMap();
        if (row.value("id").toString() == selectedSelectionMode()) {
            return row.value("point").toBool();
        }
    }
    return false;
}

bool PixEagleTargetController::supportsRectangle() const
{
    if (!_external()) {
        return !smartMode();
    }
    for (const auto& value : selectionModes()) {
        const auto row = value.toMap();
        if (row.value("id").toString() == selectedSelectionMode()) {
            return row.value("rectangle").toBool();
        }
    }
    return false;
}

bool PixEagleTargetController::trackingActive() const
{
    return _client && _client->targetStateFresh() && _client->targetState().value("tracking_active").toBool();
}

bool PixEagleTargetController::busy() const
{
    return _client && _client->targetMutationPending();
}

bool PixEagleTargetController::smartMode() const
{
    if (!_client) {
        return false;
    }
    const auto state = _client->targetState();
    return state.value("mode").toString() == QStringLiteral("smart") ||
           (_external() && state.value("external_selection_mode").toString() == QStringLiteral("smart"));
}

QString PixEagleTargetController::trackingState() const
{
    if (busy()) {
        return QStringLiteral("updating");
    }
    if (!_client || !_client->targetStateFresh()) {
        return QStringLiteral("unknown");
    }
    const auto status = _client->targetState().value("target_status").toString();
    if (status == QStringLiteral("tracking") || status == QStringLiteral("acquiring") ||
        status == QStringLiteral("lost")) {
        return status;
    }
    return QStringLiteral("idle");
}

QString PixEagleTargetController::statusText() const
{
    if (busy()) {
        return tr("Updating target…");
    }
    if (!_client || !_client->targetStateFresh()) {
        return tr("Target status unavailable");
    }
    if (_client->targetState().value("following_active").toBool()) {
        return tr("Following active · Tap to replace target");
    }
    const auto status = _client->targetState().value("target_status").toString();
    if (status == QStringLiteral("acquiring")) {
        return tr("Acquiring target");
    }
    if (status == QStringLiteral("lost")) {
        return tr("Target lost · Select again");
    }
    if (trackingActive()) {
        return tr("Tracking target");
    }
    return tr("No target selected");
}

QString PixEagleTargetController::instructionText() const
{
    if (!_notice.isEmpty()) {
        return _notice;
    }
    if (busy()) {
        return tr("Waiting for PixEagle to confirm the action.");
    }
    if (!_client || !_client->targetStateFresh()) {
        return tr("Waiting for current PixEagle target state.");
    }
    if (_client->targetState().value("following_active").toBool()) {
        return canSelect() ? tr("Tap another target to replace it. Commands pause until the new target is confirmed.")
                           : tr("Following active. Target selection is unavailable for this video.");
    }
    if (!_client->targetWriteAllowed()) {
        return tr(
            "View only. Target control needs operator access and a valid aircraft association, or no connected "
            "aircraft.");
    }
    if (selectionArmed()) {
        if (trackingActive()) {
            return smartMode() ? tr("Tap another detected target to replace it.")
                               : tr("Tap another target or drag a box to replace it.");
        }
        return smartMode() ? tr("Tap a detected target in the video.") : tr("Tap a target or drag a box around it.");
    }
    if (!_surface || !_surface->selectionReady()) {
        return _surface && _surface->frameFresh() ? tr("Target selection is unavailable for this video source.")
                                                  : tr("Waiting for current video before selecting.");
    }
    return {};
}

QString PixEagleTargetController::modeUnavailableReason() const
{
    if (!_client || !_client->targetStateFresh()) {
        return {};
    }
    if (_external()) {
        return selectionModes().isEmpty() ? tr("Camera tracking modes are unavailable. Check camera status.")
                                          : QString();
    }
    const auto availability = _client->targetState().value("mode_availability").toObject().value("smart").toObject();
    return availability.value("available").toBool()
               ? QString()
               : tr("Smart unavailable: %1")
                     .arg(availability.value("reason").toString(tr("Check PixEagle's tracker setup.")));
}

QVariantList PixEagleTargetController::targetEngines() const
{
    QVariantList result;
    if (!_client) {
        return result;
    }
    QStringList engines;
    for (const auto& value : trackerEntries(_client->targetCatalog())) {
        const auto entry = value.toObject();
        if (!entry.value("available").toBool() || entry.value("smart_mode").toBool()) {
            continue;
        }
        const auto engine = entry.value("target_engine").toString(QStringLiteral("local"));
        if ((engine == "local" || engine == "camera") && !engines.contains(engine)) {
            engines.append(engine);
            result.append(QVariantMap{{"id", engine}, {"label", engine == "local" ? tr("PixEagle") : tr("Camera")}});
        }
    }
    const auto current = targetEngine();
    if (!engines.contains(current)) {
        result.append(
            QVariantMap{{"id", current},
                        {"label", current == "camera" ? tr("Camera (unavailable)") : tr("PixEagle (unavailable)")}});
    }
    return result;
}

QString PixEagleTargetController::savedEngine() const
{
    if (!_client || !_client->targetStateFresh()) {
        return {};
    }
    const auto saved = _client->targetState().value("saved_engine").toString();
    return saved == "local" || saved == "camera" ? saved : QString{};
}

QVariantList PixEagleTargetController::trackerChoices() const
{
    QVariantList result;
    if (!_client) {
        return result;
    }
    QStringList types;
    for (const auto& value : trackerEntries(_client->targetCatalog())) {
        const auto entry = value.toObject();
        if (!entry.value("available").toBool() || entry.value("smart_mode").toBool() ||
            entry.value("target_engine").toString() == "camera") {
            continue;
        }
        const auto type = entry.value("request_tracker_type").toString(entry.value("name").toString());
        const auto factory = entry.value("factory_key").toString(type);
        if (!type.isEmpty() && !types.contains(factory.toLower())) {
            types.append(factory.toLower());
            result.append(QVariantMap{
                {"label", entry.value("display_name").toString(type)}, {"value", type}, {"factory_key", factory}});
        }
    }
    return result;
}

QVariantList PixEagleTargetController::unavailableTrackers() const
{
    QVariantList result;
    if (!_client) {
        return result;
    }
    QStringList types;
    for (const auto& choice : trackerChoices()) {
        types.append(choice.toMap().value("factory_key").toString().toLower());
    }
    for (const auto& value : trackerEntries(_client->targetCatalog())) {
        const auto entry = value.toObject();
        const auto type = entry.value("request_tracker_type").toString(entry.value("name").toString());
        const auto factory = entry.value("factory_key").toString(type).toLower();
        if (entry.value("available").toBool() || type.isEmpty() || types.contains(factory)) {
            continue;
        }
        types.append(factory);
        result.append(QVariantMap{
            {"label", entry.value("display_name").toString(type)},
            {"reason", entry.value("unavailable_reason").toString(tr("Not available in this PixEagle runtime."))}});
    }
    return result;
}

int PixEagleTargetController::trackerIndex() const
{
    if (!_client) {
        return -1;
    }
    const auto current = _client->targetState().value("tracker_type").toString();
    const auto choices = trackerChoices();
    for (int i = 0; i < choices.size(); ++i) {
        const auto choice = choices[i].toMap();
        if (choice.value("value").toString().compare(current, Qt::CaseInsensitive) == 0 ||
            choice.value("factory_key").toString().compare(current, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

QVariantList PixEagleTargetController::modelChoices() const
{
    QVariantList result;
    if (!_client || !_client->modelStateFresh()) {
        return result;
    }
    const auto models = _client->modelInventory().value("models").toArray();
    for (const auto& value : models) {
        const auto entry = value.toObject();
        result.append(QVariantMap{{"modelId", entry.value("model_id").toString()},
                                  {"label", entry.value("display_name").toString()},
                                  {"available", entry.value("available").toBool()},
                                  {"reason", entry.value("unavailable_reason").toString()},
                                  {"task", entry.value("task").toString()},
                                  {"labels", entry.value("labels").toArray().toVariantList()},
                                  {"totalLabels", entry.value("total_labels").toInt()},
                                  {"hasMoreLabels", entry.value("has_more_labels").toBool()}});
    }
    return result;
}

QString PixEagleTargetController::configuredModelId() const
{
    return _client && _client->modelStateFresh() ? _client->modelInventory().value("configured_model_id").toString()
                                                 : QString();
}

QString PixEagleTargetController::modelStatusText() const
{
    if (busy()) {
        return tr("Waiting for PixEagle. Loading a model can take up to a minute.");
    }
    if (!_modelNotice.isEmpty()) {
        return _modelNotice;
    }
    if (!_client || !_client->modelStateFresh()) {
        return tr("Model information is unavailable. Refresh to check this companion.");
    }
    const auto inventory = _client->modelInventory();
    if (!inventory.value("available").toBool()) {
        return inventory.value("unavailable_reason").toString(tr("Smart models are unavailable on this companion."));
    }
    const auto displayName = [&inventory](const QString& id) {
        for (const auto& value : inventory.value("models").toArray()) {
            const auto entry = value.toObject();
            if (entry.value("model_id").toString() == id) {
                return entry.value("display_name").toString();
            }
        }
        return id;
    };
    const QString activeId = inventory.value("active_model_id").toString();
    if (!activeId.isEmpty()) {
        const auto runtime = inventory.value("runtime").toObject();
        const QString device = runtime.value("device").toString();
        if (runtime.value("fallback_occurred").toBool()) {
            return tr("%1 running on %2 after device fallback.").arg(displayName(activeId), device);
        }
        return device.isEmpty() ? tr("%1 running.").arg(displayName(activeId))
                                : tr("%1 running on %2.").arg(displayName(activeId), device);
    }
    return configuredModelId().isEmpty()
               ? tr("Choose an installed model for Smart mode.")
               : tr("Selected: %1. Smart mode is not running.").arg(displayName(configuredModelId()));
}

bool PixEagleTargetController::canSelectModel() const
{
    return _client && _client->modelSelectionAllowed();
}

QString PixEagleTargetController::modelFeedbackText() const
{
    if (!_modelNotice.isEmpty()) {
        return _modelNotice;
    }
    if (busy()) {
        return {};
    }
    if (!_client || !_client->modelStateFresh()) {
        return tr("Checking available models…");
    }
    const auto inventory = _client->modelInventory();
    if (!inventory.value("available").toBool()) {
        return inventory.value("unavailable_reason").toString(tr("Smart models are unavailable on this companion."));
    }
    if (smartMode() && trackingActive()) {
        return tr("Cancel tracking before changing models.");
    }
    return {};
}

void PixEagleTargetController::setModelsRequested(bool requested)
{
    if (_modelsRequested == requested) {
        return;
    }
    _modelsRequested = requested;
    _modelControlToken.clear();
    _modelControlGeneration.clear();
    _modelControlGuard = {};
    _modelControlClient = nullptr;
    if (requested) {
        cancelGesture();
        _modelNotice.clear();
    }
    if (_client) {
        _client->setModelsRequested(requested);
    }
    emit changed();
}

void PixEagleTargetController::refreshModels()
{
    _modelNotice.clear();
    if (_client) {
        _client->refreshModels();
    }
    emit changed();
}

QString PixEagleTargetController::captureModelContext()
{
    if (!canSelectModel()) {
        return {};
    }
    _modelControlToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    _modelControlClient = _client;
    _modelControlGuard = _client->targetGuard();
    _modelControlGeneration = _client->modelInventory().value("model_generation").toString();
    return _modelControlToken;
}

void PixEagleTargetController::selectModel(const QString& modelId, const QString& token)
{
    const bool valid = canSelectModel() && !token.isEmpty() && token == _modelControlToken &&
                       _client == _modelControlClient && _modelControlGuard == _client->targetGuard() &&
                       _modelControlGeneration == _client->modelInventory().value("model_generation").toString();
    const auto guard = _modelControlGuard;
    const auto generation = _modelControlGeneration;
    _modelControlToken.clear();
    _modelControlGeneration.clear();
    _modelControlGuard = {};
    _modelControlClient = nullptr;
    _notice.clear();
    cancelGesture();
    _modelNotice.clear();
    if (!valid || !_client->submitModelSelection(modelId, guard, generation)) {
        _modelNotice = tr("Model or connection changed. Refresh the models and choose again.");
    }
    emit changed();
}

void PixEagleTargetController::_clearGesture()
{
    _gestureToken.clear();
    _gestureGuard = {};
    if (_surface) {
        _surface->cancelSelection();
    }
}

void PixEagleTargetController::cancelGesture()
{
    _armed = false;
    _selectionGuard = {};
    cancelPointerGesture();
}

void PixEagleTargetController::cancelPointerGesture()
{
    _clearGesture();
    emit gestureInvalidated();
    emit changed();
}

void PixEagleTargetController::armSelection()
{
    if (_tapToTarget) {
        return;
    }
    if (_armed) {
        cancelGesture();
        return;
    }
    if (!canSelect()) {
        return;
    }
    _notice.clear();
    _armed = true;
    _selectionGuard = _client->targetGuard();
    emit changed();
}

bool PixEagleTargetController::beginGesture(QQuickItem* inputItem, qreal x, qreal y)
{
    if (!selectionArmed() || !_gestureToken.isEmpty()) {
        return false;
    }
    _notice.clear();
    _gestureGuard = _client->targetGuard();
    _gestureToken = _surface->beginSelection(inputItem, QPointF(x, y));
    if (_gestureToken.isEmpty()) {
        _notice = _surface->selectionError();
        _gestureGuard = {};
        emit changed();
        return false;
    }
    return true;
}

void PixEagleTargetController::finishGesture(qreal x, qreal y, bool rectangle)
{
    if (_gestureToken.isEmpty() || !_client || !_surface) {
        return;
    }
    const auto guard = _gestureGuard;
    const auto token = _gestureToken;
    _gestureToken.clear();
    _gestureGuard = {};
    _armed = false;
    _selectionGuard = {};
    const auto selection = _surface->finishSelection(token, QPointF(x, y), rectangle);
    if (!selection.value("valid").toBool() || guard != _client->targetGuard()) {
        _notice = selection.value("reason", tr("Target state changed. Select the target again.")).toString();
        emit changed();
        return;
    }
    if (selection.value("selection_geometry").toMap().value("target_revision").toString() !=
        guard.value("target_revision").toString()) {
        _notice = tr("The displayed frame belongs to an earlier target state. Select again on the current video.");
        emit changed();
        return;
    }
    if ((rectangle && !supportsRectangle()) || (!rectangle && !supportsPoint())) {
        _notice = tr("Smart mode needs a single click on a detected target.");
        emit changed();
        return;
    }
    const auto geometry = QJsonObject::fromVariantMap(selection.value(rectangle ? "rectangle" : "point").toMap());
    QJsonObject payload{
        {"frame", QJsonObject{{"provenance", QJsonObject::fromVariantMap(selection.value("context").toMap())},
                              {"selection_geometry",
                               QJsonObject::fromVariantMap(selection.value("selection_geometry").toMap())}}}};
    QString action;
    if (_external()) {
        action = QStringLiteral("gimbal_control");
        payload.insert("operation", "select");
        payload.insert("x", geometry.value("x").toDouble() + (rectangle ? geometry.value("width").toDouble() / 2 : 0));
        payload.insert("y", geometry.value("y").toDouble() + (rectangle ? geometry.value("height").toDouble() / 2 : 0));
        if (rectangle) {
            payload.insert("width", geometry.value("width"));
            payload.insert("height", geometry.value("height"));
        }
    } else {
        action = smartMode() ? QStringLiteral("smart_click") : QStringLiteral("tracking_start");
        auto coordinates = geometry;
        coordinates.insert("coordinate_space", "normalized");
        payload.insert(smartMode() ? "click" : (rectangle ? "bbox" : "point"), coordinates);
    }
    _submit(action, payload, guard);
}

void PixEagleTargetController::_submit(const QString& action, const QJsonObject& payload, const QJsonObject& guard)
{
    cancelGesture();
    _notice.clear();
    if (!_client || !_client->submitTargetAction(action, payload, guard)) {
        _notice = tr("Action unavailable or target state changed. Check the current state and try again.");
    }
    emit changed();
}

QString PixEagleTargetController::captureControlContext()
{
    if (!_client || !_client->targetWriteAllowed()) {
        return {};
    }
    _controlClient = _client;
    _controlGuard = _client->targetGuard();
    _controlToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return _controlToken;
}

QJsonObject PixEagleTargetController::_takeControlGuard(const QString& token)
{
    const bool valid = !token.isEmpty() && token == _controlToken && _client && _client == _controlClient &&
                       _controlGuard == _client->targetGuard();
    const auto guard = valid ? _controlGuard : QJsonObject{};
    _controlToken.clear();
    _controlGuard = {};
    _controlClient = nullptr;
    if (!valid) {
        _notice = tr("Target context changed. Review the current target and try again.");
        emit changed();
    }
    return guard;
}

void PixEagleTargetController::cancelTracking(const QString& token)
{
    if (!canCancel()) {
        return;
    }
    const auto guard = _takeControlGuard(token);
    if (guard.isEmpty()) {
        return;
    }
    _submit(_external() ? QStringLiteral("gimbal_control") : QStringLiteral("tracking_stop"),
            _external() ? QJsonObject{{"operation", "cancel"}} : QJsonObject{}, guard);
}

void PixEagleTargetController::setSmartMode(bool enabled, const QString& token)
{
    selectSelectionMode(enabled ? QStringLiteral("smart") : QStringLiteral("classic"), token);
}

void PixEagleTargetController::selectSelectionMode(const QString& mode, const QString& token)
{
    if (!canChangeMode() || mode == selectedSelectionMode()) {
        return;
    }
    bool available = false;
    for (const auto& value : selectionModes()) {
        const auto row = value.toMap();
        available |= row.value("id").toString() == mode && row.value("available").toBool();
    }
    if (!available) {
        return;
    }
    const auto guard = _takeControlGuard(token);
    if (guard.isEmpty()) {
        return;
    }
    _submit(_external() ? QStringLiteral("gimbal_control") : QStringLiteral("smart_mode_toggle"),
            _external() ? QJsonObject{{"operation", "set_mode"}, {"selection_mode", mode}}
                        : QJsonObject{{"enabled", mode == QStringLiteral("smart")}},
            guard);
}

void PixEagleTargetController::selectTargetEngine(const QString& engine, const QString& token, bool persist)
{
    if (!canConfigure() || (engine != "local" && engine != "camera")) {
        return;
    }
    if (engine == targetEngine()) {
        if (persist && savedEngine() != engine) {
            const auto guard = _takeControlGuard(token);
            const auto type = _client->targetState().value("tracker_type").toString();
            if (!guard.isEmpty() && !type.isEmpty()) {
                _submit(QStringLiteral("tracker_switch"),
                        QJsonObject{{"tracker_type", type}, {"persist", true}, {"restore_engine_selection", true}},
                        guard);
            }
        }
        return;
    }
    for (const auto& value : trackerEntries(_client->targetCatalog())) {
        const auto entry = value.toObject();
        if (!entry.value("available").toBool() || entry.value("smart_mode").toBool() ||
            entry.value("target_engine").toString(QStringLiteral("local")) != engine) {
            continue;
        }
        const auto type = entry.value("request_tracker_type").toString(entry.value("name").toString());
        if (type.isEmpty()) {
            continue;
        }
        const auto guard = _takeControlGuard(token);
        if (!guard.isEmpty()) {
            _submit(QStringLiteral("tracker_switch"),
                    QJsonObject{{"tracker_type", type}, {"persist", persist}, {"restore_engine_selection", true}},
                    guard);
        }
        return;
    }
}

void PixEagleTargetController::selectTracker(int index, const QString& token)
{
    const auto choices = trackerChoices();
    if (!canConfigure() || index < 0 || index >= choices.size() || index == trackerIndex()) {
        return;
    }
    const auto guard = _takeControlGuard(token);
    if (guard.isEmpty()) {
        return;
    }
    _submit(QStringLiteral("tracker_switch"),
            QJsonObject{{"tracker_type", choices[index].toMap().value("value").toString()}, {"persist", false}}, guard);
}

void PixEagleTargetController::_stateChanged()
{
    if (!_gestureToken.isEmpty() &&
        (!_client || !_client->targetWriteAllowed() || _gestureGuard != _client->targetGuard() || !_surface)) {
        _notice = tr("Target context changed. Select the target again.");
        cancelGesture();
    } else if (_armed &&
               (!_client || !_client->targetWriteAllowed() || !_surface || _selectionGuard != _client->targetGuard())) {
        cancelGesture();
    }
    emit changed();
}
