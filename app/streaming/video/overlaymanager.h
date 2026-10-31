#pragma once

#include <QString>
#include <QReadWriteLock>

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

    void setDebugOverlayPosition(DebugOverlayPosition position);
    DebugOverlayPosition getDebugOverlayPosition();
    void cycleDebugOverlayPosition();

    void setOverlayRenderer(IOverlayRenderer* renderer);

private:
    void notifyOverlayUpdated(OverlayType type);
    SDL_Surface* RenderTextOutlinedWrapped(TTF_Font* font, const char* text, SDL_Color textColor, SDL_Color outlineColor, int outlineWidth, int wrapWidth);
    SDL_Surface* RenderTextPanel(TTF_Font* font, const char* text, SDL_Color textColor, SDL_Color panelColor);

    struct {
        bool enabled;
        int fontSize;
        SDL_Color color;
        char text[1024];

        TTF_Font* font;
        SDL_Surface* surface;
    } m_Overlays[OverlayMax];
    SDL_atomic_t m_DebugOverlayMode;
    SDL_atomic_t m_DebugOverlayPosition;
    IOverlayRenderer* m_Renderer;
    QByteArray m_FontData;
    QReadWriteLock m_OverlayRendererLock;
};

}
