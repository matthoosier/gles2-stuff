#include <assert.h>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include "base.hpp"

const struct wl_registry_listener WaylandWindow::s_registryListener = {
    &WaylandWindow::handleRegistryGlobal,
    &WaylandWindow::handleRegistryGlobalRemove,
};

const struct wl_callback_listener WaylandWindow::s_configureCallbackListener = {
    &WaylandWindow::handleConfigureCallback,
};

const struct wl_callback_listener WaylandWindow::s_frameCallbackListener = {
    &WaylandWindow::handleFrameCallback,
};

const struct wl_shell_surface_listener WaylandWindow::s_shellSurfaceListener = {
    &WaylandWindow::handlePing,
    &WaylandWindow::handleConfigure,
    &WaylandWindow::handlePopupDone,
};

WaylandWindow::WaylandWindow()
    : m_display(NULL)
    , m_registry(NULL)
    , m_compositor(NULL)
    , m_shell(NULL)
    , m_configured(false)
    , m_callback(NULL)
{
}

WaylandWindow::~WaylandWindow()
{
}

void WaylandWindow::init(int* argc, char* argv[])
{
    m_width = 250;
    m_height = 250;

    m_display = wl_display_connect(NULL);
    assert(m_display);

    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &s_registryListener, this);
    wl_display_dispatch(m_display);
    assert(m_compositor);
    assert(m_shell);

    static const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE,
    };

    const char* extensions;

    EGLint stock_config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    };

    // First populate the boilerplate EGL config attributes into a
    // vector
    std::vector<EGLint> config_attribs;

    for (int i = 0; i < sizeof(stock_config_attribs) / sizeof(stock_config_attribs[0]);) {
        config_attribs.push_back(stock_config_attribs[i]);
        config_attribs.push_back(stock_config_attribs[i + 1]);

        i += 2;
    }

    // Fetch any additional EGL config attributes that the specific
    // application uses.
    std::vector<EGLint> custom_config_attribs = requiredEglConfigAttribs();

    config_attribs.insert(
            config_attribs.end(),
            custom_config_attribs.begin(),
            custom_config_attribs.end()
    );

    // Terminate the vector of EGL config attributes
    config_attribs.push_back(EGL_NONE);

    m_eglDisplay = eglGetDisplay(m_display);
    assert(m_eglDisplay);

    int major, minor;
    int ret = eglInitialize(m_eglDisplay, &major, &minor);
    assert(ret == EGL_TRUE);

    ret = eglBindAPI(EGL_OPENGL_ES_API);
    assert(ret == EGL_TRUE);

    int count;
    if (!eglGetConfigs(m_eglDisplay, NULL, 0, &count) || count < 1) {
        assert(false);
    }

    EGLConfig* configs = new EGLConfig[count];

    int n;
    ret = eglChooseConfig(m_eglDisplay, &config_attribs[0], configs, count, &n);
    assert(ret && n >= 1);

    m_eglConfig = NULL;
    int buffer_size = 32;

    for (int i = 0; i < n; i++) {
        EGLint size;
        eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_BUFFER_SIZE, &size);

        if (buffer_size == size) {
            m_eglConfig = configs[i];
            break;
        }
    }

    delete[] configs;

    if (m_eglConfig == NULL) {
        std::fprintf(stderr, "did not find config with buffer size %d\n", buffer_size);
        exit(EXIT_FAILURE);
    }

    m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, context_attribs);
    assert(m_eglContext);

    m_surface = wl_compositor_create_surface(m_compositor);
    m_shellSurface = wl_shell_get_shell_surface(m_shell, m_surface);

    wl_shell_surface_add_listener(m_shellSurface, &s_shellSurfaceListener, this);

    m_eglWindow = wl_egl_window_create(m_surface, m_width, m_height);
    m_eglSurface = eglCreateWindowSurface(m_eglDisplay, m_eglConfig, m_eglWindow, NULL);

    wl_shell_surface_set_title(m_shellSurface, "blah");
    wl_shell_surface_set_toplevel(m_shellSurface);
    handleConfigure(this, m_shellSurface, 0, m_width, m_height);

    struct wl_callback* callback;
    callback = wl_display_sync(m_display);
    wl_callback_add_listener(callback, &s_configureCallbackListener, this);

    ret = eglMakeCurrent(m_eglDisplay, m_eglSurface,
                         m_eglSurface, m_eglContext);
    assert(ret);

    setupGl();
}

void WaylandWindow::handleRegistryGlobal(
        void* data,
        struct wl_registry* registry,
        uint32_t name,
        const char* interface,
        uint32_t version)
{
    WaylandWindow* self = static_cast<WaylandWindow*>(data);

    if (std::string("wl_compositor") == interface) {
        self->m_compositor = (struct wl_compositor*)wl_registry_bind(
                registry,
                name,
                &wl_compositor_interface,
                1);
    }
    else if (std::string("wl_shell") == interface) {
        self->m_shell = (struct wl_shell*)wl_registry_bind(
                registry,
                name,
                &wl_shell_interface,
                1);
    }
}

void WaylandWindow::handleRegistryGlobalRemove(
        void* data,
        struct wl_registry* registry,
        uint32_t name)
{
}

void WaylandWindow::handleConfigureCallback(
        void* data,
        struct wl_callback* callback,
        uint32_t time)
{
    WaylandWindow* self = static_cast<WaylandWindow*>(data);

    wl_callback_destroy(callback);

    self->m_configured = true;

    if (!self->m_callback) {
        self->redraw(NULL, time);
    }
}

void WaylandWindow::handleConfigure(
        void* data,
        struct wl_shell_surface* shell_surface,
        uint32_t edges,
        int32_t width,
        int32_t height)
{
    WaylandWindow* self = static_cast<WaylandWindow*>(data);

    if (self->m_eglWindow) {
        wl_egl_window_resize(self->m_eglWindow, width, height, 0, 0);
    }

    self->m_width = width;
    self->m_height = height;
}

void WaylandWindow::handlePing(void* data,
                               struct wl_shell_surface* shell_surface,
                               uint32_t serial)
{
}

void WaylandWindow::handlePopupDone(void* data,
                                    struct wl_shell_surface* shell_surface)
{
}

void WaylandWindow::handleFrameCallback(
        void* data,
        struct wl_callback* callback,
        uint32_t time)
{
    WaylandWindow* self = static_cast<WaylandWindow*>(data);
    self->redraw(callback, time);
}

void WaylandWindow::redraw(struct wl_callback* callback, uint32_t time)
{
    assert(m_callback == callback);
    m_callback = NULL;

    if (callback) {
        wl_callback_destroy(callback);
    }

    if (!m_configured) {
        return;
    }

    drawGl(time);

    m_callback = wl_surface_frame(m_surface);
    wl_callback_add_listener(m_callback, &s_frameCallbackListener, this);

    eglSwapBuffers(m_eglDisplay, m_eglSurface);
}

void WaylandWindow::run()
{
    for (int ret = 0; ret != -1;) {
        ret = wl_display_dispatch(m_display);
    }
}

GLuint WaylandWindow::createShader(std::string const& shaderText, GLenum shaderType)
{
    GLuint shader;
    GLint status;

    shader = glCreateShader(shaderType);
    assert(shader != 0);

    GLchar* sources[] = {
        (GLchar*)shaderText.c_str(),
        NULL
    };

    glShaderSource(shader, 1, sources, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[1000];
        GLsizei len;
        glGetShaderInfoLog(shader, 1000, &len, log);
        std::fprintf(stderr, "Error: compiling %s: %*s\n",
                     shaderType == GL_VERTEX_SHADER ? "vertex" : "fragment",
                     len, log);
        exit(EXIT_FAILURE);
    }

    return shader;
}