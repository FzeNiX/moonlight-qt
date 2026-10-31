#include "overlaymanager.h"
#include "path.h"

using namespace Overlay;

OverlayManager::OverlayManager() :
    m_Renderer(nullptr),
    m_FontData(Path::readDataFile("ModeSeven.ttf"))
{
    memset(m_Overlays, 0, sizeof(m_Overlays));

    // Near-white text on a translucent dark panel stays readable on both
    // bright and dark game content (unlike the old unpadded yellow text).
    m_Overlays[OverlayType::OverlayDebug].color = {0xF0, 0xF0, 0xF0, 0xFF};
    m_Overlays[OverlayType::OverlayDebug].fontSize = 20;

    SDL_AtomicSet(&m_DebugOverlayMode, DebugOverlayModeFull);
    SDL_AtomicSet(&m_DebugOverlayPosition, DebugOverlayPositionLeft);

    m_Overlays[OverlayType::OverlayStatusUpdate].color = {0xCC, 0x00, 0x00, 0xFF};
    m_Overlays[OverlayType::OverlayStatusUpdate].fontSize = 36;

    // While TTF will usually not be initialized here, it is valid for that not to
    // be the case, since Session destruction is deferred and could overlap with
    // the lifetime of a new Session object.
    //SDL_assert(TTF_WasInit() == 0);

    if (TTF_Init() != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "TTF_Init() failed: %s",
                    TTF_GetError());
        return;
    }
}

OverlayManager::~OverlayManager()
{
    for (int i = 0; i < OverlayType::OverlayMax; i++) {
        if (m_Overlays[i].surface != nullptr) {
            SDL_FreeSurface(m_Overlays[i].surface);
        }
        if (m_Overlays[i].font != nullptr) {
            TTF_CloseFont(m_Overlays[i].font);
        }
    }

    TTF_Quit();

    // For similar reasons to the comment in the constructor, this will usually,
    // but not always, deinitialize TTF. In the cases where Session objects overlap
    // in lifetime, there may be an additional reference on TTF for the new Session
    // that means it will not be cleaned up here.
    //SDL_assert(TTF_WasInit() == 0);
}

bool OverlayManager::isOverlayEnabled(OverlayType type)
{
    return m_Overlays[type].enabled;
}

char* OverlayManager::getOverlayText(OverlayType type)
{
    return m_Overlays[type].text;
}

void OverlayManager::updateOverlayText(OverlayType type, const char* text)
{
    SDL_utf8strlcpy(m_Overlays[type].text, text, sizeof(m_Overlays[0].text));
    setOverlayTextUpdated(type);
}

int OverlayManager::getOverlayMaxTextLength()
{
    return sizeof(m_Overlays[0].text);
}

int OverlayManager::getOverlayFontSize(OverlayType type)
{
    return m_Overlays[type].fontSize;
}

SDL_Surface* OverlayManager::getUpdatedOverlaySurface(OverlayType type)
{
    // If a new surface is available, return it. If not, return nullptr.
    // Caller must free the surface on success.
    return (SDL_Surface*)SDL_AtomicSetPtr((void**)&m_Overlays[type].surface, nullptr);
}

bool OverlayManager::isOverlayRightAligned(OverlayType type)
{
    switch (type) {
    case OverlayDebug:
        return getDebugOverlayPosition() == DebugOverlayPositionRight;
    default:
        return false;
    }
}

void OverlayManager::setDebugOverlayMode(DebugOverlayMode mode)
{
    if (mode < 0 || mode >= DebugOverlayModeMax) {
        SDL_assert(false);
        mode = DebugOverlayModeFull;
    }

    // The new text layout is picked up on the next stats refresh
    SDL_AtomicSet(&m_DebugOverlayMode, mode);
}

DebugOverlayMode OverlayManager::getDebugOverlayMode()
{
    return (DebugOverlayMode)SDL_AtomicGet(&m_DebugOverlayMode);
}

void OverlayManager::cycleDebugOverlayMode()
{
    setDebugOverlayMode((DebugOverlayMode)((getDebugOverlayMode() + 1) % DebugOverlayModeMax));
}

void OverlayManager::setDebugOverlayPosition(DebugOverlayPosition position)
{
    if (position < 0 || position >= DebugOverlayPositionMax) {
        SDL_assert(false);
        position = DebugOverlayPositionLeft;
    }

    if (SDL_AtomicSet(&m_DebugOverlayPosition, position) != position) {
        // Re-render so the renderer can recompute the overlay placement
        setOverlayTextUpdated(OverlayDebug);
    }
}

DebugOverlayPosition OverlayManager::getDebugOverlayPosition()
{
    return (DebugOverlayPosition)SDL_AtomicGet(&m_DebugOverlayPosition);
}

void OverlayManager::cycleDebugOverlayPosition()
{
    setDebugOverlayPosition((DebugOverlayPosition)((getDebugOverlayPosition() + 1) % DebugOverlayPositionMax));
}

void OverlayManager::setOverlayTextUpdated(OverlayType type)
{
    // Only update the overlay state if it's enabled. If it's not enabled,
    // the renderer has already been notified by setOverlayState().
    if (m_Overlays[type].enabled) {
        notifyOverlayUpdated(type);
    }
}

void OverlayManager::setOverlayState(OverlayType type, bool enabled)
{
    bool stateChanged = m_Overlays[type].enabled != enabled;

    m_Overlays[type].enabled = enabled;

    if (stateChanged) {
        if (!enabled) {
            // Set the text to empty string on disable
            m_Overlays[type].text[0] = 0;
        }

        notifyOverlayUpdated(type);
    }
}

SDL_Color OverlayManager::getOverlayColor(OverlayType type)
{
    return m_Overlays[type].color;
}

void OverlayManager::setOverlayRenderer(IOverlayRenderer* renderer)
{
    QWriteLocker lock(&m_OverlayRendererLock);
    m_Renderer = renderer;
}

void OverlayManager::notifyOverlayUpdated(OverlayType type)
{
    QReadLocker lock(&m_OverlayRendererLock);

    if (m_Renderer == nullptr) {
        return;
    }

    // Construct the required font to render the overlay
    if (m_Overlays[type].font == nullptr) {
        if (m_FontData.isEmpty()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL overlay font failed to load");
            return;
        }

        // m_FontData must stay around until the font is closed
        m_Overlays[type].font = TTF_OpenFontRW(SDL_RWFromConstMem(m_FontData.constData(), m_FontData.size()),
                                               1,
                                               m_Overlays[type].fontSize);
        if (m_Overlays[type].font == nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "TTF_OpenFont() failed: %s",
                        TTF_GetError());

            // Can't proceed without a font
            return;
        }
    }

    SDL_Surface* newSurface = nullptr;
    if (m_Overlays[type].enabled) {
        if (type == OverlayDebug) {
            // Stats are drawn on a translucent panel for legibility
            newSurface = RenderTextPanel(m_Overlays[type].font,
                                         m_Overlays[type].text,
                                         m_Overlays[type].color,
                                         {0x10, 0x10, 0x14, 0xB8});
        }
        else {
            // The _Wrapped variant is required for line breaks to work
            newSurface = RenderTextOutlinedWrapped(m_Overlays[type].font,
                                                   m_Overlays[type].text,
                                                   m_Overlays[type].color,
                                                   {0, 0, 0, 255},
                                                   4,
                                                   1024);
        }
    }

    // Exchange the old surface with the new one
    SDL_Surface* oldSurface = (SDL_Surface*)SDL_AtomicSetPtr((void**)&m_Overlays[type].surface, newSurface);

    // Notify the renderer
    m_Renderer->notifyOverlayUpdated(type);

    // Free the old surface
    if (oldSurface != nullptr) {
        SDL_FreeSurface(oldSurface);
    }
}

SDL_Surface* OverlayManager::RenderTextOutlinedWrapped(TTF_Font* font, const char* text, SDL_Color textColor, SDL_Color outlineColor, int outlineWidth, int wrapWidth) {
    if (text == nullptr || text[0] == '\0') {
        return nullptr;
    }

    int oldOutline = TTF_GetFontOutline(font);
    TTF_SetFontOutline(font, outlineWidth);

    // Verify that the string won't require wrapping (which could cause the outline and the text
    // to diverge due to different wrapping positions).
    //
    // FIXME: We do this rather than just disabling wrapping entirely (wrapWidth = 0) because we
    // need further testing to ensure that all renderers can handle non-NPOT overlay textures.
    for (const QString& line : QString(text).split('\n')) {
        int extent, count;
        if (TTF_MeasureUTF8(font, line.toUtf8(), wrapWidth, &extent, &count) == 0 && count < line.size()) {
            // If it requires wrapping, render it without the outline
            TTF_SetFontOutline(font, oldOutline);
            return TTF_RenderUTF8_Blended_Wrapped(font, text, textColor, wrapWidth);
        }
    }

    // Draw text twice, but outline is a bit bigger
    auto outlineSurface = TTF_RenderUTF8_Blended_Wrapped(font, text, outlineColor, wrapWidth);
    TTF_SetFontOutline(font, 0);
    auto textSurface = TTF_RenderUTF8_Blended_Wrapped(font, text, textColor, wrapWidth);
    TTF_SetFontOutline(font, oldOutline);

    if (outlineSurface == nullptr || textSurface == nullptr) {
        SDL_FreeSurface(outlineSurface);
        SDL_FreeSurface(textSurface);
        return nullptr;
    }

    // Merge the texts
    SDL_Rect dst = { outlineWidth, outlineWidth, textSurface->w, textSurface->h };
    SDL_BlitSurface(textSurface, nullptr, outlineSurface, &dst);

    SDL_FreeSurface(textSurface);
    return outlineSurface;
}



SDL_Surface* OverlayManager::RenderTextPanel(TTF_Font* font, const char* text, SDL_Color textColor, SDL_Color panelColor)
{
    // Spacing (in pixels) between the screen edge and the panel,
    // the panel edge and the text, and the panel corner radius.
    const int margin = 8;
    const int padX = 10;
    const int padY = 6;
    const int radius = 6;

    // Trailing line breaks would just add empty space at the bottom of the panel
    QByteArray trimmedText(text);
    while (trimmedText.endsWith('\n')) {
        trimmedText.chop(1);
    }

    const int outlineWidth = 1;
    const int maxWrapWidth = 1024;

    // Some SDL_ttf versions size wrapped text surfaces to the wrap width rather
    // than the longest line, so measure the longest line to keep the panel tight.
    int contentWidth = 0;
    int slackWidth = 0;
    int oldOutline = TTF_GetFontOutline(font);
    TTF_SetFontOutline(font, outlineWidth);
    for (const QByteArray& line : trimmedText.split('\n')) {
        int lineWidth;
        if (TTF_SizeUTF8(font, line.constData(), &lineWidth, nullptr) == 0) {
            contentWidth = SDL_max(contentWidth, lineWidth);
        }
    }
    // SDL_ttf's wrapping can break a line that exactly fits the wrap width,
    // so leave a couple of characters of slack to ensure it only breaks on '\n'.
    TTF_SizeUTF8(font, "MM", &slackWidth, nullptr);
    TTF_SetFontOutline(font, oldOutline);

    int wrapWidth = (contentWidth > 0) ? SDL_min(contentWidth + slackWidth, maxWrapWidth) : maxWrapWidth;

    // A thin outline keeps glyph edges crisp against the panel
    SDL_Surface* textSurface = RenderTextOutlinedWrapped(font, trimmedText.constData(), textColor, {0, 0, 0, 0xFF}, outlineWidth, wrapWidth);
    if (textSurface == nullptr) {
        return nullptr;
    }

    // Text is left-aligned, so anything past the longest line is empty
    if (contentWidth <= 0 || contentWidth + 2 * outlineWidth > textSurface->w) {
        contentWidth = textSurface->w;
    }
    else {
        contentWidth += 2 * outlineWidth;
    }

    int panelW = contentWidth + 2 * padX;
    int panelH = textSurface->h + 2 * padY;

    // The surface includes a transparent margin, so renderers can keep
    // placing it flush against the screen edge.
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0,
                                                          panelW + 2 * margin,
                                                          panelH + 2 * margin,
                                                          32,
                                                          SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr) {
        SDL_FreeSurface(textSurface);
        return nullptr;
    }

    SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 0, 0, 0, 0));

    if (SDL_LockSurface(surface) == 0) {
        for (int y = 0; y < panelH; y++) {
            Uint32* row = (Uint32*)((Uint8*)surface->pixels + (y + margin) * surface->pitch) + margin;

            for (int x = 0; x < panelW; x++) {
                // Distance into the rounded corner region (0 outside of corners)
                float dx = SDL_max(0, SDL_max(radius - x, x - (panelW - 1 - radius)));
                float dy = SDL_max(0, SDL_max(radius - y, y - (panelH - 1 - radius)));
                float coverage = 1.0f;

                if (dx > 0 && dy > 0) {
                    // Anti-aliased rounded corner
                    coverage = SDL_min(1.0f, SDL_max(0.0f, radius + 0.5f - SDL_sqrtf(dx * dx + dy * dy)));
                }

                if (coverage > 0.0f) {
                    row[x] = SDL_MapRGBA(surface->format,
                                         panelColor.r, panelColor.g, panelColor.b,
                                         (Uint8)(panelColor.a * coverage));
                }
            }
        }

        SDL_UnlockSurface(surface);
    }

    SDL_SetSurfaceBlendMode(textSurface, SDL_BLENDMODE_BLEND);
    SDL_Rect src = { 0, 0, contentWidth, textSurface->h };
    SDL_Rect dst = { margin + padX, margin + padY, contentWidth, textSurface->h };
    SDL_BlitSurface(textSurface, &src, surface, &dst);
    SDL_FreeSurface(textSurface);

    return surface;
}
