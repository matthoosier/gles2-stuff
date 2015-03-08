#include <cstdio>
#include <sys/time.h>

#include "base.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define N_ELEMENTS(_a) (sizeof(_a) / sizeof(_a[0]))

class CubeWindow: public WaylandWindow
{
public:
    CubeWindow()            {}
    virtual ~CubeWindow()   {}

protected:
    virtual void setupGl();
    virtual void drawGl(uint32_t time);
    virtual void teardownGl();

    virtual std::vector<EGLint> requiredEglConfigAttribs() {
        std::vector<EGLint> ret;
        ret.push_back(EGL_DEPTH_SIZE);
        ret.push_back(4);
        return ret;
    }

private:
    GLuint m_uModel;
    GLuint m_uView;
    GLuint m_uProjection;
    GLuint m_uLightPos;
    GLuint m_uAmbient;
    GLuint m_aPos;
    GLuint m_aNorm;
    GLuint m_aColor;
};

static const char *vert_shader_text =
        "uniform mat4 u_model;\n"
	"uniform mat4 u_view;\n"
	"uniform mat4 u_projection;\n"

	"attribute vec4 a_pos;\n"
	"attribute vec4 a_norm;\n"
	"attribute vec3 a_color;\n"

	"varying vec3 v_color;\n"
	"varying vec4 v_norm;\n"
	"varying vec4 v_pos;\n"

	"void main() {\n"
	"  gl_Position = u_projection * u_view * u_model * a_pos;\n"
	"  v_pos = u_view * u_model * a_pos;\n"
	"  v_norm = u_model * normalize(a_norm);\n"
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

void CubeWindow::setupGl()
{
	GLuint frag, vert;
	GLuint program;
	GLint status;

	frag = createShader(frag_shader_text, GL_FRAGMENT_SHADER);
	vert = createShader(vert_shader_text, GL_VERTEX_SHADER);

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

	m_uModel = glGetUniformLocation(program, "u_model");
	m_uView = glGetUniformLocation(program, "u_view");
	m_uProjection = glGetUniformLocation(program, "u_projection");
	m_uLightPos = glGetUniformLocation(program, "u_light_pos");
	m_uAmbient = glGetUniformLocation(program, "u_ambient");
	m_aPos = glGetAttribLocation(program, "a_pos");
	m_aNorm = glGetAttribLocation(program, "a_norm");
	m_aColor = glGetAttribLocation(program, "a_color");
}

void CubeWindow::drawGl(uint32_t time)
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

	GLfloat angle;
	static const uint32_t benchmark_interval = 5;
	static const uint32_t speed_div = 20;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	time = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	angle = (time / speed_div) % 360 * M_PI / 180.0;

	glViewport(0, 0, currentSize().m_width, currentSize().m_height);

	// Axes
	glm::vec3 left(1.f, 0.f, 0.f);
	glm::vec3 up(0.f, 1.f, 0.f);
	glm::vec3 near(0.f, 0.f, 1.f);

	// Rotation matrix. Use different primes to avoid gimbal lock.
	glm::mat4 u_model = glm::rotate(glm::mat4(1.0f), float(angle * 3. / 10), up)
	                  * glm::rotate(glm::mat4(1.0f), float(angle), left)
	                  * glm::rotate(glm::mat4(1.0f), float(angle * 7. / 10), near);

#if 0
	glm::mat4 u_view = glm::translate(glm::mat4(1.f), glm::vec3(0.0, 0.0, -1 * sqrt(3)));

	// Simple orthographic projection just barely large enough
	// to be a bounding box on the radius of the vertices
	glm::mat4 u_projection = glm::ortho(-sqrt(3), sqrt(3),
	                                    -sqrt(3), sqrt(3),
	                                    (double)(-2 * sqrt(3)), (double)(+2 * sqrt(3)));
#else
	// Simple frustum whose front pane is from (-1.5, 1.5 * aspectRatio) to
	// (1.5, -1.5 * aspectRatio) on the z=4.5 and whose back pane is on the
	// z=10.0 plane.
	glm::mat4 u_view = glm::translate(glm::mat4(1.f), glm::vec3(0.0, 0.0, -7.0));
	float aspectRatio = currentSize().m_width * 1.0f / currentSize().m_height;
	glm::mat4 u_projection = glm::frustum(-1.5 * aspectRatio, 1.5 * aspectRatio, 1.5, -1.5, 4.5, 10.0);
#endif

	glm::vec4 u_light_pos = glm::vec4(10.0, 10.0, +10.0, 1);

	glUniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(u_model));

	glUniformMatrix4fv(m_uView, 1, GL_FALSE, glm::value_ptr(u_view));

	glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, glm::value_ptr(u_projection));

	glUniform4fv(m_uLightPos, 1, glm::value_ptr(u_light_pos));

	glUniform1f(m_uAmbient, .5);

	glClearColor(0.0, 0.0, 0.0, 0.5);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);

	glVertexAttribPointer(m_aPos, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	glEnableVertexAttribArray(m_aPos);

	glVertexAttribPointer(m_aNorm, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	glEnableVertexAttribArray(m_aNorm);

	glVertexAttribPointer(m_aColor, 3, GL_FLOAT, GL_FALSE, 0, colors);
	glEnableVertexAttribArray(m_aColor);

	glDrawArrays(GL_TRIANGLES, 0, N_ELEMENTS(vertices) / 3);

	glDisableVertexAttribArray(m_aPos);
	glDisableVertexAttribArray(m_aNorm);
	glDisableVertexAttribArray(m_aColor);

	glDisable(GL_DEPTH_TEST);
}

void CubeWindow::teardownGl()
{
}

int
main(int argc, char **argv)
{
    CubeWindow w;
    w.init(&argc, argv);
    w.run();
    return EXIT_SUCCESS;
}
