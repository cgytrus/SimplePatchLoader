#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <numbers>

#include "pp.hpp"

// do u guys like hardcore opengl? well i have good news for u !!!!!! :3

// cuz mac also ccglprogram is annoying anyway fuck ccglprogram all my homies do shaders manually
struct Shader {
    GLuint program = 0;

    Result<std::string> setup(
        const ghc::filesystem::path& vertexPath,
        const ghc::filesystem::path& fragmentPath
    ) {
        auto vertexSource = file::readString(vertexPath);
        if (!vertexSource)
            return Err(vertexSource.unwrapErr());

        auto fragmentSource = file::readString(fragmentPath);
        if (!fragmentSource)
            return Err(fragmentSource.unwrapErr());

        auto getShaderLog = [](GLuint id) -> std::string {
            GLint length, written;
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
            if (length <= 0)
                return "";
            auto stuff = new char[length + 1];
            glGetShaderInfoLog(id, length, &written, stuff);
            std::string result(stuff);
            delete[] stuff;
            return result;
        };
        auto getProgramLog = [](GLuint id) -> std::string {
            GLint length, written;
            glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length);
            if (length <= 0)
                return "";
            auto stuff = new char[length + 1];
            glGetProgramInfoLog(id, length, &written, stuff);
            std::string result(stuff);
            delete[] stuff;
            return result;
        };
        GLint res;

        GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
        auto oglSucks = vertexSource.unwrap().c_str();
        glShaderSource(vertex, 1, &oglSucks, nullptr);
        glCompileShader(vertex);
        auto vertexLog = getShaderLog(vertex);

        glGetShaderiv(vertex, GL_COMPILE_STATUS, &res);
        if(!res) {
            glDeleteShader(vertex);
            return Err("vertex shader compilation failed:\n{}", vertexLog);
        }

        GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
        oglSucks = fragmentSource.unwrap().c_str();
        glShaderSource(fragment, 1, &oglSucks, nullptr);
        glCompileShader(fragment);
        auto fragmentLog = getShaderLog(fragment);

        glGetShaderiv(fragment, GL_COMPILE_STATUS, &res);
        if(!res) {
            glDeleteShader(vertex);
            glDeleteShader(fragment);
            return Err("fragment shader compilation failed:\n{}", fragmentLog);
        }

        program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        auto programLog = getProgramLog(program);

        glDeleteShader(vertex);
        glDeleteShader(fragment);

        glGetProgramiv(program, GL_LINK_STATUS, &res);
        if(!res) {
            glDeleteProgram(program);
            program = 0;
            return Err("shader link failed:\n{}", programLog);
        }

        return Ok(fmt::format(
            "shader compilation successful. logs:\nvert:\n{}\nfrag:\n{}\nprogram:\n{}",
            vertexLog, fragmentLog, programLog
        ));
    }

    void cleanup() {
        if (program)
            glDeleteProgram(program);
        program = 0;
    }
};

// too lazy to figure out the cocos way so we're going full opengl mode
// i will probably regret this later
struct RenderTexture {
    GLuint fbo = 0;
    GLuint tex = 0;
    GLuint rbo = 0;

    void setup(GLsizei width, GLsizei height) {
        GLint drawFbo = 0;
        GLint readFbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            log::error("pp fbo not complete, uh oh! i guess i will have to cut off ur pp now");
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
    }

    void cleanup() {
        if (fbo)
            glDeleteFramebuffers(1, &fbo);
        if (tex)
            glDeleteTextures(1, &tex);
        if (rbo)
            glDeleteRenderbuffers(1, &rbo);
        fbo = 0;
        tex = 0;
        rbo = 0;
    }
};

RenderTexture ppRt0;
RenderTexture ppRt1;
GLuint ppVao = 0;
GLuint ppVbo = 0;
Shader ppShader;
GLint ppShaderFast = 0;
GLint ppShaderFirst = 0;
GLint ppShaderRadius = 0;

void setupPostProcess() {
    auto size = CCDirector::get()->getOpenGLView()->getFrameSize();

    ppRt0.setup((GLsizei)size.width, (GLsizei)size.height);
    ppRt1.setup((GLsizei)size.width, (GLsizei)size.height);

    GLfloat ppVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
        1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
        1.0f, -1.0f,  1.0f, 0.0f,
        1.0f,  1.0f,  1.0f, 1.0f
    };
    glGenVertexArrays(1, &ppVao);
    glGenBuffers(1, &ppVbo);
    glBindVertexArray(ppVao);
    glBindBuffer(GL_ARRAY_BUFFER, ppVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ppVertices), &ppVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    auto vertexPath = (std::string)CCFileUtils::get()->fullPathForFilename("pp-vert.glsl"_spr, false);
    auto fragmentPath = (std::string)CCFileUtils::get()->fullPathForFilename("pp-frag.glsl"_spr, false);

    auto res = ppShader.setup(vertexPath, fragmentPath);
    if (!res) {
        log::error("{}", res.unwrapErr());
        return;
    }
    log::info("{}", res.unwrap());
    glUniform1i(glGetUniformLocation(ppShader.program, "screen"), 0);
    ppShaderFast = glGetUniformLocation(ppShader.program, "fast");
    ppShaderFirst = glGetUniformLocation(ppShader.program, "first");
    ppShaderRadius = glGetUniformLocation(ppShader.program, "radius");
}
void cleanupPostProcess() {
    ppRt0.cleanup();
    ppRt1.cleanup();

    if (ppVao)
        glDeleteVertexArrays(1, &ppVao);
    if (ppVbo)
        glDeleteBuffers(1, &ppVbo);
    ppVao = 0;
    ppVbo = 0;

    ppShader.cleanup();
    ppShaderFast = 0;
    ppShaderFirst = 0;
    ppShaderRadius = 0;
}

#include <Geode/modify/CCNode.hpp>
class $modify(CCNode) {
    void visit() { // NOLINT(*-use-override)
        if ((CCNode*)this != CCDirector::get()->getRunningScene() || ppShader.program == 0) {
            CCNode::visit();
            return;
        }
        float blur = pp::blurAmount * (1.f - std::cos((float)std::numbers::pi * pp::blurTimer)) * 0.5f;
        if (blur == 0.f) {
            CCNode::visit();
            return;
        }

        // the pipeline goes like this:
        // gd > ppRt0
        // ppRt0 > ppRt1
        // ppRt1 > window
        // the second step runs the first pass of the shader and
        // the third step runs the second pass

        // save currently bound fbo
        GLint drawFbo = 0;
        GLint readFbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);

        // bind our framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, ppRt0.fbo);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        CCNode::visit();

        glBindVertexArray(ppVao);
#if defined(GEODE_IS_WINDOWS)
        ccGLUseProgram(ppShader.program);
#else
        GLint prevProgram;
        glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
        glUseProgram(ppShader.program);
#endif
        glUniform1i(ppShaderFast, pp::blurFast);
        glUniform1f(ppShaderRadius, blur);

        // bind our first pass framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, ppRt1.fbo);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glUniform1i(ppShaderFirst, GL_TRUE);
        glBindTexture(GL_TEXTURE_2D, ppRt0.tex);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // bind the framebuffer that was bound before us
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
        // CCDirector::drawScene already clears the main framebuffer

        glUniform1i(ppShaderFirst, GL_FALSE);
        glBindTexture(GL_TEXTURE_2D, ppRt1.tex);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(0);

#if !defined(GEODE_IS_WINDOWS)
        glUseProgram(prevProgram);
#endif
    }
};

#include <Geode/modify/CCEGLViewProtocol.hpp>
class $modify(CCEGLViewProtocol) {
    void setFrameSize(float width, float height) { // NOLINT(*-use-override)
        CCEGLViewProtocol::setFrameSize(width, height);
        if (!CCDirector::get()->getOpenGLView())
            return;
        cleanupPostProcess();
        setupPostProcess();
    }
};
#include <Geode/modify/GameManager.hpp>
class $modify(GameManager) {
    void reloadAllStep5() {
        GameManager::reloadAllStep5();
        cleanupPostProcess();
        setupPostProcess();
    }
};

$on_mod(Loaded) {
    Loader::get()->queueInMainThread([]() {
        cleanupPostProcess();
        setupPostProcess();
    });
}
$on_mod(Unloaded) {
    cleanupPostProcess();
}
