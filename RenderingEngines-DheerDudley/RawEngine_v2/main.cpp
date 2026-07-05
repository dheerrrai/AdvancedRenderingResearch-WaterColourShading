#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include "core/mesh.h"
#include "core/assimpLoader.h"
#include "core/BaseWrapperTemplate.h"
#include "core/Camera.h"
#include "core/texture.h"

#define VSTUDIO

#ifdef MAC_CLION
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#endif

#ifdef VSTUDIO
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#endif
#include <filesystem>
#include <iostream>

double Mousex = 0;
double Mousey = 0;
int g_width = 800;
int g_height = 600;
int SceneID = 0;
double deltaTime = 0;
float lastX = g_width / 2.0f;
float lastY = g_height / 2.0f;
bool firstMouse = true;
float yaw = -90.0f;
float pitch = 0.0f;
float sensitivity = 0.5f;
core::Camera* CurCamera = new core::Camera();
bool rightMouseActive = false;
bool vsyncEnabled = true;
static int SelectedShader = 0;
int PixelCount = 1;

// Main scene render target (holds the pristine, unprocessed render each frame)
unsigned int sceneFrameBuffer;
unsigned int sceneColorTex;
unsigned int rbo;

// Ping-pong targets used for the linear part of the post-process chain
unsigned int frameBuffer01;
unsigned int textureColorbuffer;
unsigned int frameBuffer02;
unsigned int textureColorbuffer2;

// Dedicated targets for the Bleed/EdgeDarken branch, since both read the
// same pristine scene independently and are merged afterward
unsigned int bleedOutFrameBuffer;
unsigned int bleedOutTex;
unsigned int edgeOutFrameBuffer;
unsigned int edgeOutTex;

void checkGLError(const char* label) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        printf("GL ERROR [%s]: 0x%x\n", label, err);
    }
}

double FrameRate() {
    static double lastTime = glfwGetTime();
    static int nbFrames = 0;
    static double msPerFrame = 0.0;

    double currentTime = glfwGetTime();
    nbFrames++;
    deltaTime = currentTime - lastTime;

    if (deltaTime >= 1.0) {
        msPerFrame = double(nbFrames) / deltaTime;
        nbFrames = 0;
        lastTime += 1.0;
    }
    return msPerFrame;
}

void processInput(GLFWwindow *window) {
    const float cameraSpeed = 0.05f;

    if (glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        CurCamera->cameraPos += cameraSpeed * CurCamera->cameraForward;
        CurCamera->cameraTarget += cameraSpeed * CurCamera->cameraForward;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        CurCamera->cameraPos -= cameraSpeed * CurCamera->cameraForward;
        CurCamera->cameraTarget -= cameraSpeed * CurCamera->cameraForward;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        CurCamera->cameraPos -= cameraSpeed * CurCamera->cameraRight;
        CurCamera->cameraTarget -= cameraSpeed * CurCamera->cameraRight;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        CurCamera->cameraPos += cameraSpeed * CurCamera->cameraRight;
        CurCamera->cameraTarget += cameraSpeed * CurCamera->cameraRight;
    }

    ImGuiIO& io = ImGui::GetIO();
    rightMouseActive = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    if (rightMouseActive && !io.WantCaptureMouse) {
        float xoffset = io.MouseDelta.x * sensitivity;
        float yoffset = -io.MouseDelta.y * sensitivity;

        yaw += xoffset;
        pitch += yoffset;
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        CurCamera->cameraForward = glm::normalize(front);
        CurCamera->cameraRight = glm::normalize(glm::cross(CurCamera->cameraForward, CurCamera->cameraUp));
    }

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        rightMouseActive = false;
}

void framebufferSizeCallback(GLFWwindow *window, int width, int height) {
    g_width = width;
    g_height = height;
    glViewport(0, 0, width, height);

    glBindTexture(GL_TEXTURE_2D, sceneColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, textureColorbuffer2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, bleedOutTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, edgeOutTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

std::string readFileToString(const std::string &filePath) {
    std::ifstream fileStream(filePath, std::ios::in);
    if (!fileStream.is_open()) {
        printf("Could not open file: %s\n", filePath.c_str());
        return "";
    }
    std::stringstream buffer;
    buffer << fileStream.rdbuf();
    return buffer.str();
}

GLuint generateShader(const std::string &shaderPath, const GLuint shaderType) {
    printf("Loading shader: %s\n", shaderPath.c_str());
    const std::string shaderText = readFileToString(shaderPath);
    const GLuint shader = glCreateShader(shaderType);
    const char *s_str = shaderText.c_str();
    glShaderSource(shader, 1, &s_str, nullptr);
    glCompileShader(shader);
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        printf("Error! Shader issue [%s]: %s\n", shaderPath.c_str(), infoLog);
    }
    return shader;
}

// Creates a simple color-only render target (no depth) sized to the current
// window, and attaches it to the given framebuffer. Used for every
// post-process stage that doesn't need depth testing.
void createColorTarget(unsigned int &fbo, unsigned int &tex) {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g_width, g_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("ERROR::FRAMEBUFFER:: color target incomplete!\n");
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow *window = glfwCreateWindow(g_width, g_height, "LearnOpenGL", NULL, NULL);
    if (window == NULL) {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 400");

    glEnable(GL_DEPTH_TEST);
    glFrontFace(GL_CCW);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ---------------- Performance tracking ----------------
    const int historyLength = 200; // ~a few seconds of frames at 60fps
    float passHistory[5][historyLength] = {}; // [pass][frame]
    int historyWriteIndex = 0;

    bool loggingEnabled = false;
    std::ofstream csvLog;
    double logStartTime = 0.0;

    // ---------------- Shader compilation ----------------
    const GLuint modelVertexShader = generateShader("shaders/modelVertex.vert", GL_VERTEX_SHADER);
    const GLuint fragmentShader    = generateShader("shaders/fragment.frag", GL_FRAGMENT_SHADER);
    const GLuint litShader         = generateShader("shaders/lit.frag", GL_FRAGMENT_SHADER);
    const GLuint textureShader     = generateShader("shaders/texture.frag", GL_FRAGMENT_SHADER);
    const GLuint KernelFrag        = generateShader("shaders/Kernel.frag", GL_FRAGMENT_SHADER);
    const GLuint RegularColours    = generateShader("shaders/RegularColours.frag", GL_FRAGMENT_SHADER);
    const GLuint BlurFrag          = generateShader("shaders/BloomBlur.frag", GL_FRAGMENT_SHADER);
    const GLuint PixelFrag         = generateShader("shaders/Pixelated.frag", GL_FRAGMENT_SHADER);
    const GLuint BasePostPVertex   = generateShader("shaders/BaseQuad.vert", GL_VERTEX_SHADER);
    const GLuint PaperTextFrag     = generateShader("shaders/PaperTexture.frag", GL_FRAGMENT_SHADER);
    const GLuint WhiteBaseFrag     = generateShader("shaders/WhiteBase.frag", GL_FRAGMENT_SHADER);
    const GLuint GranFrag          = generateShader("shaders/Granulation.frag", GL_FRAGMENT_SHADER);
    const GLuint BleedFrag         = generateShader("shaders/BleedingColours.frag", GL_FRAGMENT_SHADER);
    const GLuint EdgeDarkenFrag    = generateShader("shaders/EdgeDarken.frag", GL_FRAGMENT_SHADER);
    const GLuint CompositeFrag     = generateShader("shaders/Composite.frag", GL_FRAGMENT_SHADER);

    int success;
    char infoLog[512];

    auto linkProgram = [&](GLuint vert, GLuint frag, const char* name) -> GLuint {
        GLuint prog = glCreateProgram();
        glAttachShader(prog, vert);
        glAttachShader(prog, frag);
        glLinkProgram(prog);
        glGetProgramiv(prog, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(prog, 512, NULL, infoLog);
            printf("Error! Making Shader Program [%s]: %s\n", name, infoLog);
        }
        return prog;
    };

    const unsigned int modelShaderProgram = linkProgram(modelVertexShader, fragmentShader, "model");
    const unsigned int textureShaderProgram = linkProgram(modelVertexShader, textureShader, "texture");
    const unsigned int litShaderProgram = linkProgram(modelVertexShader, litShader, "lit");

    const unsigned int KernelShader        = linkProgram(BasePostPVertex, KernelFrag, "Kernel");
    const unsigned int PaperTextureShader  = linkProgram(BasePostPVertex, PaperTextFrag, "PaperTexture");
    const unsigned int BleedColourShader   = linkProgram(BasePostPVertex, BleedFrag, "Bleed");
    const unsigned int GranShader          = linkProgram(BasePostPVertex, GranFrag, "Granulation");
    const unsigned int WhiteBaseShader     = linkProgram(BasePostPVertex, WhiteBaseFrag, "WhiteBase");
    const unsigned int RegularShader       = linkProgram(BasePostPVertex, RegularColours, "Regular");
    const unsigned int BlurShader          = linkProgram(BasePostPVertex, BlurFrag, "Blur");
    const unsigned int PixelShader         = linkProgram(BasePostPVertex, PixelFrag, "Pixel");
    const unsigned int EdgeDarkenShader    = linkProgram(BasePostPVertex, EdgeDarkenFrag, "EdgeDarken");
    const unsigned int CompositeShader     = linkProgram(BasePostPVertex, CompositeFrag, "Composite");

    glDeleteShader(litShader);
    glDeleteShader(modelVertexShader);
    glDeleteShader(fragmentShader);
    glDeleteShader(textureShader);
    glDeleteShader(KernelFrag);
    glDeleteShader(RegularColours);
    glDeleteShader(BasePostPVertex);
    glDeleteShader(BlurFrag);
    glDeleteShader(PixelFrag);
    glDeleteShader(PaperTextFrag);
    glDeleteShader(WhiteBaseFrag);
    glDeleteShader(GranFrag);
    glDeleteShader(BleedFrag);
    glDeleteShader(EdgeDarkenFrag);
    glDeleteShader(CompositeFrag);

    BaseWrapperTemplate<GLuint> ShaderPrograms;
    ShaderPrograms.add("Regular", RegularShader);
    ShaderPrograms.add("Kernel", KernelShader);
    ShaderPrograms.add("Blur", BlurShader);
    ShaderPrograms.add("Pixel", PixelShader);
    ShaderPrograms.add("WhiteBase", WhiteBaseShader);
    ShaderPrograms.add("PaperTexture", PaperTextureShader);
    ShaderPrograms.add("Bleed", BleedColourShader);
    ShaderPrograms.add("Granulation", GranShader);
    ShaderPrograms.add("EdgeDarken", EdgeDarkenShader);
    ShaderPrograms.add("Composite", CompositeShader);

    // ---------------- Scene render target ----------------
    // Holds the pristine, untouched model render each frame. Every
    // post-process pass that needs the "original" image (rather than
    // another pass's output) reads from here.
    glGenFramebuffers(1, &sceneFrameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFrameBuffer);

    glGenTextures(1, &sceneColorTex);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g_width, g_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTex, 0);

    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, g_width, g_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("ERROR::FRAMEBUFFER:: sceneFrameBuffer incomplete!\n");

    // ---------------- Post-process ping-pong targets ----------------
    // Used by the linear part of the chain (WhiteBase, Granulation, PaperTexture,
    // and the merged Bleed/EdgeDarken result).
    createColorTarget(frameBuffer01, textureColorbuffer);
    createColorTarget(frameBuffer02, textureColorbuffer2);

    // ---------------- Bleed / EdgeDarken branch targets ----------------
    // Both passes read the pristine scene independently, so each needs its
    // own output target; a Composite pass merges the two afterward.
    createColorTarget(bleedOutFrameBuffer, bleedOutTex);
    createColorTarget(edgeOutFrameBuffer, edgeOutTex);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---------------- Screen quad ----------------
    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    // ---------------- Scene content ----------------
    core::Mesh quad = core::Mesh::generateQuad();
    core::Model quadModel({quad});
    quadModel.translate(glm::vec3(0, 0, -2.5));
    quadModel.scale(glm::vec3(5, 5, 1));

    core::Model suzanne = core::AssimpLoader::loadModel("models/knight.obj");

    core::Texture RedTexture("textures/Armour.jpg");

    core::Model dog = core::AssimpLoader::loadModel("models/dogtags_online.obj");
    core::Model sanic = core::AssimpLoader::loadModel("models/sanic.obj");
    core::Model Mill = core::AssimpLoader::loadModel("models/low-poly-mill.obj");
    Mill.scale(glm::vec3(0.05, 0.05, 0.05));

    printf("DEBUG camPos: (%.2f, %.2f, %.2f)  camForward: (%.2f, %.2f, %.2f)\n",
        CurCamera->cameraPos.x, CurCamera->cameraPos.y, CurCamera->cameraPos.z,
        CurCamera->cameraForward.x, CurCamera->cameraForward.y, CurCamera->cameraForward.z);

    glm::vec4 clearColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);

    unsigned int watercolourQueries[5];
    glGenQueries(5, watercolourQueries);
    double passTimings[5] = {0, 0, 0, 0, 0};

    double currentTime = glfwGetTime();
    double finishFrameTime = 0.0;
    float deltaTimeF = 0.0f;
    float rotationStrength = 0.0f;
    bool xAxis = false, yAxis = true, zAxis = false;

    glm::vec3 lightDirection = {1.0f, 0.5f, 1.0f};
    glm::vec3 diffuseColour = {1.0f, 1.0f, 1.0f};
    glm::vec3 ambientColour = {1.0f, 1.0f, 1.0f};
    glm::vec3 specColr = {1.0f, 1.0f, 1.0f};
    float ambientIntensity = 0.1f;
    float specIntensity = 0.1f;
    float constAtt = 0, linAtt = 1, quadAtt = 1;

    // ---------------- Watercolour pipeline state ----------------
    // Timing/history/display index mapping is fixed as:
    // 0 = WhiteBase, 1 = EdgeDarken, 2 = Bleed, 3 = Granulation, 4 = PaperTexture
    // Bleed and EdgeDarken both read the scene render independently (rather
    // than chaining off each other) and are merged by a Composite pass, so
    // the outline stays crisp regardless of how strong the bleed is.
    bool whiteEnabled = false;
    float whiteThreshold = 1.5f;

    bool edgeEnabled = false;
    float edgeStrength = 0.003f;

    bool bleedColoursEnabled = false;
    float bleedStrength = 0.003f;

    // Used by Composite to decide which pixels come from the EdgeDarken
    // result versus the Bleed result.
    float edgeCompositeThreshold = 0.3f;

    bool granEnabled = false;
    float granulationStrength = 0.2f;

    bool paperEnabled = false;

    std::vector<std::vector<core::Model*>> SceneList;
    std::vector<core::Model*> scene;
    scene.push_back(&suzanne);
    // scene.push_back(&dog);
    SceneList.push_back(scene);

    std::vector<core::Model*> scene2;
    scene2.push_back(&Mill);
    SceneList.push_back(scene2);

    // ---------------- Uniform locations ----------------
    GLint mvpMatrixUniformLit = glGetUniformLocation(litShaderProgram, "mvpMatrix");
    GLint diffuseColorUniform = glGetUniformLocation(litShaderProgram, "diffuseCol");
    GLint LightDirUniform     = glGetUniformLocation(litShaderProgram, "lightDirection");
    GLint ModelMatrix         = glGetUniformLocation(litShaderProgram, "mMatrix");
    GLint litAmbientColour    = glGetUniformLocation(litShaderProgram, "ambientCol");
    GLint litAmbientintensity = glGetUniformLocation(litShaderProgram, "ambientIntensity");
    GLint litConstAtt         = glGetUniformLocation(litShaderProgram, "constAttenuation");
    GLint litLinAtt           = glGetUniformLocation(litShaderProgram, "linearAttenuation");
    GLint litQuadAtt          = glGetUniformLocation(litShaderProgram, "quadAttenuation");
    GLint specIntens          = glGetUniformLocation(litShaderProgram, "specIntensity");
    GLint specCol             = glGetUniformLocation(litShaderProgram, "specCol");
    GLint litTextureUniform   = glGetUniformLocation(litShaderProgram, "text");
    GLint CamPos              = glGetUniformLocation(litShaderProgram, "camPos");

    printf("DEBUG uniform locations -- mvpMatrixUniformLit:%d diffuseColorUniform:%d "
           "LightDirUniform:%d ModelMatrix:%d litAmbientColour:%d litAmbientintensity:%d "
           "litTextureUniform:%d CamPos:%d\n",
        mvpMatrixUniformLit, diffuseColorUniform, LightDirUniform, ModelMatrix,
        litAmbientColour, litAmbientintensity, litTextureUniform, CamPos);

    while (!glfwWindowShouldClose(window)) {
        const glm::mat4 view = glm::lookAt(CurCamera->cameraPos,
                             CurCamera->cameraPos + CurCamera->cameraForward,
                             CurCamera->cameraUp);
        const glm::mat4 projection = glm::perspective(glm::radians(45.0f),
            static_cast<float>(g_width) / static_cast<float>(g_height), 0.1f, 100.0f);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Raw Engine v2");
        ImGui::Text("%f FPS\n", FrameRate());
        if (ImGui::Checkbox("VSync", &vsyncEnabled)) {
            glfwSwapInterval(vsyncEnabled ? 1 : 0);
        }
        ImGui::SliderFloat("Rotation Strength", &rotationStrength, 0.0f, 1000.0f);

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Rotation Axis")) {
                ImGui::Checkbox("X - Axis", &xAxis);
                ImGui::Checkbox("Y - Axis", &yAxis);
                ImGui::Checkbox("Z - Axis", &zAxis);
                if ((int)xAxis + (int)yAxis + (int)zAxis == 0) yAxis = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Post Processing")) {
                ImGui::Checkbox("White Base Remap", &whiteEnabled);
                ImGui::SameLine(); ImGui::Text("%.3f ms", passTimings[0]);
                ImGui::SliderFloat("WhiteThreshold", &whiteThreshold, 0.0f, 5.0f);

                ImGui::Checkbox("Edge Creation", &edgeEnabled);
                ImGui::SameLine(); ImGui::Text("%.3f ms", passTimings[1]);
                ImGui::SliderFloat("EdgeStrength", &edgeStrength, 0.00f, 10.0f);

                ImGui::Checkbox("BleedColours", &bleedColoursEnabled);
                ImGui::SameLine(); ImGui::Text("%.3f ms", passTimings[2]);
                ImGui::SliderFloat("Bleed Strength", &bleedStrength, 0.0f, 10.0f);
                ImGui::SliderFloat("Composite Edge Threshold", &edgeCompositeThreshold, 0.0f, 1.0f);

                ImGui::Checkbox("Granulation", &granEnabled);
                ImGui::SameLine(); ImGui::Text("%.3f ms", passTimings[3]);
                ImGui::SliderFloat("Granulation Strength", &granulationStrength, 0.0f, 0.3f);

                ImGui::Checkbox("Paper Texture", &paperEnabled);
                ImGui::SameLine(); ImGui::Text("%.3f ms", passTimings[4]);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Lighting")) {
                if (ImGui::BeginMenu("Position")) {
                    ImGui::SliderFloat("X", &lightDirection[0], -50.0f, 50.0f);
                    ImGui::SliderFloat("Y", &lightDirection[1], -50.0f, 50.0f);
                    ImGui::SliderFloat("Z", &lightDirection[2], -50.0f, 50.0f);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Attenuation")) {
                    ImGui::SliderFloat("Constant", &constAtt, 0, 30.0f);
                    ImGui::SliderFloat("Linear", &linAtt, 0, 1.0f);
                    ImGui::SliderFloat("Quad", &quadAtt, 0, 1.0f);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Ambience")) {
                    ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 1.0f);
                    ImGui::ColorEdit3("Colour", &ambientColour[0]);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Diffuse")) {
                    ImGui::ColorEdit3("Colour", &diffuseColour[0]);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Specular")) {
                    ImGui::SliderFloat("Intensity", &specIntensity, 0, 128);
                    ImGui::ColorEdit3("Colour", &specColr[0]);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Performance")) {
                const char* passLabels[5] = { "WhiteBase", "EdgeDarken", "Bleed", "Granulation", "PaperTexture" };
                for (int p = 0; p < 5; p++) {
                    char overlay[32];
                    snprintf(overlay, sizeof(overlay), "%.3f ms", passTimings[p]);
                    ImGui::PlotLines(passLabels[p], passHistory[p], historyLength,
                                      historyWriteIndex, overlay, 0.0f, FLT_MAX, ImVec2(200, 40));
                }
                ImGui::Separator();
                ImGui::Checkbox("Log to CSV", &loggingEnabled);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            SceneID = (SceneID + 1) % SceneList.size();
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            SceneID = (SceneID - 1 + SceneList.size()) % SceneList.size();
        ImGui::End();

        processInput(window);
        Mill.rotate(glm::vec3((float)xAxis, (float)yAxis, (float)zAxis),
                    glm::radians(rotationStrength) * deltaTimeF);

        // ---------------- Scene pass -> sceneFrameBuffer ----------------
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFrameBuffer);
        glViewport(0, 0, g_width, g_height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        auto SceneIteration = SceneList[SceneID].begin();
        while (SceneIteration != SceneList[SceneID].end()) {
            core::Model* model = *SceneIteration;
            glUseProgram(litShaderProgram);
            glUniformMatrix4fv(mvpMatrixUniformLit, 1, GL_FALSE, glm::value_ptr(projection * view * model->getModelMatrix()));
            glUniformMatrix4fv(ModelMatrix, 1, GL_FALSE, glm::value_ptr(model->getModelMatrix()));
            glUniform3fv(LightDirUniform, 1, glm::value_ptr(lightDirection));
            glUniform1f(litAmbientintensity, ambientIntensity);
            glUniform3fv(diffuseColorUniform, 1, glm::value_ptr(diffuseColour));
            glUniform3fv(litAmbientColour, 1, glm::value_ptr(ambientColour));
            glUniform1f(litConstAtt, constAtt);
            glUniform1f(litLinAtt, linAtt);
            glUniform1f(litQuadAtt, quadAtt);
            glUniform1f(specIntens, specIntensity);
            glUniform3fv(CamPos, 1, glm::value_ptr(CurCamera->cameraPos));
            glUniform3fv(specCol, 1, glm::value_ptr(specColr));
            glUniform1i(litTextureUniform, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, RedTexture.getId());
            model->render();
            glBindVertexArray(0);
            SceneIteration++;
        }
        checkGLError("scene pass");

        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(quadVAO);

        // currentInput always points at whichever texture holds the most
        // up-to-date processed image; it starts as the pristine scene render.
        unsigned int currentInput = sceneColorTex;

        // ---------------- WhiteBase ----------------
        // Runs first since it's a simple per-pixel remap of the raw scene.
        if (whiteEnabled) {
            glBeginQuery(GL_TIME_ELAPSED, watercolourQueries[0]);

            glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer01);
            glViewport(0, 0, g_width, g_height);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(WhiteBaseShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, currentInput);
            glUniform1i(glGetUniformLocation(WhiteBaseShader, "textureUniform"), 0);
            glUniform2f(glGetUniformLocation(WhiteBaseShader, "ScreenSize"), (float)g_width, (float)g_height);
            glUniform1f(glGetUniformLocation(WhiteBaseShader, "WhiteThreshold"), whiteThreshold);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glEndQuery(GL_TIME_ELAPSED);
            currentInput = textureColorbuffer;

            GLuint64 elapsed;
            glGetQueryObjectui64v(watercolourQueries[0], GL_QUERY_RESULT, &elapsed);
            passTimings[0] = elapsed / 1000000.0;
        } else {
            passTimings[0] = 0.0;
        }

        // ---------------- Bleed and EdgeDarken branch ----------------
        // Both passes read the same input independently, so the outline
        // detection always runs on crisp, unblurred geometry regardless of
        // how strong the bleed effect is.
        unsigned int branchInput = currentInput;

        if (bleedColoursEnabled) {
            glBeginQuery(GL_TIME_ELAPSED, watercolourQueries[2]);

            glBindFramebuffer(GL_FRAMEBUFFER, bleedOutFrameBuffer);
            glViewport(0, 0, g_width, g_height);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(BleedColourShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, branchInput);
            glUniform1i(glGetUniformLocation(BleedColourShader, "textureUniform"), 0);
            glUniform2f(glGetUniformLocation(BleedColourShader, "ScreenSize"), (float)g_width, (float)g_height);
            glUniform1f(glGetUniformLocation(BleedColourShader, "BleedStrength"), bleedStrength);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glEndQuery(GL_TIME_ELAPSED);

            GLuint64 elapsed;
            glGetQueryObjectui64v(watercolourQueries[2], GL_QUERY_RESULT, &elapsed);
            passTimings[2] = elapsed / 1000000.0;
        } else {
            passTimings[2] = 0.0;
        }

        if (edgeEnabled) {
            glBeginQuery(GL_TIME_ELAPSED, watercolourQueries[1]);

            glBindFramebuffer(GL_FRAMEBUFFER, edgeOutFrameBuffer);
            glViewport(0, 0, g_width, g_height);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(EdgeDarkenShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, branchInput);
            glUniform1i(glGetUniformLocation(EdgeDarkenShader, "textureUniform"), 0);
            glUniform2f(glGetUniformLocation(EdgeDarkenShader, "ScreenSize"), (float)g_width, (float)g_height);
            glUniform1f(glGetUniformLocation(EdgeDarkenShader, "EdgeDarkenStrength"), edgeStrength);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glEndQuery(GL_TIME_ELAPSED);

            GLuint64 elapsed;
            glGetQueryObjectui64v(watercolourQueries[1], GL_QUERY_RESULT, &elapsed);
            passTimings[1] = elapsed / 1000000.0;
        } else {
            passTimings[1] = 0.0;
        }

        // Merge the two branch outputs. If only one of the two ran, use its
        // result directly rather than compositing against an untouched image.
        if (bleedColoursEnabled && edgeEnabled) {
            glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer02);
            glViewport(0, 0, g_width, g_height);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(CompositeShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, bleedOutTex);
            glUniform1i(glGetUniformLocation(CompositeShader, "bleedTex"), 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, edgeOutTex);
            glUniform1i(glGetUniformLocation(CompositeShader, "edgeTex"), 1);
            glUniform1f(glGetUniformLocation(CompositeShader, "EdgeThreshold"), edgeCompositeThreshold);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            currentInput = textureColorbuffer2;
        }
        else if (bleedColoursEnabled) {
            currentInput = bleedOutTex;
        }
        else if (edgeEnabled) {
            currentInput = edgeOutTex;
        }
        // else: neither ran, currentInput is unchanged from before the branch

        // ---------------- Granulation and PaperTexture ----------------
        // Resume as a normal chain, each reading the previous result.
        bool laterToggles[2] = { granEnabled, paperEnabled };
        const char* laterPassNames[2] = { "Granulation", "PaperTexture" };
        unsigned int laterFBOs[2] = { frameBuffer01, frameBuffer02 };
        unsigned int laterTextures[2] = { textureColorbuffer, textureColorbuffer2 };
        int laterWriteIndex = 0;

        for (int i = 0; i < 2; i++) {
            int timingIndex = 3 + i; // 3 = Granulation, 4 = PaperTexture
            if (!laterToggles[i]) {
                passTimings[timingIndex] = 0.0;
                continue;
            }

            glBeginQuery(GL_TIME_ELAPSED, watercolourQueries[timingIndex]);

            glBindFramebuffer(GL_FRAMEBUFFER, laterFBOs[laterWriteIndex]);
            glViewport(0, 0, g_width, g_height);
            glClear(GL_COLOR_BUFFER_BIT);

            GLuint prog = ShaderPrograms.get(laterPassNames[i]);
            glUseProgram(prog);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, currentInput);
            glUniform1i(glGetUniformLocation(prog, "textureUniform"), 0);
            glUniform2f(glGetUniformLocation(prog, "ScreenSize"), (float)g_width, (float)g_height);
            if (i == 0) glUniform1f(glGetUniformLocation(prog, "GranulationStrength"), granulationStrength);

            glDrawArrays(GL_TRIANGLES, 0, 6);

            glEndQuery(GL_TIME_ELAPSED);

            currentInput = laterTextures[laterWriteIndex];
            laterWriteIndex = 1 - laterWriteIndex;

            GLuint64 elapsed;
            glGetQueryObjectui64v(watercolourQueries[timingIndex], GL_QUERY_RESULT, &elapsed);
            passTimings[timingIndex] = elapsed / 1000000.0;
        }

        for (int p = 0; p < 5; p++) {
            passHistory[p][historyWriteIndex] = (float)passTimings[p];
        }
        historyWriteIndex = (historyWriteIndex + 1) % historyLength;
        checkGLError("post process");

        // ---------------- CSV logging ----------------
        static bool wasLogging = false;
        if (loggingEnabled && !wasLogging) {
            csvLog.open("../CSVLogging/PerformanceLogs.csv");

            if (!csvLog.is_open())
                std::cout << "Failed to open CSV\n";
            else
                std::cout << "CSV opened successfully\n";

            csvLog << "Time,WhiteBase,EdgeDarken,Bleed,Granulation,PaperTexture\n";
            logStartTime = glfwGetTime();
        }
        if (!loggingEnabled && wasLogging) {
            csvLog.close();
        }
        wasLogging = loggingEnabled;

        if (loggingEnabled && csvLog.is_open()) {
            csvLog << (glfwGetTime() - logStartTime) << ","
                   << passTimings[0] << "," << passTimings[1] << ","
                   << passTimings[2] << "," << passTimings[3] << ","
                   << passTimings[4] << "\n";
        }

        // ---------------- Final composite to screen ----------------
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, g_width, g_height);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(RegularShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentInput);
        glUniform1i(glGetUniformLocation(RegularShader, "textureUniform"), 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        checkGLError("final blit");

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
        finishFrameTime = glfwGetTime();
        deltaTimeF = static_cast<float>(finishFrameTime - currentTime);
        currentTime = finishFrameTime;
        FrameRate();
    }

    glDeleteProgram(modelShaderProgram);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    delete CurCamera;
    glfwTerminate();
    return 0;
}