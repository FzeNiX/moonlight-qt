#include "overlaymanager.h"
#include "path.h"

using namespace Overlay;

// Menu metrics
static const int k_MenuTrackWidth = 240;
static const int k_MenuTrackHeight = 6;
static const int k_MenuKnobWidth = 10;
static const int k_MenuRowPadding = 10;
static const int k_MenuColumnGap = 14;
static const int k_MenuButtonPadX = 8;


OverlayManager::OverlayManager() :
    m_DebugStatsEnabled(false),
    m_NotificationLock(0),
    m_NotificationTimer(0),
    m_GraphLock(0),
    m_GraphTargetFps(60),
    m_MenuOpen(false),
    m_Renderer(nullptr),
    m_FontData(Path::readDataFile("ModeSeven.ttf"))
{
    memset(m_Overlays, 0, sizeof(m_Overlays));
    memset(m_GraphSamples, 0, sizeof(m_GraphSamples));
    memset(m_GraphNext, 0, sizeof(m_GraphNext));
    memset(m_GraphCount, 0, sizeof(m_GraphCount));
    m_MenuLayout.valid = false;

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
    if (m_NotificationTimer != 0) {
        SDL_RemoveTimer(m_NotificationTimer);
    }

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

bool OverlayManager::isDebugStatsEnabled()
{
    return m_DebugStatsEnabled;
}

void OverlayManager::updateDebugVisibility()
{
    SDL_AtomicLock(&m_NotificationLock);
    bool hasNotification = !m_NotificationText.isEmpty();
    SDL_AtomicUnlock(&m_NotificationLock);

    m_Overlays[OverlayDebug].enabled = m_DebugStatsEnabled || hasNotification || m_MenuOpen;

    // Always re-render since the content (if not the visibility) has changed
    notifyOverlayUpdated(OverlayDebug);
}

Uint32 OverlayManager::notificationTimerCallback(Uint32, void* param)
{
    auto me = (OverlayManager*)param;

    SDL_AtomicLock(&me->m_NotificationLock);
    me->m_NotificationText.clear();
    me->m_NotificationTimer = 0;
    SDL_AtomicUnlock(&me->m_NotificationLock);

    me->updateDebugVisibility();

    // One-shot timer
    return 0;
}

void OverlayManager::showDebugNotification(const QString& text, Uint32 durationMs)
{
    SDL_AtomicLock(&m_NotificationLock);
    m_NotificationText = text;
    SDL_TimerID oldTimer = m_NotificationTimer;
    m_NotificationTimer = 0;
    SDL_AtomicUnlock(&m_NotificationLock);

    // Restart the hide timer (outside the lock, since the callback takes it)
    if (oldTimer != 0) {
        SDL_RemoveTimer(oldTimer);
    }
    SDL_TimerID newTimer = SDL_AddTimer(durationMs, notificationTimerCallback, this);

    SDL_AtomicLock(&m_NotificationLock);
    if (m_NotificationText == text) {
        m_NotificationTimer = newTimer;
    }
    SDL_AtomicUnlock(&m_NotificationLock);

    updateDebugVisibility();
}

QByteArray OverlayManager::composeDebugText(bool includeMenuText)
{
    SDL_AtomicLock(&m_NotificationLock);
    QByteArray composed = m_NotificationText.toUtf8();
    SDL_AtomicUnlock(&m_NotificationLock);

    // Plain text version of the menu for renderers that only draw text
    if (includeMenuText && m_MenuOpen) {
        QMutexLocker lock(&m_RenderMutex);
        QString menuText = m_Menu.title;
        for (int i = 0; i < m_Menu.items.count(); i++) {
            menuText += QString("\n%1 %2 %3").arg(i == m_Menu.selected ? ">" : " ")
                                             .arg(m_Menu.items[i].label, -10)
                                             .arg(m_Menu.items[i].valueText);
        }
        QByteArray menuBytes = menuText.toUtf8();
        if (!composed.isEmpty()) {
            menuBytes += "\n\n";
            menuBytes += composed;
        }
        composed = menuBytes;
    }

    if (m_DebugStatsEnabled && m_Overlays[OverlayDebug].text[0] != 0) {
        if (!composed.isEmpty()) {
            composed += "\n\n";
        }
        composed += m_Overlays[OverlayDebug].text;
    }

    return composed;
}

QString OverlayManager::getOverlayDisplayText(OverlayType type)
{
    if (type == OverlayDebug) {
        return QString::fromUtf8(composeDebugText(true));
    }
    return QString::fromUtf8(m_Overlays[type].text);
}

void OverlayManager::setDebugMenu(const OverlayMenu& menu)
{
    {
        QMutexLocker lock(&m_RenderMutex);
        m_Menu = menu;
        m_MenuOpen = true;
    }
    updateDebugVisibility();
}

void OverlayManager::closeDebugMenu()
{
    {
        QMutexLocker lock(&m_RenderMutex);
        m_MenuOpen = false;
        m_MenuLayout.valid = false;
    }
    updateDebugVisibility();
}

bool OverlayManager::isDebugMenuOpen()
{
    return m_MenuOpen;
}

QSize OverlayManager::getDebugOverlaySurfaceSize()
{
    QMutexLocker lock(&m_RenderMutex);
    return m_DebugSurfaceSize;
}

OverlayMenuHit OverlayManager::hitTestDebugMenu(int x, int y)
{
    QMutexLocker lock(&m_RenderMutex);
    OverlayMenuHit hit = { OverlayMenuHit::None, -1, 0.0f };

    if (!m_MenuOpen || !m_MenuLayout.valid || !m_MenuLayout.panel.contains(x, y)) {
        return hit;
    }

    hit.kind = OverlayMenuHit::Panel;
    if (m_MenuLayout.close.contains(x, y)) {
        hit.kind = OverlayMenuHit::Close;
    }
    else if (m_MenuLayout.reset.contains(x, y)) {
        hit.kind = OverlayMenuHit::Reset;
    }
    else {
        for (int i = 0; i < m_MenuLayout.rows.count(); i++) {
            if (m_MenuLayout.tracks[i].contains(x, y)) {
                hit.kind = OverlayMenuHit::Track;
                hit.index = i;
                break;
            }
            else if (m_MenuLayout.rows[i].contains(x, y)) {
                hit.kind = OverlayMenuHit::Item;
                hit.index = i;
                break;
            }
        }
    }

    if (hit.kind == OverlayMenuHit::Track) {
        lock.unlock();
        hit.fraction = getDebugMenuTrackFraction(hit.index, x);
    }
    return hit;
}

float OverlayManager::getDebugMenuTrackFraction(int index, int x)
{
    QMutexLocker lock(&m_RenderMutex);
    if (!m_MenuLayout.valid || index < 0 || index >= m_MenuLayout.tracks.count()) {
        return 0.0f;
    }

    // The hit area extends half a knob past each end of the drawn track
    const QRect& area = m_MenuLayout.tracks[index];
    int trackStart = area.x() + k_MenuKnobWidth / 2;
    int trackWidth = area.width() - k_MenuKnobWidth;
    return SDL_min(1.0f, SDL_max(0.0f, (float)(x - trackStart) / (trackWidth - 1)));
}

void OverlayManager::addFrameGraphSample(FrameGraphSeries series, float intervalMs)
{
    SDL_AtomicLock(&m_GraphLock);
    m_GraphSamples[series][m_GraphNext[series]] = intervalMs;
    m_GraphNext[series] = (m_GraphNext[series] + 1) % k_FrameGraphSamples;
    m_GraphCount[series] = SDL_min(m_GraphCount[series] + 1, k_FrameGraphSamples);
    SDL_AtomicUnlock(&m_GraphLock);
}

void OverlayManager::setFrameGraphTargetFps(int fps)
{
    SDL_AtomicLock(&m_GraphLock);
    m_GraphTargetFps = fps > 0 ? fps : 60;
    SDL_AtomicUnlock(&m_GraphLock);
}

bool OverlayManager::takeFrameGraphSnapshot(FrameGraphSnapshot& snapshot)
{
    bool hasSamples = false;

    SDL_AtomicLock(&m_GraphLock);
    for (int s = 0; s < FrameGraphSeriesMax; s++) {
        int count = m_GraphCount[s];
        int start = (m_GraphNext[s] - count + k_FrameGraphSamples) % k_FrameGraphSamples;
        for (int i = 0; i < count; i++) {
            snapshot.samples[s][i] = m_GraphSamples[s][(start + i) % k_FrameGraphSamples];
        }
        snapshot.count[s] = count;
        hasSamples = hasSamples || count > 1;
    }
    snapshot.targetMs = 1000.0f / m_GraphTargetFps;
    SDL_AtomicUnlock(&m_GraphLock);

    return hasSamples;
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
    if (type == OverlayDebug) {
        // This is the user's stats toggle. The overlay may stay visible
        // without stats while a notification is being shown.
        m_DebugStatsEnabled = enabled;
        if (!enabled) {
            m_Overlays[type].text[0] = 0;
        }
        updateDebugVisibility();
        return;
    }

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

    // Overlays may be updated concurrently from several threads, but TTF fonts
    // are not thread-safe, so rendering must be serialized.
    QMutexLocker renderLock(&m_RenderMutex);

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
            // The frame time graph accompanies the Full and Compact stats layouts
            FrameGraphSnapshot graph;
            bool showGraph = m_DebugStatsEnabled &&
                             m_Overlays[type].text[0] != 0 &&
                             getDebugOverlayMode() != DebugOverlayModeMini &&
                             takeFrameGraphSnapshot(graph);

            // Stats, notifications, and the menu are drawn on a translucent panel for legibility
            QByteArray text = composeDebugText(false);
            m_MenuLayout.valid = false;
            newSurface = RenderTextPanel(m_Overlays[type].font,
                                         text.constData(),
                                         m_Overlays[type].color,
                                         {0x10, 0x10, 0x14, 0xB8},
                                         showGraph ? &graph : nullptr,
                                         m_MenuOpen ? &m_Menu : nullptr);
            m_DebugSurfaceSize = newSurface ? QSize(newSurface->w, newSurface->h) : QSize();
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



SDL_Surface* OverlayManager::RenderTextPanel(TTF_Font* font, const char* text, SDL_Color textColor, SDL_Color panelColor,
                                             const FrameGraphSnapshot* graph, const OverlayMenu* menu)
{
    // Spacing (in pixels) between the screen edge and the panel,
    // the panel edge and the content, and the panel corner radius.
    const int margin = 8;
    const int padX = 10;
    const int padY = 6;
    const int radius = 6;
    const int sectionGap = 8;

    // Trailing line breaks would just add empty space at the bottom of the panel
    QByteArray trimmedText(text);
    while (trimmedText.endsWith('\n')) {
        trimmedText.chop(1);
    }

    const int outlineWidth = 1;
    const int maxWrapWidth = 1024;

    // The panel stacks up to three sections: menu, text, and graph
    SDL_Surface* textSurface = nullptr;
    int contentWidth = 0;
    if (!trimmedText.isEmpty()) {
        // Some SDL_ttf versions size wrapped text surfaces to the wrap width rather
        // than the longest line, so measure the longest line to keep the panel tight.
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
        textSurface = RenderTextOutlinedWrapped(font, trimmedText.constData(), textColor, {0, 0, 0, 0xFF}, outlineWidth, wrapWidth);

        // Text is left-aligned, so anything past the longest line is empty
        if (textSurface != nullptr) {
            if (contentWidth <= 0 || contentWidth + 2 * outlineWidth > textSurface->w) {
                contentWidth = textSurface->w;
            }
            else {
                contentWidth += 2 * outlineWidth;
            }
        }
        else {
            contentWidth = 0;
        }
    }

    QSize menuSize = menu ? MeasureMenu(font, *menu) : QSize();
    const int graphWidth = k_FrameGraphSamples * 2;
    const int graphHeight = graph ? (TTF_FontHeight(font) + 4 + k_FrameGraphHeight) : 0;
    const int textHeight = textSurface ? textSurface->h : 0;

    if (!menu && textSurface == nullptr) {
        // Nothing to show
        return nullptr;
    }

    int innerWidth = SDL_max(SDL_max(contentWidth, menu ? menuSize.width() : 0), graph ? graphWidth : 0);
    int innerHeight = 0;
    int menuY = 0, textY = 0, graphY = 0;
    if (menu) {
        menuY = innerHeight;
        innerHeight += menuSize.height();
    }
    if (textSurface) {
        innerHeight += innerHeight > 0 ? sectionGap : 0;
        textY = innerHeight;
        innerHeight += textHeight;
    }
    if (graph && textSurface) {
        innerHeight += sectionGap;
        graphY = innerHeight;
        innerHeight += graphHeight;
    }

    int panelW = innerWidth + 2 * padX;
    int panelH = innerHeight + 2 * padY;

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

    int originX = margin + padX;
    int originY = margin + padY;

    if (menu) {
        MenuLayout layout = {};
        DrawMenu(surface, font, originX, originY + menuY, innerWidth, *menu, layout);
        layout.panel = QRect(margin, margin, panelW, panelH);
        layout.valid = true;
        m_MenuLayout = layout;
    }

    if (textSurface) {
        SDL_SetSurfaceBlendMode(textSurface, SDL_BLENDMODE_BLEND);
        SDL_Rect src = { 0, 0, contentWidth, textSurface->h };
        SDL_Rect dst = { originX, originY + textY, contentWidth, textSurface->h };
        SDL_BlitSurface(textSurface, &src, surface, &dst);
        SDL_FreeSurface(textSurface);

        if (graph) {
            DrawFrameGraph(surface, font, originX, originY + graphY, graphWidth, *graph);
        }
    }

    return surface;
}

QSize OverlayManager::MeasureMenu(TTF_Font* font, const OverlayMenu& menu)
{
    int lineH = TTF_FontHeight(font);
    int labelW = 0, valueW = 0, w;

    for (const OverlayMenuItem& item : menu.items) {
        if (TTF_SizeUTF8(font, item.label.toUtf8().constData(), &w, nullptr) == 0) {
            labelW = SDL_max(labelW, w);
        }
        if (TTF_SizeUTF8(font, item.valueText.toUtf8().constData(), &w, nullptr) == 0) {
            valueW = SDL_max(valueW, w);
        }
    }
    // Keep the layout stable while values change (e.g. "5%" -> "100%")
    if (TTF_SizeUTF8(font, "+100%", &w, nullptr) == 0) {
        valueW = SDL_max(valueW, w);
    }

    int rowsWidth = labelW + k_MenuColumnGap + k_MenuTrackWidth + k_MenuColumnGap + valueW;

    int titleW = 0, buttonsW = 0, hintW = 0;
    TTF_SizeUTF8(font, menu.title.toUtf8().constData(), &titleW, nullptr);
    int resetW = 0, closeW = 0;
    TTF_SizeUTF8(font, "Reset", &resetW, nullptr);
    TTF_SizeUTF8(font, "X", &closeW, nullptr);
    buttonsW = resetW + closeW + 4 * k_MenuButtonPadX + k_MenuColumnGap;
    if (!menu.hint.isEmpty()) {
        TTF_SizeUTF8(font, menu.hint.toUtf8().constData(), &hintW, nullptr);
    }

    int width = SDL_max(SDL_max(rowsWidth, titleW + k_MenuColumnGap + buttonsW), hintW);
    int height = (lineH + k_MenuRowPadding) +                          // Title row
                 menu.items.count() * (lineH + k_MenuRowPadding) +     // Item rows
                 (menu.hint.isEmpty() ? 0 : lineH + 4);                // Hint
    return QSize(width, height);
}

void OverlayManager::DrawMenu(SDL_Surface* surface, TTF_Font* font, int x, int y, int width, const OverlayMenu& menu, MenuLayout& layout)
{
    const SDL_Color white = {0xF0, 0xF0, 0xF0, 0xFF};
    const SDL_Color gray = {0x9A, 0x9A, 0x9A, 0xFF};
    const SDL_Color accent = {0x4F, 0xC3, 0xF7, 0xFF};
    const int lineH = TTF_FontHeight(font);
    const int rowH = lineH + k_MenuRowPadding;

    auto fillRect = [&](const QRect& r, SDL_Color c) {
        SDL_Rect sr = { r.x(), r.y(), r.width(), r.height() };
        SDL_FillRect(surface, &sr, SDL_MapRGBA(surface->format, c.r, c.g, c.b, c.a));
    };
    auto drawText = [&](const QString& str, int tx, int ty, SDL_Color c) -> int {
        if (str.isEmpty()) {
            return 0;
        }
        SDL_Surface* t = TTF_RenderUTF8_Blended(font, str.toUtf8().constData(), c);
        if (t == nullptr) {
            return 0;
        }
        SDL_SetSurfaceBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_Rect dst = { tx, ty, t->w, t->h };
        SDL_BlitSurface(t, nullptr, surface, &dst);
        int w = t->w;
        SDL_FreeSurface(t);
        return w;
    };
    auto drawButton = [&](const QString& label, int right, int ty) -> QRect {
        int tw = 0;
        TTF_SizeUTF8(font, label.toUtf8().constData(), &tw, nullptr);
        QRect r(right - tw - 2 * k_MenuButtonPadX, ty, tw + 2 * k_MenuButtonPadX, lineH + 4);
        fillRect(r, {0x50, 0x50, 0x5A, 0xF0});
        drawText(label, r.x() + k_MenuButtonPadX, r.y() + 2, white);
        return r;
    };

    // Measure columns the same way as MeasureMenu()
    int labelW = 0, w;
    for (const OverlayMenuItem& item : menu.items) {
        if (TTF_SizeUTF8(font, item.label.toUtf8().constData(), &w, nullptr) == 0) {
            labelW = SDL_max(labelW, w);
        }
    }

    // Title row with Reset and Close buttons on the right
    drawText(menu.title, x, y + 2, accent);
    layout.close = drawButton("X", x + width, y);
    layout.reset = drawButton("Reset", layout.close.x() - k_MenuColumnGap / 2, y);
    int rowY = y + rowH;

    layout.rows.clear();
    layout.tracks.clear();
    for (int i = 0; i < menu.items.count(); i++) {
        const OverlayMenuItem& item = menu.items[i];
        QRect row(x - 4, rowY, width + 8, rowH);
        QRect track(x + labelW + k_MenuColumnGap, rowY + (rowH - k_MenuTrackHeight) / 2, k_MenuTrackWidth, k_MenuTrackHeight);

        if (i == menu.selected) {
            fillRect(row, {0x36, 0x40, 0x52, 0xF0});
        }

        SDL_Color textColor = item.enabled ? white : gray;
        drawText(item.label, x, rowY + k_MenuRowPadding / 2, textColor);

        // Slider track, filled portion, and knob
        fillRect(track, {0x5A, 0x5A, 0x64, 0xFF});
        if (item.enabled && item.maximum > item.minimum) {
            float fraction = (float)(item.value - item.minimum) / (item.maximum - item.minimum);
            fraction = SDL_min(1.0f, SDL_max(0.0f, fraction));
            int knobX = track.x() + (int)(fraction * (track.width() - 1));
            fillRect(QRect(track.x(), track.y(), knobX - track.x(), track.height()), accent);
            fillRect(QRect(knobX - k_MenuKnobWidth / 2, rowY + 4, k_MenuKnobWidth, rowH - 8), white);
        }

        drawText(item.valueText, track.right() + 1 + k_MenuColumnGap, rowY + k_MenuRowPadding / 2, textColor);

        layout.rows.append(row);
        // The clickable track area is taller than the drawn track for easier grabbing
        layout.tracks.append(QRect(track.x() - k_MenuKnobWidth / 2, rowY, track.width() + k_MenuKnobWidth, rowH));
        rowY += rowH;
    }

    if (!menu.hint.isEmpty()) {
        drawText(menu.hint, x, rowY + 2, gray);
    }
}

void OverlayManager::DrawFrameGraph(SDL_Surface* surface, TTF_Font* font, int x, int y, int width, const FrameGraphSnapshot& graph)
{
    static const SDL_Color k_SeriesColors[FrameGraphSeriesMax] = {
        {0x4F, 0xC3, 0xF7, 0xFF}, // Network: light blue
        {0xFF, 0xC1, 0x07, 0xFF}, // Display: amber
    };
    static const char* const k_SeriesNames[FrameGraphSeriesMax] = { "net", "display" };
    const SDL_Color labelColor = {0xB0, 0xB0, 0xB0, 0xFF};
    const int samplesShown = width / 2;

    // Worst frame interval of each series over the visible window
    float seriesMax[FrameGraphSeriesMax] = {};
    float overallMax = 0;
    for (int s = 0; s < FrameGraphSeriesMax; s++) {
        int start = SDL_max(0, graph.count[s] - samplesShown);
        for (int i = start; i < graph.count[s]; i++) {
            seriesMax[s] = SDL_max(seriesMax[s], graph.samples[s][i]);
        }
        overallMax = SDL_max(overallMax, seriesMax[s]);
    }

    // Legend, e.g. "net max 21.3  display max 17.9 ms"
    int legendX = x;
    auto drawLegendText = [&](const char* str, SDL_Color color) {
        SDL_Surface* t = TTF_RenderUTF8_Blended(font, str, color);
        if (t != nullptr) {
            SDL_SetSurfaceBlendMode(t, SDL_BLENDMODE_BLEND);
            SDL_Rect dst = { legendX, y, t->w, t->h };
            SDL_BlitSurface(t, nullptr, surface, &dst);
            legendX += t->w;
            SDL_FreeSurface(t);
        }
    };
    char buf[48];
    for (int s = 0; s < FrameGraphSeriesMax; s++) {
        drawLegendText(k_SeriesNames[s], k_SeriesColors[s]);
        SDL_snprintf(buf, sizeof(buf), s + 1 < FrameGraphSeriesMax ? " max %.1f  " : " max %.1f ms", seriesMax[s]);
        drawLegendText(buf, labelColor);
    }

    // The plot's vertical scale fits the worst spike, with the frame interval target
    // at a sensible height, and is capped so a single huge stall doesn't flatten it.
    int plotY = y + TTF_FontHeight(font) + 4;
    const int plotH = k_FrameGraphHeight;
    float scaleMax = SDL_max(graph.targetMs * 2.5f, overallMax);
    scaleMax = SDL_min(scaleMax, SDL_max(100.0f, graph.targetMs * 4.0f));
    scaleMax = SDL_ceilf(scaleMax / 5.0f) * 5.0f;

    auto valueToY = [&](float ms) {
        float clamped = SDL_min(ms, scaleMax);
        return plotY + plotH - 1 - (int)(clamped / scaleMax * (plotH - 1));
    };

    if (SDL_LockSurface(surface) != 0) {
        return;
    }

    auto putPixel = [&](int px, int py, Uint32 color) {
        if (px >= 0 && py >= 0 && px < surface->w && py < surface->h) {
            ((Uint32*)((Uint8*)surface->pixels + py * surface->pitch))[px] = color;
        }
    };

    // Plot background
    Uint32 background = SDL_MapRGBA(surface->format, 0x26, 0x26, 0x2E, 0xD8);
    for (int py = plotY; py < plotY + plotH; py++) {
        for (int px = x; px < x + width; px++) {
            putPixel(px, py, background);
        }
    }

    // Dotted reference line at the target frame interval (e.g. 16.7 ms at 60 FPS)
    Uint32 targetColor = SDL_MapRGBA(surface->format, 0x90, 0x90, 0x90, 0xFF);
    int targetY = valueToY(graph.targetMs);
    for (int px = x; px < x + width; px += 4) {
        putPixel(px, targetY, targetColor);
        putPixel(px + 1, targetY, targetColor);
    }

    // Each series as a 2px wide connected line, newest sample at the right edge
    for (int s = 0; s < FrameGraphSeriesMax; s++) {
        Uint32 color = SDL_MapRGBA(surface->format, k_SeriesColors[s].r, k_SeriesColors[s].g, k_SeriesColors[s].b, 0xFF);
        int shown = SDL_min(graph.count[s], samplesShown);
        int start = graph.count[s] - shown;
        int prevY = -1;

        for (int i = 0; i < shown; i++) {
            int px = x + width - (shown - i) * 2;
            int py = valueToY(graph.samples[s][start + i]);
            int fromY = prevY < 0 ? py : prevY;

            for (int ly = SDL_min(fromY, py); ly <= SDL_max(fromY, py); ly++) {
                putPixel(px, ly, color);
                putPixel(px + 1, ly, color);
            }
            prevY = py;
        }
    }

    SDL_UnlockSurface(surface);
}
