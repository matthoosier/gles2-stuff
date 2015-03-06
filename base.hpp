#ifndef __BASE_HPP__
#define __BASE_HPP__

#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <vector>

class WaylandWindow
{
public:
    struct Size
    {
        Size(uint32_t width, uint32_t height)
            : m_width(width)
            , m_height(height)
        {
        }

        uint32_t m_width;
        uint32_t m_height;
    };

public:
    WaylandWindow();
    virtual ~WaylandWindow();

    void init(int* argc, char* argv[]);
    void run();

// Override these
protected:
    virtual void setupGl() = 0;
    virtual void drawGl(uint32_t time) = 0;
    virtual void teardownGl() = 0;

    virtual std::vector<EGLint> requiredEglConfigAttribs() {
        return std::vector<EGLint>();
    }

// Internal API
protected:
    GLuint createShader(std::string const& shaderText, GLenum shaderType);

    Size const& nonFullscreenSize() const   { return m_nonFullscreenSize; }
    Size const& currentSize() const         { return m_currentSize; }

    void setFullscreen(bool fullscreen);

private:
    void redraw(struct wl_callback* callback, uint32_t time);

private:
    // Server interfaces
    struct wl_display* m_display;
    struct wl_registry* m_registry;
    struct wl_compositor* m_compositor;
    struct wl_shell* m_shell;
    struct wl_seat* m_seat;
    struct wl_keyboard* m_keyboard;

    // Callbacks
    static void handleRegistryGlobal(void* data, struct wl_registry* registry,
                                     uint32_t name, const char* interface,
                                     uint32_t version);

    static void handleRegistryGlobalRemove(void* data,
                                           struct wl_registry* registry,
                                           uint32_t name);

    static void handleConfigureCallback(void* data,
                                        struct wl_callback* callback,
                                        uint32_t time);

    static void handleFrameCallback(void* data,
                                    struct wl_callback* callback,
                                    uint32_t time);

    static void handleConfigure(void* data,
                                struct wl_shell_surface* shell_surface,
                                uint32_t edges,
                                int32_t width,
                                int32_t height);

    static void handlePing(void* data, struct wl_shell_surface* shell_surface,
                           uint32_t serial);

    static void handlePopupDone(void* data,
                                struct wl_shell_surface* shell_surface);

    static void handleSeatCapabilities(void* data,
                                       struct wl_seat* seat,
                                       uint32_t capabilities);

    static void handleSeatName(void* data,
                               struct wl_seat* seat,
                               char const* name);

    static void handleKeyboardKeymap(void *data,
                                     struct wl_keyboard* keyboard,
                                     uint32_t format,
                                     int32_t fd,
                                     uint32_t size);

    static void handleKeyboardEnter(void *data,
                                    struct wl_keyboard* keyboard,
                                    uint32_t serial,
                                    struct wl_surface* surface,
                                    struct wl_array* keys);

    static void handleKeyboardLeave(void *data,
                                    struct wl_keyboard* keyboard,
                                    uint32_t serial,
                                    struct wl_surface* surface);

    static void handleKeyboardKey(void *data, struct wl_keyboard* keyboard,
                                  uint32_t serial, uint32_t time,
                                  uint32_t key, uint32_t state);

    static void handleKeyboardModifiers(void* data,
                                        struct wl_keyboard* keyboard,
                                        uint32_t serial,
                                        uint32_t mods_depressed,
                                        uint32_t mods_latched,
                                        uint32_t mods_locked,
                                        uint32_t group);

    static void handleKeyboardRepeatInfo(void* data,
                                         struct wl_keyboard* keyboard,
                                         int32_t rate,
                                         int32_t delay);

    // Callback table structures
    static const struct wl_registry_listener s_registryListener;
    static const struct wl_callback_listener s_configureCallbackListener;
    static const struct wl_callback_listener s_frameCallbackListener;
    static const struct wl_shell_surface_listener s_shellSurfaceListener;
    static const struct wl_seat_listener s_seatListener;
    static const struct wl_keyboard_listener s_keyboardListener;

    // Client objects
    struct wl_surface* m_surface;
    struct wl_shell_surface* m_shellSurface;
    struct wl_egl_window* m_eglWindow;
    EGLDisplay m_eglDisplay;
    EGLContext m_eglContext;
    EGLConfig m_eglConfig;
    EGLSurface m_eglSurface;
    bool m_configured;
    struct wl_callback* m_callback;

    // Random data
    Size m_nonFullscreenSize;
    Size m_currentSize;
    bool m_fullscreen;
};

#endif
