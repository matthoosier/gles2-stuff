#include "base.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#define N_ELEMENTS(_a) (sizeof(_a) / sizeof(_a[0]))

class IcosahedronWindow: public WaylandWindow
{
public:
    IcosahedronWindow()             {}
    virtual ~IcosahedronWindow()    {}

protected:
    virtual void setupGl();
    virtual void drawGl(uint32_t time);
    virtual void teardownGl();
    virtual std::vector<EGLint> requiredEglConfigAttribs();

private:
    GLuint m_rotationUniform;
    GLuint m_position;
    GLuint m_color;
};

#define X .525731112119133606 
#define Z .850650808352039932

static GLfloat vdata[12][3] = {    
   {-X, 0.0, Z}, {X, 0.0, Z}, {-X, 0.0, -Z}, {X, 0.0, -Z},    
   {0.0, Z, X}, {0.0, Z, -X}, {0.0, -Z, X}, {0.0, -Z, -X},    
   {Z, X, 0.0}, {-Z, X, 0.0}, {Z, -X, 0.0}, {-Z, -X, 0.0} 
};

static GLuint tindices[20][3] = { 
   {0,4,1}, {0,9,4}, {9,5,4}, {4,5,8}, {4,8,1},    
   {8,10,1}, {8,3,10}, {5,3,8}, {5,2,3}, {2,7,3},    
   {7,10,3}, {7,6,10}, {7,11,6}, {11,0,6}, {0,1,6}, 
   {6,1,10}, {9,0,11}, {9,11,2}, {9,2,5}, {7,2,11}
};

// These are just a bunch of available colors. We'll randomly assign
// them to individual trianges.
static const GLfloat available_colors[][3] = {
    { 0, 0, 0 }, // black
    { 1, 1, 1 }, // white
    { 1, .5, .5 }, // pink
    { .5, 1, .5 }, // light green
    { .5, .5, 1 }, // light blue
    { .5, .5, .5 }, // medium grey
    { 0, .5, 0 }, // dark green
    { 1, .25, .25 }, // light red
    { 224.0 / 256, 176.0 / 255, 255.0 / 256 }, // purple
    { 0, 1, 1 }, // cyan
    { .25, .25, .25 }, // dark grey
    { 1, 0, 0 }, // true red
    { 0, 1, 0 }, // true green
    { 0, 0, 1 }, // true blue
    { .75, .75, .75 }, // light grey
    { 0, 0, .5 }, // dark blue
    { .5, 0, 0, }, // dark red
    { 1, 1, .5 }, // light yellow
};

static GLfloat icosahedron_vertices[N_ELEMENTS(tindices) * 3][3];
static GLfloat icosahedron_vertex_colors[N_ELEMENTS(tindices) * 3][3];

#define str(s) _str(s)
#define _str(s) #s

static const char *vert_shader_text =
    "uniform mat4 rotation;\n"

    "attribute vec4 pos;\n"
    "attribute vec3 color;\n"

    "const vec3 light_pos = vec3(0, 0, +1);\n"

    "varying vec3 v_color;\n"

    "void main() {\n"
    "  gl_Position = rotation * pos;\n"

    "  // Gouraud shading. Compute a lighting-influenced color value for each\n"
    "  // vertex and stuff it into 'v_color'. The fragment shader will be\n"
    "  // presented with an interpolated value of 'v_color' for each specific\n"
    "  // pixel, saving us from doing a per-pixel lighting calculation.\n"

    "  // Figure out the maximum conceivable distance to the light source\n"
    "  float max_x_delta = -" str(X) " - light_pos.x;\n"
    "  float max_y_delta = light_pos.y;\n"
    "  float max_z_delta = -" str(Z) " - light_pos.z;\n"
    "  float max_distance = sqrt(pow(max_x_delta, 2) + pow(max_y_delta, 2) + pow(max_z_delta, 2));\n"
    "  float x_diff = gl_Position.x - light_pos.x;\n"
    "  float y_diff = gl_Position.y - light_pos.y;\n"
    "  float z_diff = gl_Position.z - light_pos.z;\n"

    "  float distance_to_light = sqrt(x_diff * x_diff + y_diff * y_diff + z_diff * z_diff);\n"

    "  // This is a cheat. Luminance actually decreaes with the inverse square of\n"
    "  // distance, but that didn't produce a striking enough effect here. So we\n"
    "  // calculate the luminance as the inverse cube of distance to make it pop\n"
    "  // better.\n"
    "  float luminance = pow((distance_to_light / max_distance), 3);\n"
    "  v_color = color * luminance;\n"
    "}\n";

static const char *frag_shader_text =
    "precision mediump float;\n"
    "varying vec3 v_color;\n"
    "void main() {\n"
    "  gl_FragColor = vec4(v_color, 1);\n"
    "}\n";

std::vector<EGLint> IcosahedronWindow::requiredEglConfigAttribs()
{
    std::vector<EGLint> ret;
    ret.push_back(EGL_DEPTH_SIZE);
    ret.push_back(4);
    return ret;
}

void IcosahedronWindow::setupGl()
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

    m_rotationUniform = glGetUniformLocation(program, "rotation");
    m_position = glGetAttribLocation(program, "pos");
    m_color = glGetAttribLocation(program, "color");

    int i;
    int c;

    for (i = 0, c = 0; i < N_ELEMENTS(tindices); i++, c = (c + 1) % N_ELEMENTS(available_colors)) {

    	GLfloat* vertex0 = icosahedron_vertices[i*3 + 0];
    	GLfloat* vertex1 = icosahedron_vertices[i*3 + 1];
    	GLfloat* vertex2 = icosahedron_vertices[i*3 + 2];

    	memcpy(vertex0, &vdata[tindices[i][0]][0], 3 * sizeof(GLfloat));
    	memcpy(vertex1, &vdata[tindices[i][1]][0], 3 * sizeof(GLfloat));
    	memcpy(vertex2, &vdata[tindices[i][2]][0], 3 * sizeof(GLfloat));

    	GLfloat* v0color = icosahedron_vertex_colors[i*3 + 0];
    	GLfloat* v1color = icosahedron_vertex_colors[i*3 + 1];
    	GLfloat* v2color = icosahedron_vertex_colors[i*3 + 2];

    	memcpy(v0color, &available_colors[c][0], 3 * sizeof(GLfloat));
    	memcpy(v1color, &available_colors[c][0], 3 * sizeof(GLfloat));
    	memcpy(v2color, &available_colors[c][0], 3 * sizeof(GLfloat));
    }
}

void IcosahedronWindow::drawGl(uint32_t time)
{
    static const GLfloat verts[3][3] = {
    	{ -0.5, -0.5, 0 },
    	{  0.5, -0.5, 0 },
    	{  0,    0.5, 0 },
    };
    GLfloat rotation[4][4] = {
    	{ 1, 0, 0, 0 },
    	{ 0, 1, 0, 0 },
    	{ 0, 0, 1, 0 },
    	{ 0, 0, 0, 1 }
    };

    static const uint32_t speed_div = 5, benchmark_interval = 5;

    GLfloat angle = (time / speed_div) % 360 * M_PI / 180.0;
    rotation[0][0] =  cos(angle);
    rotation[0][2] =  sin(angle);
    rotation[2][0] = -sin(angle);
    rotation[2][2] =  cos(angle);

    glViewport(0, 0, currentSize().m_width, currentSize().m_width);

    glUniformMatrix4fv(m_rotationUniform, 1, GL_FALSE, (GLfloat *) rotation);

    glClearColor(0.0, 0.0, 0.0, 0.5);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    glVertexAttribPointer(m_position, 3, GL_FLOAT, GL_FALSE, 0, icosahedron_vertices);
    glEnableVertexAttribArray(m_position);

    glVertexAttribPointer(m_color, 3, GL_FLOAT, GL_FALSE, 0, icosahedron_vertex_colors);
    glEnableVertexAttribArray(m_color);

    glDrawArrays(GL_TRIANGLES, 0, N_ELEMENTS(icosahedron_vertices));

    glDisableVertexAttribArray(m_position);
    glDisableVertexAttribArray(m_color);

    glDisable(GL_DEPTH_TEST);
}

void IcosahedronWindow::teardownGl()
{
}

int
main(int argc, char **argv)
{
    IcosahedronWindow w;
    w.init(&argc, argv);
    w.run();
    return EXIT_SUCCESS;
}
