#include "fonts/menu/ui_sans.hpp"
#include "fonts/weapons/so2_weapon_icons.hpp"
#include "fonts/weapons/skeed_weapons.hpp"
#include "../../../src/func/weapon_icons.hpp"
#include "Android_draw/draw.h"
#include "../../../src/ui/preview_assets.hpp"
#include "../../../src/ui/cfg.hpp"

EGLDisplay display = EGL_NO_DISPLAY;
EGLConfig config;
EGLSurface surface = EGL_NO_SURFACE;
EGLContext context = EGL_NO_CONTEXT;

ANativeWindow *native_window;
ImFont* fontBold;
ImFont* fontMedium;
ImFont* fontDesc;
ImFont* espFont;
ImFont* weaponIconFont = nullptr;

int native_window_screen_x = 0;
int native_window_screen_y = 0;
android::ANativeWindowCreator::DisplayInfo displayInfo{0};
uint32_t orientation = 0;
bool g_Initialized = false;
ImGuiWindow *g_window = nullptr;

bool initGUI_draw(uint32_t _screen_x, uint32_t _screen_y, bool log) {
    orientation = displayInfo.orientation;

    if (!init_egl(_screen_x, _screen_y, log)) {
        return false;
    }

    if (!ImGui_init()) {
        return false;
    }

    return true;
}

bool init_egl(uint32_t _screen_x, uint32_t _screen_y, bool log) {
    ::native_window = android::ANativeWindowCreator::Create("Overlay", _screen_x, _screen_y, !cfg::style::capture_overlay, !cfg::style::capture_overlay);
    if(!native_window)return false;
    cfg::style::capture_applied=cfg::style::capture_overlay;

    ANativeWindow_acquire(native_window);
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        return false;
    }

    if (eglInitialize(display, 0, 0) != EGL_TRUE) {
        return false;
    }

    EGLint num_config = 0;
    const EGLint attribList[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_BLUE_SIZE, 5,
        EGL_GREEN_SIZE, 6,
        EGL_RED_SIZE, 5,
        EGL_BUFFER_SIZE, 32,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    const EGLint attrib_list[] = {
        EGL_CONTEXT_CLIENT_VERSION,
        3,
        EGL_NONE
    };

    if (eglChooseConfig(display, attribList, &config, 1, &num_config) != EGL_TRUE) {
        return false;
    }

    EGLint egl_format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &egl_format);
    ANativeWindow_setBuffersGeometry(native_window, 0, 0, egl_format);
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrib_list);
    if (context == EGL_NO_CONTEXT) {
        return false;
    }

    surface = eglCreateWindowSurface(display, config, native_window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        return false;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        return false;
    }

    return true;
}

// Recreate only the window/EGLSurface: the GL context, fonts, ImGui settings,
// touch subsystem and feature configuration remain alive. Commit after a successful
// bind; failure leaves the old surface in use and rolls the UI toggle back.
bool set_overlay_capture(bool enabled) {
    auto fail=[&](const char* error){
        cfg::style::capture_overlay=cfg::style::capture_applied;
        cfg::style::capture_error=error;
        std::fprintf(stderr,"[bycmd][CAPTURE] %s\n",error);return false;
    };
    if(!g_Initialized||!native_window||display==EGL_NO_DISPLAY||context==EGL_NO_CONTEXT||surface==EGL_NO_SURFACE)
        return fail("Capture change failed: graphics are not ready.");
    if(enabled==cfg::style::capture_applied){cfg::style::capture_overlay=enabled;cfg::style::capture_error=nullptr;return true;}
    auto* next_window=android::ANativeWindowCreator::Create("Overlay",native_window_screen_x,native_window_screen_y,!enabled,!enabled);
    if(!next_window)return fail("Capture change failed: cannot create window.");
    ANativeWindow_acquire(next_window);
    auto release_next=[&](){ANativeWindow_release(next_window);android::ANativeWindowCreator::Destroy(next_window);};
    EGLint format=0;
    if(eglGetConfigAttrib(display,config,EGL_NATIVE_VISUAL_ID,&format)!=EGL_TRUE||
       ANativeWindow_setBuffersGeometry(next_window,0,0,format)!=0){
        release_next();return fail("Capture change failed: window format unavailable.");
    }
    const auto next_surface=eglCreateWindowSurface(display,config,next_window,nullptr);
    if(next_surface==EGL_NO_SURFACE){release_next();return fail("Capture change failed: cannot create EGL surface.");}
    if(eglMakeCurrent(display,next_surface,next_surface,context)!=EGL_TRUE){
        eglMakeCurrent(display,surface,surface,context);
        eglDestroySurface(display,next_surface);release_next();
        return fail("Capture change failed: cannot bind new surface.");
    }
    if(!ImGui_ImplAndroid_Init(next_window)){
        eglMakeCurrent(display,surface,surface,context);ImGui_ImplAndroid_Init(native_window);
        eglDestroySurface(display,next_surface);release_next();
        return fail("Capture change failed: Android backend initialization.");
    }
    auto* old_window=native_window;const auto old_surface=surface;
    native_window=next_window;surface=next_surface;
    eglDestroySurface(display,old_surface);
    ANativeWindow_release(old_window);android::ANativeWindowCreator::Destroy(old_window);
    cfg::style::capture_applied=cfg::style::capture_overlay=enabled;
    cfg::style::capture_error=nullptr;
    std::fprintf(stderr,"[bycmd][CAPTURE] layer capture requested=%s (device/recorder dependent)\n",enabled?"included":"excluded");
    return true;
}

void screen_config() {
    displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
}

bool ImGui_init() {
    if (g_Initialized) {
        return true;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplAndroid_Init(native_window);
    ImGui_ImplOpenGL3_Init("#version 300 es");

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;

    static const ImWchar ui_ranges[] = {0x0020,0x00FF,0x0400,0x052F,0x2000,0x206F,0};
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    font_cfg.OversampleH = 2;
    font_cfg.OversampleV = 2;
    fontBold = io.Fonts->AddFontFromMemoryTTF((void*)ui_fonts::ui_bold, sizeof(ui_fonts::ui_bold), 26.f, &font_cfg, ui_ranges);
    fontMedium = io.Fonts->AddFontFromMemoryTTF((void*)ui_fonts::ui_regular, sizeof(ui_fonts::ui_regular), 20.f, &font_cfg, ui_ranges);
    fontDesc = io.Fonts->AddFontFromMemoryTTF((void*)ui_fonts::ui_regular, sizeof(ui_fonts::ui_regular), 16.f, &font_cfg, ui_ranges);
    espFont = io.Fonts->AddFontFromMemoryTTF((void*)ui_fonts::ui_regular, sizeof(ui_fonts::ui_regular), 18.f, &font_cfg, ui_ranges);
    // Primary Weapons[] font from skeed; legacy font retained only for otherwise unsupported icons.
    static const ImWchar weapon_ranges[] = {0x0041,0x005A,0x0061,0x006F,0};
    static const ImWchar legacy_ranges[] = {0x0028,0x0028,0x0030,0x0039,0x0055,0x0055,0x005B,0x005B,0x005D,0x005D,0x0069,0x0069,0x007B,0x007B,0x007D,0x007D,0};
    ImFontConfig icon_cfg;
    icon_cfg.FontDataOwnedByAtlas = false;
    icon_cfg.OversampleH = 2;   // baked larger + oversampled: icons stay sharp when scaled down
    icon_cfg.OversampleV = 2;
    icon_cfg.PixelSnapH = false;
    weaponIconFont = io.Fonts->AddFontFromMemoryTTF((void*)weapon_font::skeed_weapons, sizeof(weapon_font::skeed_weapons), 72.f, &icon_cfg, weapon_ranges);
    weapon_icons::legacy_font = io.Fonts->AddFontFromMemoryTTF((void*)weapon_font::so2_icons, sizeof(weapon_font::so2_icons), 72.f, &icon_cfg, legacy_ranges);
    io.FontDefault = fontMedium;
    preview_assets::add(io.Fonts);
    io.Fonts->Build();
    preview_assets::blit(io.Fonts);

    ::g_Initialized = true;
    return true;
}

void drawBegin() {
    // UI requests from the preceding frame are applied before ImGui::NewFrame.
    if(cfg::style::capture_overlay!=cfg::style::capture_applied)set_overlay_capture(cfg::style::capture_overlay);
    screen_config();
    if (::orientation != displayInfo.orientation) {
        ::orientation = displayInfo.orientation;
        touch::update(displayInfo.width, displayInfo.height, displayInfo.orientation);
        if (g_window) {
            g_window->Pos.x = 100;
            g_window->Pos.y = 125;
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(native_window_screen_x, native_window_screen_y);
    ImGui::NewFrame();
}

void drawEnd() {
    ImGui::Render();
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    eglSwapBuffers(display, surface);
}

void shutdown() {
    if (!g_Initialized) {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();

    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
        }
        if (surface != EGL_NO_SURFACE) {
            eglDestroySurface(display, surface);
        }
        eglTerminate(display);
    }
    display = EGL_NO_DISPLAY;
    context = EGL_NO_CONTEXT;
    surface = EGL_NO_SURFACE;

    ANativeWindow_release(native_window);
    android::ANativeWindowCreator::Destroy(native_window);
    ::g_Initialized = false;
}
