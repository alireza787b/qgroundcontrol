#include "PixEagleManager.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QSettings>

#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "PixEagleSettings.h"
#include "PixEagleVideoController.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"

namespace {
bool validDashboardUrl(const QUrl& url)
{
    return url.isValid() && !url.host().isEmpty() && (url.scheme() == "https" || url.scheme() == "http") &&
           url.userInfo().isEmpty() && !url.hasQuery() && !url.hasFragment() && url.port() != 0 &&
           !url.path().contains('\\');
}
}  // namespace

PixEagleManager::PixEagleManager(PixEagleSettings* settings, QObject* parent)
    : QObject(parent)
    , _settings(settings)
{
    _companion = new PixEagleClient(this, true);
    _companion->setEndpoint(QSettings().value("PixEagle/CompanionEndpoint").toString());
    _companion->setEnabled(_settings->integrationEnabled()->rawValue().toBool());
    connect(_companion, &PixEagleClient::endpointChanged, this,
            [this]() { QSettings().setValue("PixEagle/CompanionEndpoint", _companion->endpoint()); });
    connect(_companion, &PixEagleClient::endpointChanged, this, [this]() {
        if (activeClient() == _companion) {
            emit dashboardUrlChanged();
        }
    });
    connect(this, &PixEagleManager::activeClientChanged, this, &PixEagleManager::dashboardUrlChanged);
    auto* manager = MultiVehicleManager::instance();
    connect(manager, &MultiVehicleManager::vehicleAdded, this, &PixEagleManager::_addVehicle);
    connect(manager, &MultiVehicleManager::vehicleRemoved, this, [this](Vehicle* vehicle) {
        _provisionalEndpoints.remove(vehicle);
        if (auto* client = _clients.take(vehicle)) {
            if (_clients.isEmpty()) {
                _companion->setEndpoint(client->endpoint());
                _companion->restoreConnectionFrom(*client);
            }
            client->setEnabled(false);
            client->deleteLater();
        }
        _updateDuplicates();
        emit vehiclesChanged();
        emit followingVehiclesChanged();
        emit activeClientChanged();
    });
    connect(manager, &MultiVehicleManager::activeVehicleChanged, this, &PixEagleManager::activeClientChanged);
    connect(_settings->integrationEnabled(), &Fact::rawValueChanged, this, &PixEagleManager::_updateEnabled);
    connect(MAVLinkProtocol::instance(), &MAVLinkProtocol::messageReceived, this,
            [this](LinkInterface*, const mavlink_message_t& message) {
                if (message.msgid != MAVLINK_MSG_ID_AUTOPILOT_VERSION) {
                    return;
                }
                mavlink_autopilot_version_t version{};
                mavlink_msg_autopilot_version_decode(&message, &version);
                // QGC merges equal system IDs; conflicting raw UIDs must still invalidate the association.
                for (auto it = _clients.cbegin(); it != _clients.cend(); ++it) {
                    Vehicle* vehicle = it.key();
                    if (vehicle && vehicle->id() == message.sysid && vehicle->defaultComponentId() == message.compid) {
                        it.value()->observeAircraftUid(QString::number(version.uid));
                    }
                }
            });
    for (int index = 0; index < manager->vehicles()->count(); ++index) {
        _addVehicle(manager->vehicles()->value<Vehicle*>(index));
    }
}

void PixEagleManager::_addVehicle(Vehicle* vehicle)
{
    if (!vehicle || _clients.contains(vehicle)) {
        return;
    }
    auto* source = _clients.isEmpty() ? _companion : nullptr;
    auto* client = new PixEagleClient(this);
    if (source) {
        client->setEndpoint(source->endpoint());
    }
    _provisionalEndpoints.insert(vehicle, client->endpoint());
    _clients.insert(vehicle, client);
    connect(vehicle, &Vehicle::vehicleUIDChanged, this, [this, vehicle]() { _updateIdentity(vehicle); });
    connect(vehicle->vehicleLinkManager(), &VehicleLinkManager::communicationLostChanged, this,
            [this, vehicle]() { _updateIdentity(vehicle); });
    connect(client, &PixEagleClient::contextChanged, this, &PixEagleManager::_updateDuplicates);
    connect(client, &PixEagleClient::followingChanged, this, &PixEagleManager::followingVehiclesChanged);
    connect(client, &PixEagleClient::endpointChanged, this, [this, client]() {
        if (activeClient() == client) {
            emit dashboardUrlChanged();
        }
    });
    connect(client, &PixEagleClient::endpointChanged, this, [client]() {
        if (!client->aircraftUid().isEmpty()) {
            QSettings().setValue("PixEagle/Endpoints/" + client->aircraftUid(), client->endpoint());
        }
    });
    _updateIdentity(vehicle);
    client->setEnabled(_settings->integrationEnabled()->rawValue().toBool());
    if (source) {
        client->restoreConnectionFrom(*source);
    }
    emit vehiclesChanged();
    emit followingVehiclesChanged();
    emit activeClientChanged();
}

void PixEagleManager::_updateIdentity(Vehicle* vehicle)
{
    auto* client = _clients.value(vehicle);
    if (!vehicle || !client) {
        return;
    }
    const QString uid = vehicle->vehicleUID() ? QString::number(vehicle->vehicleUID()) : QString();
    const bool firstIdentity = client->aircraftUid().isEmpty() && !uid.isEmpty();
    client->setVehicleIdentity(vehicle->id(), uid, !vehicle->vehicleLinkManager()->communicationLost());
    if (firstIdentity) {
        const QString saved = QSettings().value("PixEagle/Endpoints/" + uid).toString();
        const bool inherited = _provisionalEndpoints.take(vehicle) == client->endpoint();
        if (inherited && !saved.isEmpty() && PixEagleClient::validateEndpoint(saved)) {
            client->setEndpoint(saved);
        }
        if (client->endpoint() != client->defaultEndpoint()) {
            QSettings().setValue("PixEagle/Endpoints/" + uid, client->endpoint());
        }
    }
    _updateDuplicates();
    emit vehiclesChanged();
}

void PixEagleManager::_updateDuplicates()
{
    for (auto it = _clients.cbegin(); it != _clients.cend(); ++it) {
        QStringList conflictingVehicles;
        for (auto other = _clients.cbegin(); other != _clients.cend(); ++other) {
            if (it == other) {
                continue;
            }
            const auto* a = it.value();
            const auto* b = other.value();
            if ((!a->aircraftUid().isEmpty() && a->aircraftUid() == b->aircraftUid()) ||
                (!a->instanceId().isEmpty() && a->instanceId() == b->instanceId())) {
                conflictingVehicles.append(b->vehicleLabel());
            }
        }
        conflictingVehicles.sort();
        it.value()->setDuplicateAssociation(!conflictingVehicles.isEmpty(), conflictingVehicles.join(", "));
        it.value()->setAutoVerifySingleVehicle(_clients.size() == 1 && conflictingVehicles.isEmpty());
    }
}

void PixEagleManager::_updateEnabled()
{
    const bool enabled = _settings->integrationEnabled()->rawValue().toBool();
    _companion->setEnabled(enabled);
    for (auto* client : std::as_const(_clients)) {
        client->setEnabled(enabled);
    }
    emit followingVehiclesChanged();
}

PixEagleClient* PixEagleManager::activeClient() const
{
    return _clients.value(MultiVehicleManager::instance()->activeVehicle(), _companion);
}

QString PixEagleManager::_dashboardSettingsKey() const
{
    const auto* client = activeClient();
    if (!client || client->endpoint().isEmpty()) {
        return {};
    }
    // Include API port and proxy prefix: different companions can share one host.
    const QByteArray hash = QCryptographicHash::hash(client->endpoint().toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("PixEagle/DashboardUrls/") + QString::fromLatin1(hash);
}

QString PixEagleManager::dashboardUrlOverride() const
{
    const QString key = _dashboardSettingsKey();
    const QString saved = key.isEmpty() ? QString() : QSettings().value(key).toString();
    return validDashboardUrl(QUrl(saved, QUrl::StrictMode)) ? saved : QString();
}

QUrl PixEagleManager::dashboardUrl() const
{
    const QString saved = dashboardUrlOverride();
    if (!saved.isEmpty()) {
        return QUrl(saved, QUrl::StrictMode);
    }
    const auto* client = activeClient();
    QUrl url(client ? client->endpoint() : QString());
    if (url.isEmpty()) {
        return {};
    }
    url.setPort(3040);
    url.setPath("/");
    return url;
}

bool PixEagleManager::setDashboardUrlOverride(const QString& address)
{
    const QString key = _dashboardSettingsKey();
    const QString text = address.trimmed();
    const QUrl url(text, QUrl::StrictMode);
    if (key.isEmpty() || (!text.isEmpty() && !validDashboardUrl(url))) {
        return false;
    }
    if (text.isEmpty()) {
        QSettings().remove(key);
    } else {
        QSettings().setValue(key, url.toString(QUrl::FullyEncoded));
    }
    emit dashboardUrlChanged();
    return true;
}

void PixEagleManager::initializeVideo()
{
    if (!_video) {
        _video = new PixEagleVideoController(this, _settings);
        emit videoChanged();
        _video->initialize();
    }
}

QStringList PixEagleManager::vehicleLabels() const
{
    QStringList labels;
    const auto* vehicles = MultiVehicleManager::instance()->vehicles();
    for (int index = 0; index < vehicles->count(); ++index) {
        const auto* vehicle = vehicles->value<Vehicle*>(index);
        if (vehicle) {
            labels.append(tr("Vehicle %1").arg(vehicle->id()));
        }
    }
    return labels;
}

QVariantList PixEagleManager::followingVehicles() const
{
    QVariantList result;
    const auto* vehicles = MultiVehicleManager::instance()->vehicles();
    for (int index = 0; index < vehicles->count(); ++index) {
        auto* vehicle = vehicles->value<Vehicle*>(index);
        const auto* client = _clients.value(vehicle);
        if (vehicle && client && (client->followingActive() || client->canStopFollowing()) && client->authenticated()) {
            result.append(QVariantMap{{"index", index},
                                      {"label", client->vehicleLabel()},
                                      {"stopAvailable", client->canStopFollowing()},
                                      {"stopToken", client->captureFollowingStop()}});
        }
    }
    return result;
}

int PixEagleManager::activeIndex() const
{
    auto* manager = MultiVehicleManager::instance();
    return manager->vehicles()->indexOf(manager->activeVehicle());
}

void PixEagleManager::selectVehicle(int index)
{
    auto* manager = MultiVehicleManager::instance();
    if (index >= 0 && index < manager->vehicles()->count()) {
        Vehicle* vehicle = manager->vehicles()->value<Vehicle*>(index);
        if (vehicle) {
            manager->setActiveVehicle(vehicle);
        }
    }
}

bool PixEagleManager::stopFollowingForVehicle(int index, const QString& token)
{
    auto* manager = MultiVehicleManager::instance();
    if (index < 0 || index >= manager->vehicles()->count()) {
        return false;
    }
    auto* vehicle = manager->vehicles()->value<Vehicle*>(index);
    auto* client = _clients.value(vehicle);
    return client && client->stopFollowing(token);
}
