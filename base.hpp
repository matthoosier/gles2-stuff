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
    WaylandWindow();
    virtual ~WaylandWindow();

    void init(int* argc, char* argv[]);
    void run();

    virtual void setupGl() = 0;
    virtual void drawGl(uint32_t time) = 0;
    virtual void teardownGl() = 0;

protected:
    GLuint createShader(std::string const& shaderText, GLenum shaderType);

    int width() { return m_width; }
    int height() { return m_height; }

    virtual std::vector<EGLint> requiredEglConfigAttribs() {
        return std::vector<EGLint>();
    }

private:
    void redraw(struct wl_callback* callback, uint32_t time);
    
private:
    // Server interfaces
    struct wl_display* m_display;
    struct wl_registry* m_registry;
    struct wl_compositor* m_compositor;
    struct wl_shell* m_shell;

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

    // Callback table structures
    static const struct wl_registry_listener s_registryListener;
    static const struct wl_callback_listener s_configureCallbackListener;
    static const struct wl_callback_listener s_frameCallbackListener;
    static const struct wl_shell_surface_listener s_shellSurfaceListener;

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
    int m_width;
    int m_height;
};

#endif
