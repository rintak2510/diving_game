//=== STEP 1 & STEP 2: 初期表示・モデル・カメラ切替対応 + X軸回転対応 + 回転ジャンプ条件処理 ===

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <GL/glew.h>
#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <iostream>
#include "common.h"
#endif

// シンプルなプレイヤー表現
struct Player {
    glm::vec3 position = glm::vec3(0.0f, 2.875f, 0.0f); //現状は足が2.0fになるように設定
    float rotationX = 0.0f;
};

std::vector<float> modelVertices;
std::vector<float> modelNormals;


static const std::string MESH_FILE = std::string(DATA_DIRECTORY) + "male.obj";

// 状態定義
enum class GameState {
    Gauge_Height,
    Gauge_Spin,
    Jumping,
    Falling,
    Result
};

GameState currentState = GameState::Gauge_Height;
Player player;

float jumpGauge = 0.0f;
float spinTimer = 0.0f;
int spinCount = 0;
float velocityY = 0.0f;
float velocityZ = 0.2f;
float playerRotationSpeed = 0.0f;
float jumpElapsedTime = 0.0f;
float airtime = 0.0f;
float totalRotation = 0.0f;
const float waterY = 0.0f;

bool jumpSuccess = true;
bool jumpStarted = false;

bool loadModel(const std::string& filename) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename.c_str(), nullptr, true);
    modelVertices.clear();
    modelNormals.clear();
    
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            float vx = attrib.vertices[3 * index.vertex_index + 0];
            float vy = attrib.vertices[3 * index.vertex_index + 1];
            float vz = attrib.vertices[3 * index.vertex_index + 2];

            if (vy < minY) minY = vy;
            if (vy > maxY) maxY = vy;

            modelVertices.push_back(attrib.vertices[3 * index.vertex_index + 0]);
            modelVertices.push_back(attrib.vertices[3 * index.vertex_index + 1]);
            modelVertices.push_back(attrib.vertices[3 * index.vertex_index + 2]);

            if (!attrib.normals.empty()) {
                modelNormals.push_back(attrib.normals[3 * index.normal_index + 0]);
                modelNormals.push_back(attrib.normals[3 * index.normal_index + 1]);
                modelNormals.push_back(attrib.normals[3 * index.normal_index + 2]);
            } else {
                modelNormals.push_back(0.0f); // fallback
                modelNormals.push_back(1.0f);
                modelNormals.push_back(0.0f);
            }
        }
    }
    std::cout << "Y Range: min = " << minY << ", max = " << maxY << ", height = " << (maxY - minY) << std::endl;

    return true;
}

std::map<std::string, GLuint> resultTextures;

GLuint loadTexture(const std::string& path) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true); // PNGは上下反転しているので、読み込み時に反転する
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) {
        std::cerr << "Failed to load PNG: " << path << std::endl;
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return textureID;
}

//=== 描画関数 ===
void drawCube(float scale = 1.0f) {
    glScalef(scale, scale, scale);
    glBegin(GL_QUADS);
    float hs = 0.5f;
    // Front
    glVertex3f(-hs, -hs, hs); glVertex3f(hs, -hs, hs);
    glVertex3f(hs, hs,  hs); glVertex3f(-hs, hs,  hs);
    // Back
    glVertex3f(-hs, -hs, -hs); glVertex3f(-hs, hs, -hs);
    glVertex3f(hs, hs, -hs); glVertex3f(hs, -hs, -hs);
    // Left
    glVertex3f(-hs, -hs, -hs); glVertex3f(-hs, -hs, hs);
    glVertex3f(-hs, hs,  hs); glVertex3f(-hs, hs, -hs);
    // Right
    glVertex3f(hs, -hs, -hs); glVertex3f(hs, hs, -hs);
    glVertex3f(hs, hs,  hs); glVertex3f(hs, -hs, hs);
    // Top
    glVertex3f(-hs, hs, -hs); glVertex3f(-hs, hs, hs);
    glVertex3f(hs, hs,  hs); glVertex3f(hs, hs, -hs);
    // Bottom
    glVertex3f(-hs, -hs, -hs); glVertex3f(hs, -hs, -hs);
    glVertex3f(hs, -hs,  hs); glVertex3f(-hs, -hs, hs);
    glEnd();
}

void drawModel(float scale = 1.0f) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPushMatrix();
    glScalef(scale, scale, scale);
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < modelVertices.size(); i += 3) {
        float vy = modelVertices[i + 1] * scale + player.position.y;
        float alpha = (vy < waterY) ? 0.3f : 1.0f;  // 下に潜ってたら薄くする
        glColor4f(1.0f, 0.8f, 0.2f, alpha);         // 半透明の色設定
        glNormal3f(modelNormals[i], modelNormals[i+1], modelNormals[i+2]);
        glVertex3f(modelVertices[i], modelVertices[i+1], modelVertices[i+2]);
    }
    glEnd();
    glPopMatrix();
    glDisable(GL_BLEND);
}

void drawPlayer() {
    glPushMatrix();
    glTranslatef(player.position.x, player.position.y, player.position.z);
    glRotatef(player.rotationX, 1, 0, 0);
    glColor3f(1.0f, 0.8f, 0.2f);
    drawModel(0.50f);
    glPopMatrix();
}

void drawPlatform() {
    glPushMatrix();
    glTranslatef(0.0f, 2.8f, -0.5f);
    glScalef(2.0f, 0.2f, 1.0f);
    glColor3f(0.6f, 0.4f, 0.2f);
    drawCube();
    glPopMatrix();
}

void drawGaugeUI(int width, int height) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width, 0, height, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    if (currentState == GameState::Gauge_Height) {
        float gaugeWidth = (jumpGauge / 100.0f) * 200.0f;
        glColor3f(1.0f, 0.2f, 0.2f);
        glBegin(GL_QUADS);
        glVertex2f(50, 50);
        glVertex2f(50 + gaugeWidth, 50);
        glVertex2f(50 + gaugeWidth, 70);
        glVertex2f(50, 70);
        glEnd();
    }

    if (currentState == GameState::Gauge_Spin) {
        int currentZone = spinCount;
        float blockSize = 20.0f;
        for (int i = 0; i < 6; ++i) {
            if (i == currentZone) glColor3f(1, 1, 0);
            else glColor3f(0.3f, 0.3f, 0.3f);

            glBegin(GL_QUADS);
            glVertex2f(50 + i * (blockSize + 5), 100);
            glVertex2f(50 + i * (blockSize + 5) + blockSize, 100);
            glVertex2f(50 + i * (blockSize + 5) + blockSize, 120);
            glVertex2f(50 + i * (blockSize + 5), 120);
            glEnd();
        }
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void drawWaterSurface() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.2f, 0.5f, 0.8f, 0.6f);
    glBegin(GL_QUADS);
    glVertex3f(-20.0f, 0.0f, -20.0f);
    glVertex3f( 20.0f, 0.0f, -20.0f);
    glVertex3f( 20.0f, 0.0f,  20.0f);
    glVertex3f(-20.0f, 0.0f,  20.0f);
    glEnd();
    glDisable(GL_BLEND);
}

void drawResultImage(GLuint textureID, int screenWidth, int screenHeight) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, screenWidth, 0, screenHeight, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(1, 0); glVertex2f(screenWidth, 0);
    glTexCoord2f(1, 1); glVertex2f(screenWidth, screenHeight);
    glTexCoord2f(0, 1); glVertex2f(0, screenHeight);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void setupCamera() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float cameraY = std::max(player.position.y, 0.0f); // y < 0 のときは 0 固定！
    glm::vec3 eye = glm::vec3(player.position.x, cameraY + 1.0f, player.position.z - 3.0f);
    glm::vec3 center = glm::vec3(player.position.x, cameraY, player.position.z);

    gluLookAt(eye.x, eye.y, eye.z,
              player.position.x, player.position.y, player.position.z,
              0, 1, 0);
}

void updateGame(GLFWwindow* window, float deltaTime) {
    switch (currentState) {
        case GameState::Gauge_Height:
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                jumpGauge += deltaTime * 50.0f;
                if (jumpGauge > 100.0f) jumpGauge = 100.0f;
            }
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE && jumpGauge > 0.0f) {
                currentState = GameState::Gauge_Spin;
                spinTimer = 0.0f;
                spinCount = 0;
            }
            break;

        case GameState::Gauge_Spin: {
            spinTimer += deltaTime;
            int zone = static_cast<int>(spinTimer / 0.6f);
            spinCount = zone % 6;

            if (zone > 6 || glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                jumpSuccess = true;
                float upperLimit = 305.0f;
                float lowerLimitPerSpin = 12.0f;

                if (jumpGauge * spinCount > upperLimit)
                    jumpSuccess = false;
                if (jumpGauge < lowerLimitPerSpin * spinCount)
                    jumpSuccess = false;

                currentState = GameState::Jumping;
                velocityY = jumpSuccess ? jumpGauge * 0.05f : 0.0f;

                // ← airtimeとtotalRotationをジャンプ開始時に確定させる
                if (jumpSuccess) {
                    float g = 9.8f;
                    airtime = 2.0f * velocityY / g;
                    totalRotation = 360.0f * spinCount;
                    player.rotationX = 0.0f;
                    jumpElapsedTime = 0.0f; // 初期化
                } else {
                    playerRotationSpeed = 0.0f;
                }
            }
            break;
        }

        case GameState::Jumping: {
            if (!jumpStarted) {
                airtime = 2.0f * velocityY / 9.8f;
                totalRotation = 360.0f * spinCount;
                jumpElapsedTime = 0.0f;
                jumpStarted = true;
            }

            velocityY -= deltaTime * 9.8f;
            player.position.y += velocityY * deltaTime;
            player.position.z += velocityZ * deltaTime;

            if (jumpSuccess) {
                jumpElapsedTime += deltaTime;
                float t = std::min(jumpElapsedTime, airtime);
                player.rotationX = totalRotation * (t / airtime);
            }

            // 落下に転じたらFallingへ遷移
            if (velocityY < 0.0f) {
                currentState = GameState::Falling;
            }

            break;
        }

        case GameState::Falling: {
            velocityY -= deltaTime * 9.8f;
            player.position.y += velocityY * deltaTime;
            player.position.z += velocityZ * deltaTime;

            if (jumpSuccess) {
                jumpElapsedTime += deltaTime;
                float t = std::min(jumpElapsedTime, airtime);
                player.rotationX = totalRotation * (t / airtime);
            }

            if (player.position.y <= -1.0f) {
                player.position.y = -1.0f;
                currentState = GameState::Result;
                std::cout << "Result: " << (jumpSuccess ? (spinCount == 0 ? "Fail" : std::to_string(spinCount) + "回転") : "回転失敗") << std::endl;
            }

            break;
        }

        case GameState::Result:
            if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
                player.position = glm::vec3(0.0f, 2.875f, 0.0f);
                player.rotationX = 0.0f;
                jumpGauge = 0.0f;
                spinCount = 0;
                velocityY = 0.0f;
                jumpSuccess = true;
                playerRotationSpeed = 0.0f;
                currentState = GameState::Gauge_Height;
            }
            break;
    }
}

int main() {
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL Dive Game", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!loadModel(MESH_FILE)) {
        std::cerr << "モデルの読み込みに失敗しました" << std::endl;
        return -1;
    }

    resultTextures["Failure"]    = loadTexture("assets/failure.png");
    resultTextures["Single"]     = loadTexture("assets/single_spin.png");
    resultTextures["Double"]     = loadTexture("assets/double_spin.png");
    resultTextures["Triple"]     = loadTexture("assets/triple_spin.png");
    resultTextures["Quad"]       = loadTexture("assets/quad_spin.png");
    resultTextures["Quintuple"]  = loadTexture("assets/quintple_spin.png");

    glewExperimental = GL_TRUE;
    glewInit();

    glEnable(GL_DEPTH_TEST);

    float lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        updateGame(window, deltaTime);

        glClearColor(0.6f, 0.85f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        setupCamera();

        if (currentState == GameState::Gauge_Height || currentState == GameState::Gauge_Spin || currentState == GameState::Jumping) {
            drawPlatform();
        }
        drawWaterSurface();
        drawPlayer();

        if (currentState == GameState::Result) {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            if (!jumpSuccess) {
                drawResultImage(resultTextures["Failure"], width, height);
            } else {
                switch (spinCount) {
                    case 1: drawResultImage(resultTextures["Single"], width, height); break;
                    case 2: drawResultImage(resultTextures["Double"], width, height); break;
                    case 3: drawResultImage(resultTextures["Triple"], width, height); break;
                    case 4: drawResultImage(resultTextures["Quad"], width, height); break;
                    case 5: drawResultImage(resultTextures["Quintuple"], width, height); break;
                    default: drawResultImage(resultTextures["Failure"], width, height); break;
                }
            }
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        drawGaugeUI(width, height);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
