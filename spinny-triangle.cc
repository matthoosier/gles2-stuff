#include <cstdio>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include "base.hpp"

static const char* vert_shader_text =
    "uniform mat4 rotation;\n"
    "attribute vec4 pos;\n"
    "attribute vec4 color;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "  gl_Position = rotation * pos;\n"
    "  v_color = color;\n"
    "}\n";

static const char* frag_shader_text =
    "precision mediump float;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "  gl_FragColor = v_color;\n"
    "}\n";

class MyWaylandWindow: public WaylandWindow
{
public:
    MyWaylandWindow() {}
    virtual ~MyWaylandWindow() {}

    virtual void setupGl()
    {
        GLint status;

        m_program = glCreateProgram();
        glAttachShader(m_program, createShader(frag_shader_text, GL_FRAGMENT_SHADER));
        glAttachShader(m_program, createShader(vert_shader_text, GL_VERTEX_SHADER));
        glLinkProgram(m_program);

        glGetProgramiv(m_program, GL_LINK_STATUS, &status);
        if (!status) {
            char log[1000];
            GLsizei len;
            glGetProgramInfoLog(m_program, 1000, &len, log);
            std::fprintf(stderr, "Error: linking:\n%*s\n", len, log);
            exit(EXIT_FAILURE);
        }

        glUseProgram(m_program);

        m_pos = glGetAttribLocation(m_program, "pos");
        m_col = glGetAttribLocation(m_program, "color");
        m_rotation = glGetUniformLocation(m_program, "rotation");
    }

    virtual void drawGl(uint32_t time)
    {
        static const GLfloat verts[3][2] = {
            { -0.5, -0.5 },
            {  0.5, -0.5 },
            {  0,    0.5 }
        };
        static const GLfloat colors[3][3] = {
            { 1, 0, 0 },
            { 0, 1, 0 },
            { 0, 0, 1 },
        };

        GLfloat rotation[4][4] = {
            { 1, 0, 0, 0 },
            { 0, 1, 0, 0 },
            { 0, 0, 1, 0 },
            { 0, 0, 0, 1 },
        };

        GLfloat angle = (time / 10) % 360 * M_PI / 180.0;
        rotation[0][0] = cos(angle);
        rotation[0][1] = sin(angle);
        rotation[1][0] = -sin(angle);
        rotation[1][1] = cos(angle);

        glViewport(0, 0, width(), height());
        glClearColor(0, 0, 0, 0.5);
        glClear(GL_COLOR_BUFFER_BIT);

        glVertexAttribPointer(m_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
        glVertexAttribPointer(m_col, 3, GL_FLOAT, GL_FALSE, 0, colors);
        glEnableVertexAttribArray(m_pos);
        glEnableVertexAttribArray(m_col);

        glUniformMatrix4fv(m_rotation, 1, GL_FALSE, (GLfloat *)rotation);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glDisableVertexAttribArray(m_pos);
        glDisableVertexAttribArray(m_col);
    }

    virtual void teardownGl()
    {
    }

private:
    GLuint m_program;
    GLuint m_pos;
    GLuint m_col;
    GLuint m_rotation;
};

int main (int argc, char* argv[])
{
    MyWaylandWindow w;
    w.init(&argc, argv);
    w.run();
}
