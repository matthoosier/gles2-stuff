/*
 * Copyright © 2011 Benjamin Franzke
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>

#include <linux/input.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define N_ELEMENTS(_a) (sizeof(_a) / sizeof(_a[0]))

struct window;
struct seat;

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shell *shell;
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_touch *touch;
	struct wl_keyboard *keyboard;
	struct wl_shm *shm;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor *default_cursor;
	struct wl_surface *cursor_surface;
	struct {
		EGLDisplay dpy;
		EGLContext ctx;
		EGLConfig conf;
	} egl;
	struct window *window;
};

struct geometry {
	int width, height;
};

struct window {
	struct display *display;
	struct geometry geometry, window_size;
	struct {
		GLuint u_rotation;
		GLuint u_projection;
		GLuint u_light_pos;
		GLuint u_ambient;
		GLuint a_pos;
		GLuint a_norm;
		GLuint a_color;
	} gl;

	uint32_t benchmark_time, frames;
	struct wl_egl_window *native;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	EGLSurface egl_surface;
	struct wl_callback *callback;
	int fullscreen, configured, opaque, buffer_size, frame_sync;
};

static const char *vert_shader_text =
        "uniform mat4 u_rotation;\n"
	"uniform mat4 u_projection;\n"

	"attribute vec4 a_pos;\n"
	"attribute vec4 a_norm;\n"
	"attribute vec3 a_color;\n"

	"varying vec3 v_color;\n"
	"varying vec4 v_norm;\n"
	"varying vec4 v_pos;\n"

	"void main() {\n"
	"  gl_Position = u_rotation * a_pos;\n"
	"  v_pos = gl_Position;\n"
	"  v_norm = u_rotation * normalize(a_norm);\n"
        "  gl_Position *= u_projection;\n"
	"  v_color = a_color;\n"
	"}\n";

static const char *frag_shader_text =
	"precision mediump float;\n"

	"uniform vec4 u_light_pos;\n"
	"uniform float u_ambient;\n"

	"varying vec3 v_color;\n"
	"varying vec4 v_pos;\n"
	"varying vec4 v_norm;\n"

	"void main() {\n"
	"  vec4 L = u_light_pos - v_pos;\n"
	"  float lambert = dot(normalize(L.xyz), v_norm.xyz);\n"
	"  gl_FragColor = vec4(v_color * (u_ambient + (1.0 - u_ambient) * lambert), 1);\n"
	"}\n";

static int running = 1;

static void
init_egl(struct display *display, struct window * window)
{
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 1,
		EGL_DEPTH_SIZE, 4,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLint major, minor, n, count, i, size;
	EGLConfig *configs;
	EGLBoolean ret;

	if (window->opaque || window->buffer_size == 16)
		config_attribs[9] = 0;

	display->egl.dpy = eglGetDisplay(display->display);
	assert(display->egl.dpy);

	ret = eglInitialize(display->egl.dpy, &major, &minor);
	assert(ret == EGL_TRUE);
	ret = eglBindAPI(EGL_OPENGL_ES_API);
	assert(ret == EGL_TRUE);

	if (!eglGetConfigs(display->egl.dpy, NULL, 0, &count) || count < 1)
		assert(0);

	configs = (EGLConfig*)calloc(count, sizeof *configs);
	assert(configs);

	ret = eglChooseConfig(display->egl.dpy, config_attribs,
			      configs, count, &n);
	assert(ret && n >= 1);

	for (i = 0; i < n; i++) {
		eglGetConfigAttrib(display->egl.dpy,
				   configs[i], EGL_BUFFER_SIZE, &size);
		if (window->buffer_size == size) {
			display->egl.conf = configs[i];
			break;
		}
	}
	free(configs);
	if (display->egl.conf == NULL) {
		fprintf(stderr, "did not find config with buffer size %d\n",
			window->buffer_size);
		exit(EXIT_FAILURE);
	}

	display->egl.ctx = eglCreateContext(display->egl.dpy,
					    display->egl.conf,
					    EGL_NO_CONTEXT, context_attribs);
	assert(display->egl.ctx);
}

static void
fini_egl(struct display *display)
{
	eglTerminate(display->egl.dpy);
	eglReleaseThread();
}

static GLuint
create_shader(struct window *window, const char *source, GLenum shader_type)
{
	GLuint shader;
	GLint status;

	shader = glCreateShader(shader_type);
	assert(shader != 0);

	glShaderSource(shader, 1, (const char **) &source, NULL);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetShaderInfoLog(shader, 1000, &len, log);
		fprintf(stderr, "Error: compiling %s: %*s\n",
			shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
			len, log);
		exit(1);
	}

	return shader;
}

static void
init_gl(struct window *window)
{
	GLuint frag, vert;
	GLuint program;
	GLint status;

	frag = create_shader(window, frag_shader_text, GL_FRAGMENT_SHADER);
	vert = create_shader(window, vert_shader_text, GL_VERTEX_SHADER);

	program = glCreateProgram();
	glAttachShader(program, frag);
	glAttachShader(program, vert);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(program, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%*s\n", len, log);
		exit(1);
	}

	glUseProgram(program);

	window->gl.u_rotation =
		glGetUniformLocation(program, "u_rotation");

	window->gl.u_projection =
		glGetUniformLocation(program, "u_projection");

	window->gl.u_light_pos =
		glGetUniformLocation(program, "u_light_pos");

	window->gl.u_ambient =
		glGetUniformLocation(program, "u_ambient");

	window->gl.a_pos =
		glGetAttribLocation(program, "a_pos");

	window->gl.a_norm =
		glGetAttribLocation(program, "a_norm");

	window->gl.a_color =
		glGetAttribLocation(program, "a_color");
}

static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
	    uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
		 uint32_t edges, int32_t width, int32_t height)
{
	struct window *window = (struct window*)data;

	if (window->native)
		wl_egl_window_resize(window->native, width, height, 0, 0);

	window->geometry.width = width;
	window->geometry.height = height;

	if (!window->fullscreen)
		window->window_size = window->geometry;
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
	handle_ping,
	handle_configure,
	handle_popup_done,
};

static void redraw(void *data, struct wl_callback *callback, uint32_t time);

static void
configure_callback(void *data, struct wl_callback *callback, uint32_t time)
{
	struct window *window = (struct window*)data;

	wl_callback_destroy(callback);

	window->configured = 1;

	if (window->callback == NULL)
		redraw(data, NULL, time);
}

static struct wl_callback_listener configure_callback_listener = {
	configure_callback,
};

static void
toggle_fullscreen(struct window *window, int fullscreen)
{
	struct wl_callback *callback;

	window->fullscreen = fullscreen;
	window->configured = 0;

	if (fullscreen) {
		wl_shell_surface_set_fullscreen(window->shell_surface,
						WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
						0, NULL);
	} else {
		wl_shell_surface_set_toplevel(window->shell_surface);
		handle_configure(window, window->shell_surface, 0,
				 window->window_size.width,
				 window->window_size.height);
	}

	callback = wl_display_sync(window->display->display);
	wl_callback_add_listener(callback, &configure_callback_listener,
				 window);
}

static void
create_surface(struct window *window)
{
	struct display *display = window->display;
	EGLBoolean ret;
	
	window->surface = wl_compositor_create_surface(display->compositor);
	window->shell_surface = wl_shell_get_shell_surface(display->shell,
							   window->surface);

	wl_shell_surface_add_listener(window->shell_surface,
				      &shell_surface_listener, window);

	window->native =
		wl_egl_window_create(window->surface,
				     window->window_size.width,
				     window->window_size.height);
	window->egl_surface =
		eglCreateWindowSurface(display->egl.dpy,
				       display->egl.conf,
				       window->native, NULL);

	wl_shell_surface_set_title(window->shell_surface, "simple-egl");

	ret = eglMakeCurrent(window->display->egl.dpy, window->egl_surface,
			     window->egl_surface, window->display->egl.ctx);
	assert(ret == EGL_TRUE);

	toggle_fullscreen(window, window->fullscreen);
}

static void
destroy_surface(struct window *window)
{
	/* Required, otherwise segfault in egl_dri2.c: dri2_make_current()
	 * on eglReleaseThread(). */
	eglMakeCurrent(window->display->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
		       EGL_NO_CONTEXT);

	eglDestroySurface(window->display->egl.dpy, window->egl_surface);
	wl_egl_window_destroy(window->native);

	wl_shell_surface_destroy(window->shell_surface);
	wl_surface_destroy(window->surface);

	if (window->callback)
		wl_callback_destroy(window->callback);
}

static const struct wl_callback_listener frame_listener = {
	redraw
};

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
	static const GLfloat vertices[6 * 6 * 3] = {
		-1, -1, -1, // left face (x == -1)
		-1, -1, +1,
		-1, +1, +1,
		-1, +1, +1,
		-1, -1, -1,
		-1, +1, -1,

		+1, -1, -1, // right face (x == +1)
		+1, -1, +1,
		+1, +1, +1,
		+1, +1, +1,
		+1, -1, -1,
		+1, +1, -1,

		-1, -1, +1, // front face (z == +1)
		-1, +1, +1,
		+1, +1, +1,
		+1, +1, +1,
		-1, -1, +1,
		+1, -1, +1,

		-1, -1, -1, // back face (z == -1)
		-1, +1, -1,
		+1, +1, -1,
		+1, +1, -1,
		-1, -1, -1,
		+1, -1, -1,

		-1, +1, -1, // top face (y == +1)
		-1, +1, +1,
		+1, +1, +1,
		+1, +1, +1,
		-1, +1, -1,
		+1, +1, -1,

		-1, -1, -1, // bottom face (y == -1)
		-1, -1, +1,
		+1, -1, +1,
		+1, -1, +1,
		-1, -1, -1,
		+1, -1, -1,
	};

	static const GLfloat colors[6 * 6 * 3] = {
		1.0, 1.0, 0.5, // left (yellow)
		1.0, 1.0, 0.5,
		1.0, 1.0, 0.5,
		1.0, 1.0, 0.5,
		1.0, 1.0, 0.5,
		1.0, 1.0, 0.5,

		1.0, 0.3, 0.3, // right (red)
		1.0, 0.3, 0.3,
		1.0, 0.3, 0.3,
		1.0, 0.3, 0.3,
		1.0, 0.3, 0.3,
		1.0, 0.3, 0.3,

		0.5, 0.5, 1.0, // front (light blue)
		0.5, 0.5, 1.0,
		0.5, 0.5, 1.0,
		0.5, 0.5, 1.0,
		0.5, 0.5, 1.0,
		0.5, 0.5, 1.0,

		0.5, 0.5, 0.5, // back (grey)
		0.5, 0.5, 0.5,
		0.5, 0.5, 0.5,
		0.5, 0.5, 0.5,
		0.5, 0.5, 0.5,
		0.5, 0.5, 0.5,

		0.5, 0.0, 1.0, // top (purple)
		0.5, 0.0, 1.0,
		0.5, 0.0, 1.0,
		0.5, 0.0, 1.0,
		0.5, 0.0, 1.0,
		0.5, 0.0, 1.0,

		1.0, 1.0, 1.0, // bottom (white)
		1.0, 1.0, 1.0,
		1.0, 1.0, 1.0,
		1.0, 1.0, 1.0,
		1.0, 1.0, 1.0,
		1.0, 1.0, 1.0,
	};

	struct window *window = (struct window *)data;
	struct display *display = window->display;
	GLfloat angle;
	static const uint32_t benchmark_interval = 5;
	static const uint32_t speed_div = 20;
	struct wl_region *region;
	struct timeval tv;

	assert(window->callback == callback);
	window->callback = NULL;

	if (callback)
		wl_callback_destroy(callback);

	if (!window->configured)
		return;

	gettimeofday(&tv, NULL);
	time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	if (window->frames == 0)
		window->benchmark_time = time;
	if (time - window->benchmark_time > (benchmark_interval * 1000)) {
		printf("%d frames in %d seconds: %f fps\n",
		       window->frames,
		       benchmark_interval,
		       (float) window->frames / benchmark_interval);
		window->benchmark_time = time;
		window->frames = 0;
	}

	angle = (time / speed_div) % 360 * M_PI / 180.0;

	glViewport(0, 0, window->geometry.width, window->geometry.height);

	glm::mat4 u_rotation = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1.0f));
	// Simple orthographic projection just barely large enough
	// to be a bounding box on the radius of the vertices
	glm::mat4 u_projection = glm::ortho(-sqrt(3), sqrt(3),
	                                    -sqrt(3), sqrt(3),
	                                    -sqrt(3), sqrt(3));

	glm::vec4 u_light_pos = glm::vec4(10.0, 10.0, 10.0, 1);

	glUniformMatrix4fv(window->gl.u_rotation, 1, GL_FALSE, glm::value_ptr(u_rotation));

	glUniformMatrix4fv(window->gl.u_projection, 1, GL_FALSE, glm::value_ptr(u_projection));

	glUniform4fv(window->gl.u_light_pos, 1, glm::value_ptr(u_light_pos));

	glUniform1f(window->gl.u_ambient, .5);

	glClearColor(0.0, 0.0, 0.0, 0.5);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);

	glVertexAttribPointer(window->gl.a_pos, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	glEnableVertexAttribArray(window->gl.a_pos);

	glVertexAttribPointer(window->gl.a_norm, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	glEnableVertexAttribArray(window->gl.a_norm);

	glVertexAttribPointer(window->gl.a_color, 3, GL_FLOAT, GL_FALSE, 0, colors);
	glEnableVertexAttribArray(window->gl.a_color);

	glDrawArrays(GL_TRIANGLES, 0, N_ELEMENTS(vertices) / 3);

	glDisableVertexAttribArray(window->gl.a_pos);
	glDisableVertexAttribArray(window->gl.a_norm);
	glDisableVertexAttribArray(window->gl.a_color);

	glDisable(GL_DEPTH_TEST);

	if (window->opaque || window->fullscreen) {
		region = wl_compositor_create_region(window->display->compositor);
		wl_region_add(region, 0, 0,
			      window->geometry.width,
			      window->geometry.height);
		wl_surface_set_opaque_region(window->surface, region);
		wl_region_destroy(region);
	} else {
		wl_surface_set_opaque_region(window->surface, NULL);
	}

	window->callback = wl_surface_frame(window->surface);
	wl_callback_add_listener(window->callback, &frame_listener, window);

	eglSwapBuffers(display->egl.dpy, window->egl_surface);
	window->frames++;
}

static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface,
		     wl_fixed_t sx, wl_fixed_t sy)
{
	struct display *display = (struct display*)data;
	struct wl_buffer *buffer;
	struct wl_cursor *cursor = display->default_cursor;
	struct wl_cursor_image *image;

	if (display->window->fullscreen)
		wl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
	else if (cursor) {
		image = display->default_cursor->images[0];
		buffer = wl_cursor_image_get_buffer(image);
		wl_pointer_set_cursor(pointer, serial,
				      display->cursor_surface,
				      image->hotspot_x,
				      image->hotspot_y);
		wl_surface_attach(display->cursor_surface, buffer, 0, 0);
		wl_surface_damage(display->cursor_surface, 0, 0,
				  image->width, image->height);
		wl_surface_commit(display->cursor_surface);
	}
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface)
{
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
		      uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
}

static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		      uint32_t serial, uint32_t time, uint32_t button,
		      uint32_t state)
{
	struct display *display = (struct display*)data;

	if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED)
		wl_shell_surface_move(display->window->shell_surface,
				      display->seat, serial);
}

static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
		    uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
};

static void
touch_handle_down(void *data, struct wl_touch *wl_touch,
		  uint32_t serial, uint32_t time, struct wl_surface *surface,
		  int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
	struct display *d = (struct display *)data;

	wl_shell_surface_move(d->window->shell_surface, d->seat, serial);
}

static void
touch_handle_up(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time, int32_t id)
{
}

static void
touch_handle_motion(void *data, struct wl_touch *wl_touch,
		    uint32_t time, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
}

static void
touch_handle_frame(void *data, struct wl_touch *wl_touch)
{
}

static void
touch_handle_cancel(void *data, struct wl_touch *wl_touch)
{
}

static const struct wl_touch_listener touch_listener = {
	touch_handle_down,
	touch_handle_up,
	touch_handle_motion,
	touch_handle_frame,
	touch_handle_cancel,
};

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
		       uint32_t format, int fd, uint32_t size)
{
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface,
		      struct wl_array *keys)
{
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface)
{
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
		    uint32_t serial, uint32_t time, uint32_t key,
		    uint32_t state)
{
	struct display *d = (struct display*)data;

	if (key == KEY_F11 && state)
		toggle_fullscreen(d->window, d->window->fullscreen ^ 1);
	else if (key == KEY_ESC && state)
		running = 0;
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
			  uint32_t serial, uint32_t mods_depressed,
			  uint32_t mods_latched, uint32_t mods_locked,
			  uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_handle_keymap,
	keyboard_handle_enter,
	keyboard_handle_leave,
	keyboard_handle_key,
	keyboard_handle_modifiers,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
			 uint32_t caps)
{
	struct display *d = (struct display*)data;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !d->pointer) {
		d->pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(d->pointer, &pointer_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && d->pointer) {
		wl_pointer_destroy(d->pointer);
		d->pointer = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !d->keyboard) {
		d->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(d->keyboard, &keyboard_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && d->keyboard) {
		wl_keyboard_destroy(d->keyboard);
		d->keyboard = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !d->touch) {
		d->touch = wl_seat_get_touch(seat);
		wl_touch_set_user_data(d->touch, d);
		wl_touch_add_listener(d->touch, &touch_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && d->touch) {
		wl_touch_destroy(d->touch);
		d->touch = NULL;
	}
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities,
};

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t name, const char *interface, uint32_t version)
{
	struct display *d = (struct display*)data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			(wl_compositor*)wl_registry_bind(registry, name,
					 &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_shell") == 0) {
		d->shell = (wl_shell*)wl_registry_bind(registry, name,
					    &wl_shell_interface, 1);
	} else if (strcmp(interface, "wl_seat") == 0) {
		d->seat = (wl_seat*)wl_registry_bind(registry, name,
					   &wl_seat_interface, 1);
		wl_seat_add_listener(d->seat, &seat_listener, d);
	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = (wl_shm*)wl_registry_bind(registry, name,
					  &wl_shm_interface, 1);
		d->cursor_theme = wl_cursor_theme_load(NULL, 32, d->shm);
		if (!d->cursor_theme) {
			fprintf(stderr, "unable to load default theme\n");
			return;
		}
		d->default_cursor =
			wl_cursor_theme_get_cursor(d->cursor_theme, "left_ptr");
		if (!d->default_cursor) {
			fprintf(stderr, "unable to load default left pointer\n");
			// TODO: abort ?
		}
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

static void
signal_int(int signum)
{
	running = 0;
}

static void
usage(int error_code)
{
	fprintf(stderr, "Usage: simple-egl [OPTIONS]\n\n"
		"  -f\tRun in fullscreen mode\n"
		"  -o\tCreate an opaque surface\n"
		"  -s\tUse a 16 bpp EGL config\n"
		"  -b\tDon't sync to compositor redraw (eglSwapInterval 0)\n"
		"  -h\tThis help text\n\n");

	exit(error_code);
}

int
main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display display = { 0 };
	struct window  window  = { 0 };
	int i, ret = 0;

	window.display = &display;
	display.window = &window;
	window.window_size.width  = 250;
	window.window_size.height = 250;
	window.buffer_size = 32;
	window.frame_sync = 1;

	for (i = 1; i < argc; i++) {
		if (strcmp("-f", argv[i]) == 0)
			window.fullscreen = 1;
		else if (strcmp("-o", argv[i]) == 0)
			window.opaque = 1;
		else if (strcmp("-s", argv[i]) == 0)
			window.buffer_size = 16;
		else if (strcmp("-b", argv[i]) == 0)
			window.frame_sync = 0;
		else if (strcmp("-h", argv[i]) == 0)
			usage(EXIT_SUCCESS);
		else
			usage(EXIT_FAILURE);
	}

	display.display = wl_display_connect(NULL);
	assert(display.display);

	display.registry = wl_display_get_registry(display.display);
	wl_registry_add_listener(display.registry,
				 &registry_listener, &display);

	wl_display_dispatch(display.display);

	init_egl(&display, &window);
	create_surface(&window);
	init_gl(&window);

	display.cursor_surface =
		wl_compositor_create_surface(display.compositor);

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	while (running && ret != -1)
		ret = wl_display_dispatch(display.display);

	fprintf(stderr, "simple-egl exiting\n");

	destroy_surface(&window);
	fini_egl(&display);

	wl_surface_destroy(display.cursor_surface);
	if (display.cursor_theme)
		wl_cursor_theme_destroy(display.cursor_theme);

	if (display.shell)
		wl_shell_destroy(display.shell);

	if (display.compositor)
		wl_compositor_destroy(display.compositor);

	wl_registry_destroy(display.registry);
	wl_display_flush(display.display);
	wl_display_disconnect(display.display);

	return 0;
}