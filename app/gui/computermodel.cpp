#include "computermodel.h"
#include "settings/hostprofile.h"

#include <QCoreApplication>
#include <QThreadPool>

ComputerModel::ComputerModel(QObject* object)
    : QAbstractListModel(object) {}

void ComputerModel::initialize(ComputerManager* computerManager)
{
    m_ComputerManager = computerManager;
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &ComputerModel::handleComputerStateChanged);
    connect(m_ComputerManager, &ComputerManager::pairingCompleted,
            this, &ComputerModel::handlePairingCompleted);

    m_Computers = m_ComputerManager->getComputers();
}

// Apollo-specific details (permissions and virtual display), if the host reported them
static QString apolloDetails(const NvComputer* computer)
{
    if (computer->apolloPermissions < 0 && !computer->virtualDisplayCapable) {
        return QString();
    }

    QString details;
    if (computer->apolloPermissions >= 0) {
        static const struct {
            quint32 bit;
            const char* name;
        } k_Permissions[] = {
            { NvComputer::APOLLO_PERM_LIST, QT_TRANSLATE_NOOP("ComputerModel", "List apps") },
            { NvComputer::APOLLO_PERM_VIEW, QT_TRANSLATE_NOOP("ComputerModel", "View streams") },
            { NvComputer::APOLLO_PERM_LAUNCH, QT_TRANSLATE_NOOP("ComputerModel", "Launch apps") },
            { NvComputer::APOLLO_PERM_INPUT_KEYBOARD, QT_TRANSLATE_NOOP("ComputerModel", "Keyboard") },
            { NvComputer::APOLLO_PERM_INPUT_MOUSE, QT_TRANSLATE_NOOP("ComputerModel", "Mouse") },
            { NvComputer::APOLLO_PERM_INPUT_CONTROLLER, QT_TRANSLATE_NOOP("ComputerModel", "Controller") },
            { NvComputer::APOLLO_PERM_INPUT_TOUCH, QT_TRANSLATE_NOOP("ComputerModel", "Touch") },
            { NvComputer::APOLLO_PERM_INPUT_PEN, QT_TRANSLATE_NOOP("ComputerModel", "Pen") },
            { NvComputer::APOLLO_PERM_CLIPBOARD_READ, QT_TRANSLATE_NOOP("ComputerModel", "Read clipboard") },
            { NvComputer::APOLLO_PERM_CLIPBOARD_SET, QT_TRANSLATE_NOOP("ComputerModel", "Set clipboard") },
            { NvComputer::APOLLO_PERM_SERVER_CMD, QT_TRANSLATE_NOOP("ComputerModel", "Server commands") },
        };

        QStringList granted, missing;
        for (const auto& permission : k_Permissions) {
            QString name = QCoreApplication::translate("ComputerModel", permission.name);
            if (computer->isApolloPermissionMissing(permission.bit)) {
                missing << name;
            }
            else {
                granted << name;
            }
        }

        details += '\n' + QCoreApplication::translate("ComputerModel", "Apollo permissions: %1").arg(granted.isEmpty() ? QCoreApplication::translate("ComputerModel", "None") : granted.join(", "));
        if (!missing.isEmpty()) {
            details += '\n' + QCoreApplication::translate("ComputerModel", "Not permitted: %1").arg(missing.join(", "));
        }
    }

    QString virtualDisplay;
    if (!computer->virtualDisplayCapable) {
        virtualDisplay = QCoreApplication::translate("ComputerModel", "Not supported");
    }
    else if (!computer->virtualDisplayDriverReady) {
        virtualDisplay = QCoreApplication::translate("ComputerModel", "Driver not ready");
    }
    else {
        virtualDisplay = QCoreApplication::translate("ComputerModel", "Available");
    }
    details += '\n' + QCoreApplication::translate("ComputerModel", "Virtual display: %1").arg(virtualDisplay);

    return details;
}

QVariant ComputerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    Q_ASSERT(index.row() < m_Computers.count());

    NvComputer* computer = m_Computers[index.row()];
    QReadLocker lock(&computer->lock);

    switch (role) {
    case NameRole:
        return computer->name;
    case OnlineRole:
        return computer->state == NvComputer::CS_ONLINE;
    case PairedRole:
        return computer->pairState == NvComputer::PS_PAIRED;
    case BusyRole:
        return computer->currentGameId != 0;
    case WakeableRole:
        return !computer->macAddress.isEmpty();
    case StatusUnknownRole:
        return computer->state == NvComputer::CS_UNKNOWN;
    case ServerSupportedRole:
        return computer->isSupportedServerVersion;
    case DetailsRole: {
        QString state, pairState;

        switch (computer->state) {
        case NvComputer::CS_ONLINE:
            state = tr("Online");
            break;
        case NvComputer::CS_OFFLINE:
            state = tr("Offline");
            break;
        default:
            state = tr("Unknown");
            break;
        }

        switch (computer->pairState) {
        case NvComputer::PS_PAIRED:
            pairState = tr("Paired");
            break;
        case NvComputer::PS_NOT_PAIRED:
            pairState = tr("Unpaired");
            break;
        default:
            pairState = tr("Unknown");
            break;
        }

        return tr("Name: %1").arg(computer->name) + '\n' +
               tr("Status: %1").arg(state) + '\n' +
               tr("Active Address: %1").arg(computer->activeAddress.toString()) + '\n' +
               tr("UUID: %1").arg(computer->uuid) + '\n' +
               tr("Local Address: %1").arg(computer->localAddress.toString()) + '\n' +
               tr("Remote Address: %1").arg(computer->remoteAddress.toString()) + '\n' +
               tr("IPv6 Address: %1").arg(computer->ipv6Address.toString()) + '\n' +
               tr("Manual Address: %1").arg(computer->manualAddress.toString()) + '\n' +
               tr("MAC Address: %1").arg(computer->macAddress.isEmpty() ? tr("Unknown") : QString(computer->macAddress.toHex(':'))) + '\n' +
               tr("Pair State: %1").arg(pairState) + '\n' +
               tr("Running Game ID: %1").arg(computer->state == NvComputer::CS_ONLINE ? QString::number(computer->currentGameId) : tr("Unknown")) + '\n' +
               tr("HTTPS Port: %1").arg(computer->state == NvComputer::CS_ONLINE ? QString::number(computer->activeHttpsPort) : tr("Unknown")) +
               apolloDetails(computer);
    }
    default:
        return QVariant();
    }
}

int ComputerModel::rowCount(const QModelIndex& parent) const
{
    // We should not return a count for valid index values,
    // only the parent (which will not have a "valid" index).
    if (parent.isValid()) {
        return 0;
    }

    return m_Computers.count();
}

QHash<int, QByteArray> ComputerModel::roleNames() const
{
    QHash<int, QByteArray> names;

    names[NameRole] = "name";
    names[OnlineRole] = "online";
    names[PairedRole] = "paired";
    names[BusyRole] = "busy";
    names[WakeableRole] = "wakeable";
    names[StatusUnknownRole] = "statusUnknown";
    names[ServerSupportedRole] = "serverSupported";
    names[DetailsRole] = "details";

    return names;
}

Session* ComputerModel::createSessionForCurrentGame(int computerIndex)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    NvComputer* computer = m_Computers[computerIndex];

    // We must currently be streaming a game to use this function
    Q_ASSERT(computer->currentGameId != 0);

    for (NvApp& app : computer->appList) {
        if (app.id == computer->currentGameId) {
            return new Session(computer, app);
        }
    }

    // We have a current running app but it's not in our app list
    Q_ASSERT(false);
    return nullptr;
}

void ComputerModel::deleteComputer(int computerIndex)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    // Drop this host's profile too, so a re-added host starts from the global settings
    {
        QReadLocker lock(&m_Computers[computerIndex]->lock);
        HostProfile::remove(m_Computers[computerIndex]->uuid);
    }

    beginRemoveRows(QModelIndex(), computerIndex, computerIndex);

    // m_Computer[computerIndex] will be deleted by this call
    m_ComputerManager->deleteHost(m_Computers[computerIndex]);

    // Remove the now invalid item
    m_Computers.removeAt(computerIndex);

    endRemoveRows();
}

class DeferredWakeHostTask : public QRunnable
{
public:
    DeferredWakeHostTask(NvComputer* computer)
        : m_Computer(computer) {}

    void run()
    {
        m_Computer->wake();
    }

private:
    NvComputer* m_Computer;
};

void ComputerModel::wakeComputer(int computerIndex)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    DeferredWakeHostTask* wakeTask = new DeferredWakeHostTask(m_Computers[computerIndex]);
    QThreadPool::globalInstance()->start(wakeTask);
}

void ComputerModel::renameComputer(int computerIndex, QString name)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    m_ComputerManager->renameHost(m_Computers[computerIndex], name);
}

QVariantMap ComputerModel::getHostSettings(int computerIndex)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    NvComputer* computer = m_Computers[computerIndex];
    QVariantMap result;
    QString uuid;

    {
        QReadLocker lock(&computer->lock);

        uuid = computer->uuid;
        result["name"] = computer->name;
        result["preferredAddress"] = (int)computer->preferredAddressType;
        result["activeAddress"] = computer->activeAddress.isNull() ? QString() : computer->activeAddress.toString();
        result["localAddress"] = computer->localAddress.isNull() ? QString() : computer->localAddress.toString();
        result["remoteAddress"] = computer->remoteAddress.isNull() ? QString() : computer->remoteAddress.toString();
        result["ipv6Address"] = computer->ipv6Address.isNull() ? QString() : computer->ipv6Address.toString();
        result["manualAddress"] = computer->manualAddress.isNull() ? QString() : computer->manualAddress.toString();

        // Apollo virtual display support, as last reported by the host
        if (computer->virtualDisplayCapable) {
            result["virtualDisplayStatus"] = computer->virtualDisplayDriverReady ?
                        tr("This host supports virtual displays.") :
                        tr("This host supports virtual displays, but its driver isn't ready right now.");
        }
        else if (computer->apolloPermissions >= 0) {
            result["virtualDisplayStatus"] = tr("This Apollo host can't create virtual displays (Windows hosts only).");
        }
        else {
            result["virtualDisplayStatus"] = tr("Requires an Apollo host.");
        }
    }

    result["profileEnabled"] = HostProfile::isEnabled(uuid);
    result["profile"] = HostProfile::load(uuid);
    return result;
}

void ComputerModel::setPreferredAddress(int computerIndex, int addressType)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    if (addressType < NvComputer::PA_AUTO || addressType >= NvComputer::PA_MAX) {
        Q_ASSERT(false);
        return;
    }

    m_ComputerManager->setPreferredAddress(m_Computers[computerIndex], (NvComputer::PreferredAddressType)addressType);
}

void ComputerModel::setHostProfile(int computerIndex, bool enabled, QVariantMap profile)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    QString uuid;
    {
        QReadLocker lock(&m_Computers[computerIndex]->lock);
        uuid = m_Computers[computerIndex]->uuid;
    }

    HostProfile::save(uuid, enabled, profile);
}

QString ComputerModel::generatePinString()
{
    return m_ComputerManager->generatePinString();
}

class DeferredTestConnectionTask : public QObject, public QRunnable
{
    Q_OBJECT
public:
    void run()
    {
        unsigned int portTestResult = LiTestClientConnectivity("qt.conntest.moonlight-stream.org", 443, ML_PORT_FLAG_ALL);
        if (portTestResult == ML_TEST_RESULT_INCONCLUSIVE) {
            emit connectionTestCompleted(-1, QString());
        }
        else {
            char blockedPorts[512];
            LiStringifyPortFlags(portTestResult, "\n", blockedPorts, sizeof(blockedPorts));
            emit connectionTestCompleted(portTestResult, QString(blockedPorts));
        }
    }

signals:
    void connectionTestCompleted(int result, QString blockedPorts);
};

void ComputerModel::testConnectionForComputer(int)
{
    DeferredTestConnectionTask* testConnectionTask = new DeferredTestConnectionTask();
    QObject::connect(testConnectionTask, &DeferredTestConnectionTask::connectionTestCompleted,
                     this, &ComputerModel::connectionTestCompleted);
    QThreadPool::globalInstance()->start(testConnectionTask);
}

void ComputerModel::pairComputer(int computerIndex, QString pin)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    m_ComputerManager->pairHost(m_Computers[computerIndex], pin);
}

void ComputerModel::handlePairingCompleted(NvComputer*, QString error)
{
    emit pairingCompleted(error.isEmpty() ? QVariant() : error);
}

void ComputerModel::handleComputerStateChanged(NvComputer* computer)
{
    QVector<NvComputer*> newComputerList = m_ComputerManager->getComputers();

    // Reset the model if the structural layout of the list has changed
    if (m_Computers != newComputerList) {
        beginResetModel();
        m_Computers = newComputerList;
        endResetModel();
    }
    else {
        // Let the view know that this specific computer changed
        int index = m_Computers.indexOf(computer);
        emit dataChanged(createIndex(index, 0), createIndex(index, 0));
    }
}

#include "computermodel.moc"
