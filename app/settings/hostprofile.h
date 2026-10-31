#pragma once

#include <QString>
#include <QVariantMap>

class StreamingPreferences;

// Per-host stream settings ("host profiles").
//
// A profile is a set of overrides keyed by StreamingPreferences property
// names (e.g. "width", "bitrateKbps"). When a profile is enabled for a host,
// those values replace the global preferences for streams from that host.
// Profiles are stored separately from the global preferences and keyed by
// the host's UUID, so they survive host IP changes and renames.
namespace HostProfile
{
    bool isEnabled(const QString& hostUuid);

    // Returns only the stored overrides (may be empty)
    QVariantMap load(const QString& hostUuid);

    // Unknown keys are ignored
    void save(const QString& hostUuid, bool enabled, const QVariantMap& values);

    void remove(const QString& hostUuid);

    // Returns a new preferences object (owned by the caller) with the host's
    // overrides applied on top of the base preferences, or nullptr if the host
    // has no enabled profile.
    StreamingPreferences* createPreferencesForHost(const StreamingPreferences* base, const QString& hostUuid);
}
