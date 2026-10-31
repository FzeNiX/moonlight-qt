#pragma once

#include <QString>
#include <QReadWriteLock>
#include <QMutex>

#include "SDL_compat.h"
#include <SDL_ttf.h>

namespace Overlay {

enum OverlayType {
    OverlayDebug,
    OverlayStatusUpdate,
    OverlayMax
};

// Amount of detail shown in the performance stats overlay
enum DebugOverlayMode {
    DebugOverlayModeFull,
    DebugOverlayModeCompact,
    DebugOverlayModeMini,
    DebugOverlayModeMax
};

// Per-frame timing series shown in the stats overlay graph
enum FrameGraphSeries {
    FrameGraphNetwork,  // Interval between frames arriving from the network
    FrameGraphDisplay,  // Interval between frames being presented
    FrameGraphSeriesMax
};

// Horizontal screen edge the performance stats overlay is anchored to
enum DebugOverlayPosition {
    DebugOverlayPositionLeft,
    DebugOverlayPositionRight,
    DebugOverlayPositionMax
};

class IOverlayRenderer
{
public:
    virtual ~IOverlayRenderer() = default;

    virtual void notifyOverlayUpdated(OverlayType type) = 0;
};

class OverlayManager
{
public:
    OverlayManager();
    ~OverlayManager();

    bool isOverlayEnabled(OverlayType type);
    char* getOverlayText(OverlayType type);
    void updateOverlayText(OverlayType type, const char* text);
    int getOverlayMaxTextLength();
    void setOverlayTextUpdated(OverlayType type);
    void setOverlayState(OverlayType type, bool enabled);
    SDL_Color getOverlayColor(OverlayType type);
    int getOverlayFontSize(OverlayType type);
    SDL_Surface* getUpdatedOverlaySurface(OverlayType type);

    // Renderers use this to decide whether to anchor the overlay surface
    // to the right edge of the screen rather than the left edge.
    bool isOverlayRightAligned(OverlayType type);

    void setDebugOverlayMode(DebugOverlayMode mode);
    DebugOverlayMode getDebugOverlayMode();
    void cycleDebugOverlayMode();

    // The debug overlay shows the performance stats (if enabled by the user)
    // and/or short-lived notifications. isOverlayEnabled(OverlayDebug) reports
    // whether anything is visible, while this reports the user's stats toggle.
    bool isDebugStatsEnabled();

    // Shows text at the top of the debug overlay for a while, even if the
    // performance stats are hidden. May be called from any thread.
    void showDebugNotification(const QString& text, Uint32 durationMs);

    // The text actually displayed for an overlay (stats plus notifications),
    // for renderers that draw overlay text themselves.
    QString getOverlayDisplayText(OverlayType type);

    // Per-frame timing samples for the stats overlay graph. May be called from any thread.
    void addFrameGraphSample(FrameGraphSeries series, float intervalMs);
    void setFrameGraphTargetFps(int fps);

    void setDebugOverlayPosition(DebugOverlayPosition position);
    DebugOverlayPosition getDebugOverlayPosition();
    void cycleDebugOverlayPosition();

    void setOverlayRenderer(IOverlayRenderer* renderer);

private:
    void notifyOverlayUpdated(OverlayType type);
    SDL_Surface* RenderTextOutlinedWrapped(TTF_Font* font, const char* text, SDL_Color textColor, SDL_Color outlineColor, int outlineWidth, int wrapWidth);
    static const int k_FrameGraphSamples = 180;
    static const int k_FrameGraphHeight = 56;
    struct FrameGraphSnapshot {
        float samples[FrameGraphSeriesMax][k_FrameGraphSamples]; // Oldest first
        int count[FrameGraphSeriesMax];
        float targetMs;
    };

    SDL_Surface* RenderTextPanel(TTF_Font* font, const char* text, SDL_Color textColor, SDL_Color panelColor,
                                 const FrameGraphSnapshot* graph);
    void DrawFrameGraph(SDL_Surface* surface, TTF_Font* font, int x, int y, int width, const FrameGraphSnapshot& graph);
    bool takeFrameGraphSnapshot(FrameGraphSnapshot& snapshot);
    QByteArray composeDebugText();
    void updateDebugVisibility();
    static Uint32 notificationTimerCallback(Uint32 interval, void* param);

    struct {
        bool enabled;
        int fontSize;
        SDL_Color color;
        char text[1024];

        TTF_Font* font;
        SDL_Surface* surface;
    } m_Overlays[OverlayMax];
    // Debug overlay state
    bool m_DebugStatsEnabled;
    SDL_SpinLock m_NotificationLock;
    QString m_NotificationText;
    SDL_TimerID m_NotificationTimer;

    // Frame graph ring buffers (protected by m_GraphLock)
    SDL_SpinLock m_GraphLock;
    float m_GraphSamples[FrameGraphSeriesMax][k_FrameGraphSamples];
    int m_GraphNext[FrameGraphSeriesMax];
    int m_GraphCount[FrameGraphSeriesMax];
    int m_GraphTargetFps;

    // Serializes surface rendering, since TTF fonts aren't thread-safe and
    // overlays can be updated from the decoder, input, and timer threads.
    QMutex m_RenderMutex;

    SDL_atomic_t m_DebugOverlayMode;
    SDL_atomic_t m_DebugOverlayPosition;
    IOverlayRenderer* m_Renderer;
    QByteArray m_FontData;
    QReadWriteLock m_OverlayRendererLock;
};

}
