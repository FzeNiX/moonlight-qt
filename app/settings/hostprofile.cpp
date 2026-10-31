#include "hostprofile.h"
#include "streamingpreferences.h"

#include <QSettings>
#include <QSet>
#include <QMetaProperty>
#include <QtDebug>

#define SER_HOSTPROFILES "hostprofiles"
#define SER_ENABLED "enabled"
#define SER_VALUES "values"

// Only stream-related preferences may be overridden per host
static const QSet<QString> k_AllowedKeys = {
    "width", "height", "fps", "bitrateKbps",
    "windowMode", "enableVsync", "framePacing",
    "enableHdr", "enableYUV444", "videoCodecConfig",
    "audioConfig", "playAudioOnHost", "gameOptimizations", "quitAppAfter",
    "absoluteMouseMode", "controllerTypeOverride",
    "videoSharpening", "videoSharpness",
    "colorBrightness", "colorContrast", "colorSaturation",
    "showPerformanceOverlay", "performanceOverlayMode", "performanceOverlayPosition",
};

static QString groupForHost(const QString& hostUuid)
{
    return QString(SER_HOSTPROFILES "/%1").arg(hostUuid);
}

bool HostProfile::isEnabled(const QString& hostUuid)
{
    if (hostUuid.isEmpty()) {
        return false;
    }

    QSettings settings;
    settings.beginGroup(groupForHost(hostUuid));
    return settings.value(SER_ENABLED, false).toBool();
}

QVariantMap HostProfile::load(const QString& hostUuid)
{
    QVariantMap values;
    if (hostUuid.isEmpty()) {
        return values;
    }

    QSettings settings;
    settings.beginGroup(groupForHost(hostUuid));
    settings.beginGroup(SER_VALUES);
    for (const QString& key : settings.childKeys()) {
        if (k_AllowedKeys.contains(key)) {
            values.insert(key, settings.value(key));
        }
    }
    return values;
}

void HostProfile::save(const QString& hostUuid, bool enabled, const QVariantMap& values)
{
    if (hostUuid.isEmpty()) {
        return;
    }

    QSettings settings;
    settings.beginGroup(groupForHost(hostUuid));
    settings.setValue(SER_ENABLED, enabled);

    // Replace the stored overrides entirely
    settings.remove(SER_VALUES);
    settings.beginGroup(SER_VALUES);
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (k_AllowedKeys.contains(it.key())) {
            settings.setValue(it.key(), it.value());
        }
        else {
            qWarning() << "Ignoring unsupported host profile key:" << it.key();
        }
    }
}

void HostProfile::remove(const QString& hostUuid)
{
    if (hostUuid.isEmpty()) {
        return;
    }

    QSettings settings;
    settings.remove(groupForHost(hostUuid));
}

StreamingPreferences* HostProfile::createPreferencesForHost(const StreamingPreferences* base, const QString& hostUuid)
{
    if (!isEnabled(hostUuid)) {
        return nullptr;
    }

    QVariantMap values = load(hostUuid);
    StreamingPreferences* prefs = StreamingPreferences::createCopy(base);

    const QMetaObject* mo = prefs->metaObject();
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        int index = mo->indexOfProperty(it.key().toUtf8().constData());
        if (index < 0) {
            qWarning() << "Unknown host profile key:" << it.key();
            continue;
        }

        // INI-backed settings come back as strings, so convert explicitly.
        // Enums are stored as their integer values.
        QMetaProperty prop = mo->property(index);
        QVariant value = it.value();
        bool ok = true;
        if (prop.isEnumType() || prop.userType() == QMetaType::Int) {
            // Accept "2", "2.0", and numeric types alike
            value = qRound(value.toDouble(&ok));
        }
        else if (prop.userType() == QMetaType::Bool) {
            value = (value.toString() == "true" || value.toInt() != 0);
        }

        if (!ok || !prop.write(prefs, value)) {
            qWarning() << "Failed to apply host profile value:" << it.key() << it.value();
        }
    }

    // A profile that sets the stream format without a bitrate uses the default
    // bitrate for that resolution and frame rate ("Automatic" in the UI).
    bool formatOverridden = values.contains("width") || values.contains("height") ||
                            values.contains("fps") || values.contains("enableYUV444");
    if (formatOverridden && !values.contains("bitrateKbps")) {
        prefs->bitrateKbps = StreamingPreferences::getDefaultBitrate(prefs->width, prefs->height,
                                                                     prefs->fps, prefs->enableYUV444);
    }

    qInfo() << "Applied host profile with" << values.count() << "overrides";
    return prefs;
}
