#pragma once

#include "nvhttp.h"
#include "nvaddress.h"

#include <QThread>
#include <QReadWriteLock>
#include <QSettings>
#include <QRunnable>

class CopySafeReadWriteLock : public QReadWriteLock
{
public:
    CopySafeReadWriteLock() = default;

    // Don't actually copy the QReadWriteLock
    CopySafeReadWriteLock(const CopySafeReadWriteLock&) : QReadWriteLock() {}
    CopySafeReadWriteLock& operator=(const CopySafeReadWriteLock &) { return *this; }
};

class NvComputer
{
    friend class PcMonitorThread;
    friend class ComputerManager;
    friend class PendingQuitTask;

private:
    void sortAppList();

    bool updateAppList(QVector<NvApp> newAppList);

    bool pendingQuit;

public:
    NvComputer() = default;

    // Caller is responsible for synchronizing read access to the other host
    NvComputer(const NvComputer&) = default;

    // Caller is responsible for synchronizing read access to the other host
    NvComputer& operator=(const NvComputer &) = default;

    explicit NvComputer(NvHTTP& http, QString serverInfo);

    explicit NvComputer(QSettings& settings);

    void
    setRemoteAddress(QHostAddress);

    bool
    update(const NvComputer& that);

    bool
    wake() const;

    enum ReachabilityType
    {
        RI_UNKNOWN,
        RI_LAN,
        RI_VPN,
    };

    ReachabilityType
    getActiveAddressReachability() const;

    // Which known address to try first when connecting. This is a client-side
    // setting that persists across polls (like a custom name).
    enum PreferredAddressType
    {
        PA_AUTO,    // Default order: last working, local, remote, IPv6, manual
        PA_LOCAL,
        PA_REMOTE,
        PA_IPV6,
        PA_MANUAL,
        PA_MAX
    };

    // Returns a null address if there is no preference or that address is unknown
    NvAddress
    getPreferredAddress() const;

    QVector<NvAddress>
    uniqueAddresses() const;

    void
    serialize(QSettings& settings, bool serializeApps) const;

    // Caller is responsible for synchronizing read access to both hosts
    bool
    isEqualSerialized(const NvComputer& that) const;

    enum PairState
    {
        PS_UNKNOWN,
        PS_PAIRED,
        PS_NOT_PAIRED
    };

    enum ComputerState
    {
        CS_UNKNOWN,
        CS_ONLINE,
        CS_OFFLINE
    };

    // Ephemeral traits
    ComputerState state;
    PairState pairState;
    NvAddress activeAddress;
    uint16_t activeHttpsPort;
    int currentGameId;
    QString gfeVersion;
    QString appVersion;
    QVector<NvDisplayMode> displayModes;
    int maxLumaPixelsHEVC;
    int serverCodecModeSupport;
    QString gpuModel;
    bool isSupportedServerVersion;

    // Apollo host extensions (not persisted). apolloPermissions is -1 if the host
    // didn't report permissions (GFE, Sunshine, or an unauthenticated query).
    qint64 apolloPermissions = -1;
    bool virtualDisplayCapable = false;
    bool virtualDisplayDriverReady = false;

    // Apollo permission bits (see Apollo's src/crypto.h)
    enum ApolloPermission : quint32 {
        APOLLO_PERM_INPUT_CONTROLLER = 1u << 8,
        APOLLO_PERM_INPUT_TOUCH      = 1u << 9,
        APOLLO_PERM_INPUT_PEN        = 1u << 10,
        APOLLO_PERM_INPUT_MOUSE      = 1u << 11,
        APOLLO_PERM_INPUT_KEYBOARD   = 1u << 12,
        APOLLO_PERM_CLIPBOARD_SET    = 1u << 16,
        APOLLO_PERM_CLIPBOARD_READ   = 1u << 17,
        APOLLO_PERM_SERVER_CMD       = 1u << 20,
        APOLLO_PERM_LIST             = 1u << 24,
        APOLLO_PERM_VIEW             = 1u << 25,
        APOLLO_PERM_LAUNCH           = 1u << 26,
    };

    // True if the host reported Apollo permissions and the given one is missing
    bool isApolloPermissionMissing(quint32 permission) const
    {
        return apolloPermissions >= 0 && !((quint32)apolloPermissions & permission);
    }

    // Persisted traits
    NvAddress localAddress;
    NvAddress remoteAddress;
    NvAddress ipv6Address;
    NvAddress manualAddress;
    QByteArray macAddress;
    QString name;
    bool hasCustomName;
    PreferredAddressType preferredAddressType = PA_AUTO;
    QString uuid;
    QSslCertificate serverCert;
    QVector<NvApp> appList;
    bool isNvidiaServerSoftware;
    // Remember to update isEqualSerialized() when adding fields here!

    // Synchronization
    mutable CopySafeReadWriteLock lock;

private:
    uint16_t externalPort;
};
