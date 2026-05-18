// imagewidget_opengl.cpp
#include "imagewidget.h"

#ifdef HAS_QT6_OPENGL

void ImageWidget::initOpenGL()
{
    if (!m_glWidget) {
        m_glWidget = new QOpenGLWidget(this);
        m_glWidget->setFormat(QSurfaceFormat::defaultFormat());

        // 初始化着色器
        m_shaderProgram = new QOpenGLShaderProgram(this);

        // 顶点着色器
        const char* vertexShader = R"(
            #version 330 core
            layout(location = 0) in vec4 vertex;
            layout(location = 1) in vec2 texCoord;
            uniform mat4 matrix;
            out vec2 v_texCoord;
            void main() {
                gl_Position = matrix * vertex;
                v_texCoord = texCoord;
            }
        )";

        // 片段着色器
        const char* fragmentShader = R"(
            #version 330 core
            in vec2 v_texCoord;
            uniform sampler2D texture;
            out vec4 fragColor;
            void main() {
                fragColor = texture(texture, v_texCoord);
            }
        )";

        m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
        m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader);
        m_shaderProgram->link();

        // 设置顶点缓冲区（全屏四边形）
        float vertices[] = {
            -1.0f, -1.0f, 0.0f, 0.0f,
            1.0f, -1.0f, 1.0f, 0.0f,
            1.0f,  1.0f, 1.0f, 1.0f,
            -1.0f,  1.0f, 0.0f, 1.0f
        };

        m_vertexBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        m_vertexBuffer->create();
        m_vertexBuffer->bind();
        m_vertexBuffer->allocate(vertices, sizeof(vertices));
        m_vertexBuffer->release();
    }
}

void ImageWidget::enableOpenGLAcceleration(bool enable)
{
    m_useOpenGL = enable;
    if (enable && !m_glWidget) {
        initOpenGL();
    }
}

#endif // HAS_QT6_OPENGL