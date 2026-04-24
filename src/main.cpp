#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <learnopengl/shader_m.h>
//#include <learnopengl/model.h>
#include <learnopengl/filesystem.h>
#include <learnopengl/model_animation.h>

#include <vector>
#include <iostream>
#include <ctime>
#include <algorithm>
#include <learnopengl/animation.h>
#include <learnopengl/animator.h>

#include <irrKlang/irrKlang.h>
using namespace irrklang;

//////////////////////////////////////////////////////////////////////
/// SETTINGS
//////////////////////////////////////////////////////////////////////

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

int windowWidth = SCR_WIDTH;
int windowHeight = SCR_HEIGHT;

float deltaTime = 0;
float lastFrame = 0;
float winTimer = 0.0f;

glm::vec3 playerPos(0, 0, 0);
float playerSpeed = 5.0f;

float pitch = -20;
float cameraYaw = -90;
float modelYaw = 0.0f;

float lastX = SCR_WIDTH / 2;
float lastY = SCR_HEIGHT / 2;

bool firstMouse = true;
bool ePressed = false;

float cameraDistance = 3;

bool flashlightMode = false;

//////////////////////////////////////////////////////////////////////
/// GAME LOGIC
//////////////////////////////////////////////////////////////////////

int stage = 0;
bool hasAnomaly = false;
bool decisionMade = false;

int totalAbnormal = 3;
int foundCount = 0;

int wrongGuess = 0;
int maxWrong = 5;

bool gameWin = false;
bool gameLose = false;
bool resultPrint = false;

///////////////////////////
// ENUMS
///////////////////////////

struct Character {
    unsigned int TextureID;
    glm::ivec2 Size;
    glm::ivec2 Bearing;
    unsigned int Advance;
};

std::map<GLchar, Character> Characters;
std::map<GLchar, Character> CharactersWin;
unsigned int textVAO, textVBO;

enum GameState {
    MENU,
    PLAYING,
    WIN,
};

GameState gameState = MENU;

enum PlayerState {
    IDLE,
    WALKING,
    DANCE
};

enum FurnitureType
{
    PLANT,
    SHELF,
    CLOCK,
    CUP,
    TABLE,
    PICTURE,
    OTHER
};

struct Furniture
{
    Model* model;
    Model* fakeModel;

    glm::vec3 pos;
    glm::vec3 fakePos;

    // REAL transform
    float realScale;
    glm::vec3 realRotation;

    // FAKE transform
    float fakeScale;
    glm::vec3 fakeRotation;

    // CURRENT transform
    float scale;
    glm::vec3 rotation;

    // ORIGINAL transform
    float originalScale;
    glm::vec3 originalRotation;

    FurnitureType type;

    bool isFake = false;
    bool isExtra = false;

    bool abnormal = false;
    bool fixed = false;

    float abnormalRotationAngle = 0;
    glm::vec3 abnormalRotationAxis = glm::vec3(0, 1, 0);
};

struct ExtraObject
{
    Model* model;
    glm::vec3 pos;
    float scale;
    glm::vec3 rotation;

    bool active = false;
};

bool hasSpecial = false;

enum SpecialType
{
    NONE,
    FLASHLIGHT,
    CHAOS
};

SpecialType specialType = NONE;

enum StageMood
{
    NORMAL,
    HEAVY_ANOMALY,
    EXTRA_CHAOS,
    DARK_FLASHLIGHT
};


// All furniture in the room
std::vector<Furniture> furnitures;
std::vector<ExtraObject> extraObjects;
unsigned int rectVAO, rectVBO;

//////////////////////////////
// FUNCTION DECLARATIONS
//////////////////////////////
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    windowWidth = width;
    windowHeight = height;
    glViewport(0, 0, width, height);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    cameraDistance -= (float)yoffset * 0.1f; // zoom speed

    // clamp zoom so it doesn't go crazy
    if (cameraDistance < 2.0f) cameraDistance = 2.0f;
    if (cameraDistance > 4.0f) cameraDistance = 4.0f;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (gameState == MENU) {
        firstMouse = true;
    }

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    float sens = 0.15f;

    cameraYaw += xoffset * sens;
    pitch += yoffset * sens;

    if (pitch > 45) pitch = 45;
    if (pitch < -30) pitch = -30;
}

bool isCollidingWithWall(glm::vec3 pos)
{
    // Room boundaries
    float minX = -1.7f;
    float maxX = 1.7f;

    float minZ = -10.0f;
    float maxZ = 10.0f;

    // allow exit zones (doors)
    bool inBackDoor = (pos.z > 9.5f && abs(pos.x) < 0.5f);
    bool inFrontDoor = (pos.z < -9.4f && abs(pos.x) < 0.5f);

    // block walls EXCEPT doors
    if (pos.x < minX || pos.x > maxX ||
        pos.z < minZ || pos.z > maxZ)
    {
        if (inBackDoor || inFrontDoor)
            return false; // allow passing

        return true; // hit wall
    }

    return false;
}

glm::vec3 clampCamera(glm::vec3 camPos)
{
    float minX = -2.5f;
    float maxX = 2.5f;

    float minZ = -10.0f;
    float maxZ = 10.0f;

    camPos.x = glm::clamp(camPos.x, minX, maxX);
    camPos.z = glm::clamp(camPos.z, minZ, maxZ);

    return camPos;
}

void processInput(GLFWwindow* window, bool& isMoving)
{
    float speed = playerSpeed * deltaTime;

    glm::vec3 oldPos = playerPos;

    glm::vec3 forward;
    forward.x = cos(glm::radians(cameraYaw));
    forward.z = sin(glm::radians(cameraYaw));
    forward.y = 0;
    forward = glm::normalize(forward);

    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

    glm::vec3 moveDir(0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        moveDir += forward;
        isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        moveDir -= forward;
        isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        moveDir -= right;
        isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        moveDir += right;
        isMoving = true;
    }

    // movement + rotate model
    if (glm::length(moveDir) > 0.0f)
    {
        moveDir = glm::normalize(moveDir);

        playerPos += moveDir * speed;

        modelYaw = atan2(moveDir.x, moveDir.z);
    }

    // check collision AFTER moving
    if (isCollidingWithWall(playerPos))
    {
        playerPos = oldPos; // cancel movement
    }
}

bool isInside(float mx, float my, float x, float y, float w, float h)
{
    return mx >= x && mx <= x + w &&
        my >= y && my <= y + h;
}

//////////////////////////////////////////////////////////////////////////////
// RENDERING FUNCTIONS
/////////////////////////////////////////////////////////////////////////////
void LoadFont(std::map<GLchar, Character>& target, std::string path, int size)
{
    FT_Library ft;
    FT_Init_FreeType(&ft);

    FT_Face face;
    FT_New_Face(ft, path.c_str(), 0, &face);

    FT_Set_Pixel_Sizes(face, 0, size);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 0; c < 128; c++)
    {
        FT_Load_Char(face, c, FT_LOAD_RENDER);

        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0, GL_RED, GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        target.emplace(c, Character{
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            (unsigned int)face->glyph->advance.x
            });
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

void updateTextProjection(Shader& textShader, Shader& rectShader)
{
    glm::mat4 proj = glm::ortho(
        0.0f, (float)windowWidth,
        0.0f, (float)windowHeight
    );

    textShader.use();
    textShader.setMat4("projection", proj);

    rectShader.use();
    rectShader.setMat4("projection", proj);
}

glm::vec2 getUIScale()
{
    return glm::vec2(
        (float)windowWidth / SCR_WIDTH,
        (float)windowHeight / SCR_HEIGHT
    );
}

void RenderText(Shader& shader,
    std::map<GLchar, Character>& font,
    std::string text,
    float x, float y,
    float scale,
    glm::vec3 color)
{
    glDisable(GL_DEPTH_TEST);

    shader.use();
    shader.setVec3("textColor", color);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    for (auto c : text)
    {
        Character ch = font[c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        float vertices[6][4] = {
            { xpos, ypos + h, 0, 0 },
            { xpos, ypos,     0, 1 },
            { xpos + w, ypos, 1, 1 },

            { xpos, ypos + h, 0, 0 },
            { xpos + w, ypos, 1, 1 },
            { xpos + w, ypos + h, 1, 0 }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);

        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += (ch.Advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glEnable(GL_DEPTH_TEST); // restore
}

void DrawRect(Shader& shader, float x, float y, float w, float h,
    glm::vec3 color, float alpha, bool filled)
{
    float vertices[] = {
        x,     y,
        x + w, y,
        x + w, y + h,
        x,     y + h
    };

    glDisable(GL_DEPTH_TEST);

    shader.use();
    shader.setVec3("color", color);
    shader.setFloat("alpha", alpha);

    glBindVertexArray(rectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    if (filled)
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    else
        glDrawArrays(GL_LINE_LOOP, 0, 4);

    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

void DrawRectBorder(Shader& shader, float x, float y, float w, float h, float t, glm::vec3 color)
{
    DrawRect(shader, x, y + h - t, w, t, color, 1.0f, true);    // Top
    DrawRect(shader, x, y, w, t, color, 1.0f, true);            // Bottom
    DrawRect(shader, x, y, t, h, color, 1.0f, true);            // Left
    DrawRect(shader, x + w - t, y, t, h, color, 1.0f, true);    // Right
}

float GetTextWidth(std::map<GLchar, Character>& font, std::string text, float scale)
{
    float width = 0.0f;
    for (char c : text)
    {
        Character ch = font[c];
        width += (ch.Advance >> 6) * scale;
    }
    return width;
}
float uiScaleX() { return (float)windowWidth / SCR_WIDTH; }
float uiScaleY() { return (float)windowHeight / SCR_HEIGHT; }
float ui_Scale() { return std::min(uiScaleX(), uiScaleY()); }

float UI_X(float x) { return x * uiScaleX(); }
float UI_Y(float y) { return y * uiScaleY(); }
float UI_S(float s) { return s * ui_Scale(); }

/////////////////////////////////////////////////////////////////////////////
// OBLECT FUNCTIONS
/////////////////////////////////////////////////////////////////////////////

// Spawn furniture in the room
void spawnFurniture(Model& plant, Model& shelf, Model& clock_object, Model& cup, Model& table, Model& plantf, Model& shelff, Model& clock_objectf, Model& cupf, Model& tablef)
{
    // object - pos - scale - abnormal scale - abnormal rotation - abnormal rotation angle - TYPE
    furnitures.push_back({
        &plant,
        &plantf,
        {1.6,0.05,9.5},
        {1.6,0.5,9.5},

        1.7f, {0, 0, 0},   // REAL
        0.1f, {0, 0, 0},    // FAKE

        PLANT
        });
    //furnitures.push_back({ &plant,{1.6,0.05,9.5},1.7f,1,{0,1,0},0,PLANT });   // REAL PLANT
    //furnitures.push_back({ &plantf,{1.6,0.2,9.5},0.12f,1,{0,1,0},0,PLANT }); // FAKE PLANT

    furnitures.push_back({
        &shelf,
        &shelff,
        {-1.65,0,-2.75},
        {-1.65,0,-2.75},

        0.5f, {0, 180, 0},   // REAL
        1.4f, {0, 90, 0},    // FAKE

        SHELF
        });
    //furnitures.push_back({ &shelf,{-1.65,0,-2.75},0.5f,1,{0,1,0},0,SHELF }); // REAL SHELF
    //furnitures.push_back({ &shelff,{-1.65,0,-2.75},1.4f,1,{0,1,0},0,SHELF }); // Fake SHELF

    furnitures.push_back({
        &clock_object,
        &clock_objectf,
        {2,2.1,0},
        {2,2,0},

        1.15f, {0, 180, 0},   // REAL
        0.08f, {0, 270, 0},    // FAKE

        CLOCK
        });
    //furnitures.push_back({ &clock_object,{2,2,0},0.08f,1,{0,1,0},0,CLOCK }); // REAL CLOCK
    //furnitures.push_back({ &clock_objectf,{2,2,0},1.15f,1,{0,1,0},0,CLOCK });// FAKE CLOCK

    furnitures.push_back({
        &cup,
        &cupf,
        {0.9,0.95,9.0},
        {0.9,0.95,9.0},

        2.5f, {0, 0, 0},   // REAL
        2.7f, {270, 0, 0},    // FAKE

        CUP
        });
    //furnitures.push_back({ &cup,{2,0,0},3.0f,1,{0,1,0},0,CUP }); // REAL CUP
    //furnitures.push_back({ &cupf,{2,0,-2},4.0f,1,{0,1,0},0,CUP }); // FAKE CUP

    furnitures.push_back({
        &table,
        &tablef,
        {0.7,0,9.2},
        {0.7,0,9.2},

        1.2f, {0, 90, 0},   // REAL
        1.2f, {0, 0, 0},    // FAKE

        TABLE
        });

    //unused objects
    //furnitures.push_back({ &table,{-1.6,0,5},3.0f,1,{0,1,0},0,TABLE }); // REAL TABLE
    //furnitures.push_back({ &tablef,{-1.6,0,5},4.0f,1,{0,1,0},0,TABLE }); // FAKE TABLE

    //furnitures.push_back({ &lamp,{-3,0,2},0.05f,1,{0,1,0},0,LAMP });
    //furnitures.push_back({ &chair,{3,0,-1},0.01f,1,{0,1,0},0,CHAIR });
}

void initExtraObjects(Model& chair, Model& tableEx, Model& frame, Model& light, Model& light2)
{
    extraObjects.push_back({
         &chair,
         {0.9f, 0.0f, 8.0f},
         0.012f,
         {0,315,0},
         false
        });

    extraObjects.push_back({
        &tableEx,
        {1.5f, 1.0f, 4.0f},
        0.005f,
        {0,90,0},
        false
        });

    extraObjects.push_back({
    &frame,
    {0.04f, 1.25f, -9.945f},
    0.65f,
    {15,270,0},
    false
        });

    extraObjects.push_back({
    &light,
    {0.0f, 3.0f, 7.5f},
    0.5f,
    {0,90,0},
    false
        });

    extraObjects.push_back({
    &light2,
    {0.0f, 3.7f, 7.5f},
    0.01f,
    {0,90,0},
    false
        });

    extraObjects.push_back({
    &light2,
    {0.0f, 3.7f, 5.5f},
    0.01f,
    {0,90,0},
    false
        });

    extraObjects.push_back({
    &light2,
    {0.0f, 3.7f, 3.5f},
    0.01f,
    {0,90,0},
    false
        });

    extraObjects.push_back({
    &light2,
    {0.0f, 3.7f, 1.5f},
    0.01f,
    {0,90,0},
    false
        });

    extraObjects.push_back({
    &light2,
    {0.0f, 3.7f, -1.5f},
    0.01f,
    {0,90,0},
    false
        });

    extraObjects.push_back({
    &light2,
    {0.0f, 3.7f, -3.5f},
    0.01f,
    {0,90,0},
    false
        });

    extraObjects.push_back({
    &light2,
    {0.0f, 3.7f, -5.5f},
    0.01f,
    {0,90,0},
    false
        });

    extraObjects.push_back({
    &light2,
    {0.0f, 3.7f, -7.5f},
    0.01f,
    {0,90,0},
    false
        });
}

// Start new stage: reset all furniture and generate new abnormality
bool chance(float p)
{
    return (rand() / (float)RAND_MAX) < p;
}

void newStage(bool& chaosMode)
{
    decisionMade = false;

    playerPos = glm::vec3(0, 0, 8.5f);

    furnitures.erase(
        std::remove_if(furnitures.begin(), furnitures.end(),
            [](const Furniture& f)
            {
                return f.isExtra;
            }),
        furnitures.end()
    );

    // reset
    flashlightMode = false;
    chaosMode = false;

    for (auto& f : furnitures)
    {
        f.abnormal = false;
        f.fixed = false;

        f.scale = f.realScale;
        f.rotation = f.realRotation;

        f.originalScale = f.realScale;
        f.originalRotation = f.realRotation;

        f.isFake = false;
    }

    for (auto& e : extraObjects)
    {
        e.active = false;
    }

    // ===== SPECIAL EFFECT (independent) =====
    float difficulty = glm::clamp(stage / 12.0f, 0.0f, 1.0f);
    hasAnomaly = chance(0.5f + difficulty * 0.3f); // more anomaly later
    bool allowExtra = chance(0.3f + difficulty * 0.4f);
    bool allowSpecial = chance(0.2f + difficulty * 0.5f);

    if (allowSpecial)
    {
        if (rand() % 2 == 0)
            flashlightMode = true;
        else
            chaosMode = true;
    }

    hasAnomaly = rand() % 2;

    if (stage == 0)
    {
        hasAnomaly = false;
        flashlightMode = false;
        chaosMode = false;
        return;
    }

    if (hasAnomaly)
    {
        int type = rand() % 3;

        int anomalyCount = 1 + (difficulty > 0.6f ? rand() % 2 : 0);

        for (int a = 0; a < anomalyCount; a++)
        {
            int idx = rand() % furnitures.size();
            Furniture& f = furnitures[idx];

            switch (type)
            {
            case 0:
                // FAKE OBJECT
                f.isFake = true;
                break;

            case 1:
            {
                f.abnormal = true;

                float maxScale = 2.0f;

                switch (f.type)
                {
                case CUP:
                    maxScale = 3.0f;
                    break;
                case CLOCK:
                    maxScale = 1.25f;
                    break;
                case SHELF:
                    maxScale = 1.1f;
                    break;
                case PLANT:
                    maxScale = 1.25f;
                    break;
                case TABLE:
                    maxScale = 1.2f;
                    break;
                }

                f.scale = glm::clamp(f.scale * 1.5f, 0.5f, maxScale);

                auto randAngle = []() { return 30.0f + static_cast<float>(rand()) / RAND_MAX * 60.0f; };

                if (f.type == CUP || f.type == PLANT)
                {
                    if (rand() % 2)
                        f.rotation.x += randAngle();
                    else
                        f.rotation.z += randAngle();
                }
                else
                {
                    int axis = rand() % 3;
                    float angle = randAngle();

                    if (axis == 0) f.rotation.x += angle;
                    if (axis == 1) f.rotation.y += angle;
                    if (axis == 2) f.rotation.z += angle;
                }

                break;
            }
            case 2:
                // BOTH = more scary
                f.isFake = true;
                f.abnormal = true;
                break;
            }
        }

        // separate chance for extra
        if (allowExtra)
        {
            int count = 1 + rand() % (1 + (int)(difficulty * 3));

            for (int i = 0; i < count; i++)
            {
                int idx = rand() % extraObjects.size();
                extraObjects[idx].active = true;
            }
        }
    }

    //std::cout << "Stage: " << stage
    //    << (hasAnomaly ? " (Anomaly)\n" : " (Normal)\n");
}

/////////////////////////////////////////////////////////////////////////////
// GAME LOGIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////////

// Check if player is at back door
bool checkBackDoor()
{
    return playerPos.z > 9.9f;  // adjust location
}
bool checkEndWall()
{
    return playerPos.z < -9.9f; // adjust location
}

void resetGame(bool& chaosMode)
{
    stage = 0;

    gameWin = false;
    gameLose = false;
    decisionMade = false;
    hasAnomaly = false;

    playerPos = glm::vec3(0, 0, 8.5f);

    newStage(chaosMode);

    //std::cout << "Reset to Stage 0\n";
}

// Check if player is at decision point and make decision
void checkDecision(bool& chaosMode)
{
    // BACK = anomaly exists
    if (checkBackDoor())
    {
        decisionMade = true;

        if (hasAnomaly)
        {
            stage++;
            newStage(chaosMode);
        }
        else
        {
            stage = 0;
            newStage(chaosMode);
        }
    }

    // FORWARD = no anomaly
    if (checkEndWall())
    {
        decisionMade = true;

        if (!hasAnomaly)
        {
            stage++;
            newStage(chaosMode);
        }
        else
        {
            stage = 0;
            newStage(chaosMode);
        }
    }
}

///////////////////////////////////////////////////////////////////////////
// music function
///////////////////////////////////////////////////////////////////////////

void stopSound(ISound*& sound)
{
    if (sound)
    {
        sound->stop();
        sound = nullptr;
    }
}
void pauseSound(ISound* sound)
{
    if (sound)
        sound->setIsPaused(true);
}

void resumeSound(ISound* sound)
{
    if (sound)
        sound->setIsPaused(false);
}

/////////////////////////////////////////////////////////////////////////////////
// MAIN CODE
///////////////////////////////////////////////////////////////////////////////

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    GLFWwindow* window =
        glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Find Abnormalies", NULL, NULL);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwMakeContextCurrent(window);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);

    /////////////////////////////////////////////////////////////////////////////////////
    // SHADERS
    /////////////////////////////////////////////////////////////////////////////////////
    std::string vsPath = FileSystem::getPath("resources/shaders/1.model_loading.vs");
    std::string fsPath = FileSystem::getPath("resources/shaders/1.model_loading.fs");

    Shader staticShader(vsPath.c_str(), fsPath.c_str());

    std::string animVs = FileSystem::getPath("resources/shaders/anim_model.vs");
    std::string animFs = FileSystem::getPath("resources/shaders/anim_model.fs");

    Shader animShader(animVs.c_str(), animFs.c_str());

    // UI TEXT
    std::string textVs = FileSystem::getPath("resources/shaders/text.vs");
    std::string textFs = FileSystem::getPath("resources/shaders/text.fs");

    Shader textShader(textVs.c_str(), textFs.c_str());

    // rect buttons TEXT
    std::string rectVs = FileSystem::getPath("resources/shaders/rect.vs");
    std::string rectFs = FileSystem::getPath("resources/shaders/rect.fs");

    Shader rectShader(rectVs.c_str(), rectFs.c_str());

    //////////////////////////////////////////////////////////
    // Setup for text rendering

    glm::mat4 textProjection = glm::ortho(
        0.0f, (float)windowWidth,
        0.0f, (float)windowHeight
    );
    textShader.use();
    textShader.setMat4("projection", textProjection);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    LoadFont(Characters,
        FileSystem::getPath("resources/fonts/RubikWetPaint-Regular.ttf"),
        48);

    // WIN FONT (example: bold / horror / serif)
    LoadFont(CharactersWin,
        FileSystem::getPath("resources/fonts/Mogra-Regular.ttf"),
        72);

    // VAO/VBO
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);

    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // border for UI buttons
    rectShader.use();
    rectShader.setMat4("projection", textProjection);

    glGenVertexArrays(1, &rectVAO);
    glGenBuffers(1, &rectVBO);

    glBindVertexArray(rectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindVertexArray(0);


    ///////////////////////////////////////////////////////////////////////////
    // Load models
    ///////////////////////////////////////////////////////////////////////////

    // Objects
    // room
    Model room3(FileSystem::getPath("resources/objects/hallway/source/Hallway.obj"));
    // objects
    Model plantReal(FileSystem::getPath("resources/objects/plant_real/IndoorPotPlant3/Low-Poly+Plant_.obj"));
    Model shelfReal(FileSystem::getPath("resources/objects/shelf_real/uploads_files_6635102_bookcase.obj"));
    Model clockReal(FileSystem::getPath("resources/objects/clock_real/Clock_obj.obj"));
    Model cupReal(FileSystem::getPath("resources/objects/cup_real/cup_obj/cup.obj"));
    Model tableReal(FileSystem::getPath("resources/objects/table_real/uploads_files_3863534_RoundTable.obj"));
    // fake objects
    Model plantFake(FileSystem::getPath("resources/objects/plant_fake/PlantPot.obj"));
    Model shelfFake(FileSystem::getPath("resources/objects/shelf_fake/uploads_files_2724147_Bookshelf.obj"));
    Model clockFake(FileSystem::getPath("resources/objects/clock_fake/WallClock3.obj"));
    Model cupFake(FileSystem::getPath("resources/objects/cup_fake/Cupwithflowerpattern.obj"));
    Model tableFake(FileSystem::getPath("resources/objects/table_fake/uploads_files_3863534_RoundTable.obj"));
    // extra objects
    Model chair(FileSystem::getPath("resources/objects/chair/old_chair.obj"));
    Model tableEx(FileSystem::getPath("resources/objects/table/Desk OBJ.obj"));
    Model pictureEx(FileSystem::getPath("resources/objects/pictureEx/frame.obj"));
    Model lightEx(FileSystem::getPath("resources/objects/lightEx/Export_ON/ceiling.obj"));
    Model lightEx2(FileSystem::getPath("resources/objects/lightEx/Export_OFF/ceiling_OFF.obj"));
    // unused objects
    //Model room(FileSystem::getPath("resources/objects/room3/basement.obj"));
    //Model room2(FileSystem::getPath("resources/objects/studio/source/WhiteRoom.obj"));
    //Model player(FileSystem::getPath("resources/objects/player/Style human obj.obj"));
    //--------------------------------------------------------------------------
    // Player
    Model player(FileSystem::getPath("resources/objects/woman2/Ch41_nonPBR.dae"));
    //--
    Animation standing(FileSystem::getPath("resources/objects/woman2/Standing.dae"), &player);
    Animation walkAnimation_F(FileSystem::getPath("resources/objects/woman2/walking_1.dae"), &player);
    //--
    Animation danceAnimation1(FileSystem::getPath("resources/objects/woman2/Hip_Hop_Dancing.dae"), &player);
    Animation danceAnimation2(FileSystem::getPath("resources/objects/woman2/Hip_Hop_Dancing2.dae"), &player);
    Animation danceAnimation3(FileSystem::getPath("resources/objects/woman2/Gangnam_Style.dae"), &player);
    Animation danceAnimation4(FileSystem::getPath("resources/objects/woman2/Dancing_Running_Man.dae"), &player);
    Animation danceAnimation5(FileSystem::getPath("resources/objects/woman2/Arms_Hip_Hop_Dance.dae"), &player);
    Animation danceAnimation6(FileSystem::getPath("resources/objects/woman2/Jazz_Dancing.dae"), &player);
    Animation danceAnimation7(FileSystem::getPath("resources/objects/woman2/Twist_Dance.dae"), &player);
    Animation danceAnimation8(FileSystem::getPath("resources/objects/woman2/Ymca_Dance.dae"), &player);
    //--
    Animation poseAnimation1(FileSystem::getPath("resources/objects/woman2/Pose_1.dae"), &player);
    Animation poseAnimation2(FileSystem::getPath("resources/objects/woman2/Pose_2.dae"), &player);
    Animation poseAnimation3(FileSystem::getPath("resources/objects/woman2/Pose_3.dae"), &player);
    Animation poseAnimation4(FileSystem::getPath("resources/objects/woman2/Pose_4.dae"), &player);
    Animation poseAnimation5(FileSystem::getPath("resources/objects/woman2/Pose_5.dae"), &player);
    Animation poseAnimation6(FileSystem::getPath("resources/objects/woman2/Pose_6.dae"), &player);
    Animation poseAnimation7(FileSystem::getPath("resources/objects/woman2/Pose_7.dae"), &player);
    Animation poseAnimation8(FileSystem::getPath("resources/objects/woman2/Pose_8.dae"), &player);
    Animation poseAnimation9(FileSystem::getPath("resources/objects/woman2/Pose_9.dae"), &player);
    Animation poseAnimation10(FileSystem::getPath("resources/objects/woman2/Pose_10.dae"), &player);
    Animation poseAnimation11(FileSystem::getPath("resources/objects/woman2/Pose_11.dae"), &player);
    //--
    Animator animator(&standing);
    //--
    std::vector<Animation*> dances = {
    &danceAnimation1,
    &danceAnimation2,
    &danceAnimation3,
    &danceAnimation4,
    &danceAnimation5,
    &danceAnimation6,
    &danceAnimation7,
    &danceAnimation8
    };

    std::vector<Animation*> poses = {
    &poseAnimation1,
    &poseAnimation2,
    &poseAnimation3,
    &poseAnimation4,
    &poseAnimation5,
    &poseAnimation6,
    &poseAnimation7,
    &poseAnimation8,
    &poseAnimation9,
    &poseAnimation10,
    &poseAnimation11
    };

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Music
    ///////////////////////////////////////////////////////////////////////////////////////////

    ISoundEngine* engine = createIrrKlangDevice();

    if (!engine)
    {
        //std::cout << "ERROR: irrKlang not working\n";
        return 0;
    }

    std::string background_music = FileSystem::getPath("resources/audio/primalhousemusic_on_deep_background.mp3");

    ISound* bgMusic = engine->play2D(
        background_music.c_str(),
        true,   // loop
        false,  // start immediately
        true    // track this sound
    );

    if (bgMusic)
    {
        bgMusic->setVolume(0.1f);
    }

    // sound path string
    // background music (make chaos)
    std::string chaos_music1 = FileSystem::getPath("resources/audio/krasnoshchok-scary-horror-dark-music-404418.wav");
    std::string chaos_music2 = FileSystem::getPath("resources/audio/tim_kulig_free_music-games-of-octopi-148219.wav");
    std::string afk_music = FileSystem::getPath("resources/audio/sonican-guiding-star-babies-children-holiday-xmas-lullaby-396474.wav");
    std::string winner_music = FileSystem::getPath("resources/audio/music_for_videos-jazz-logo-164941.mp3");
    // effect
    std::string footSteps = FileSystem::getPath("resources/audio/footstep.mp3");
    std::string buttonClicked = FileSystem::getPath("resources/audio/freesound_crunchpixstudio-drop-coin.wav");
    std::string changePose = FileSystem::getPath("resources/audio/freesound_community-cartoon_wink_magic_sparkle.wav");
    // dance song
    std::string songDanceAnimation1 = FileSystem::getPath("resources/audio/sigmamusicart-jazz-lounge-relaxing-background-music.mp3");
    std::string songDanceAnimation2 = FileSystem::getPath("resources/audio/keyframe_audio-party-week.mp3");
    std::string songDanceAnimation3 = FileSystem::getPath("resources/audio/white_records-legacy-of-mozart-turkish-march.mp3");
    std::string songDanceAnimation4 = FileSystem::getPath("resources/audio/tunetank-sport-trap-music.mp3");
    std::string songDanceAnimation5 = FileSystem::getPath("resources/audio/sigmamusicart-amapiano-afrobeat.mp3");
    std::string songDanceAnimation6 = FileSystem::getPath("resources/audio/atlasaudio-piano-motivation.mp3");
    std::string songDanceAnimation7 = FileSystem::getPath("resources/audio/hitslab-brazil-rio-samba-carnival-music.mp3");
    std::string songDanceAnimation8 = FileSystem::getPath("resources/audio/fassounds-reggaeton-fiesta-upbeat-vlog-mexican-afrobeat.mp3");

    std::string songs[] = {
    songDanceAnimation1,
    songDanceAnimation2,
    songDanceAnimation3,
    songDanceAnimation4,
    songDanceAnimation5,
    songDanceAnimation6,
    songDanceAnimation7,
    songDanceAnimation8
    };

    ///////////////////////////////////////////////////////////////////////////////////////////
    // GAME VARIABLE
    ///////////////////////////////////////////////////////////////////////////////////////////

    // INIT STATE
    playerPos = glm::vec3(0, 0, 8.5f);
    PlayerState state = IDLE;
    float stepTimer = 0.0f;
    float idleTimer = 0.0f;

    bool isPosing = false;
    bool isAFK = false;
    int lastPoseIndex = -1;
    bool ePressedLastFrame = false;
    bool qPressedLastFrame = false;
    ISound* afkSound = nullptr;
    ISound* danceSound = nullptr;
    ISound* poseSound = nullptr;

    bool chaosMode = false;
    bool chaosStarted = false;
    bool stageStarted = false;
    ISound* chaosSound = nullptr;

    ISound* winSound = nullptr;
    bool winSoundPlayed = false;

    // menu 
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // game init setting
    spawnFurniture(plantReal, shelfReal, clockReal, cupReal, tableReal, plantFake, shelfFake, clockFake, cupFake, tableFake);
    initExtraObjects(chair, tableEx, pictureEx, lightEx, lightEx2);
    srand(time(0));
    newStage(chaosMode);
    //generateAbnormal();



    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        updateTextProjection(textShader, rectShader);

        bool isMoving = false;
        static bool wasMoving = false;

        // dance
        bool ePressedNow = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;

        if (ePressedNow && !ePressedLastFrame)
        {
            if (state != DANCE)
            {
                state = DANCE;

                stopSound(afkSound);
                stopSound(bgMusic);
                pauseSound(chaosSound);
                isAFK = false;
                idleTimer = 0.0f;

                int randIndex = rand() % dances.size();
                animator.PlayAnimation(dances[randIndex]);

                if (danceSound)
                {
                    danceSound->stop();
                    danceSound = nullptr;
                }

                danceSound = engine->play2D(
                    songs[randIndex].c_str(),
                    true,
                    false,
                    true
                );

                if (danceSound)
                    danceSound->setVolume(0.075f);
            }
        }

        ePressedLastFrame = ePressedNow;

        //pose
        bool qPressedNow = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;

        if (qPressedNow && !qPressedLastFrame)
        {
            isPosing = true;

            int poseCount = poses.size();
            int randIndex = rand() % poseCount;

            if (poseCount > 1)
            {
                while (randIndex == lastPoseIndex)
                    randIndex = rand() % poseCount;
            }

            lastPoseIndex = randIndex;

            animator.PlayAnimation(poses[randIndex]);

            if (poseSound)
            {
                poseSound->stop();
                poseSound = nullptr;
            }

            poseSound = engine->play2D(
                changePose.c_str(),
                false,
                false,
                true
            );

            if (poseSound)
                poseSound->setVolume(0.2f);
        }

        qPressedLastFrame = qPressedNow;

        // WALK
        if (gameState == PLAYING)
        {
            processInput(window, isMoving);
            if (isMoving && isPosing)
            {
                isPosing = false;
            }

            checkDecision(chaosMode);

            // WIN CONDITION
            if (stage > 11 && gameState != WIN)
            {
                gameWin = true;
                gameState = WIN;
                chaosMode = false;
				chaosStarted = false;

                // stop bg music
                stopSound(bgMusic);
                stopSound(chaosSound);
                stopSound(danceSound);
                stopSound(afkSound);
                stopSound(poseSound);
                stopSound(winSound);

                isAFK = false;
                idleTimer = 0.0f;

                if (!winSoundPlayed)
                {
                    winSound = engine->play2D(
                        winner_music.c_str(),
                        false, 
                        false,
                        true
                    );

                    if (winSound)
                        winSound->setVolume(0.075f);

                    winSoundPlayed = true;
                }
            }

            // CHAOS MODE
            // ENTER CHAOS
            if (chaosMode && !chaosStarted)
            {
                chaosStarted = true;

                stopSound(bgMusic);

                int r = rand() % 2;
                std::string chaosTrack = (r == 0) ? chaos_music1 : chaos_music2;

                chaosSound = engine->play2D(chaosTrack.c_str(), true, false, true);

                if (chaosSound)
                    chaosSound->setVolume(0.15f);
            }

            // EXIT CHAOS
            if (!chaosMode && chaosStarted)
            {
                chaosStarted = false;

                stopSound(chaosSound);

                if (!bgMusic)
                {
                    bgMusic = engine->play2D(background_music.c_str(), true, false, true);
                    if (bgMusic)
                        bgMusic->setVolume(0.1f);
                }
                else
                {
                    resumeSound(bgMusic);
                }
            }


            if (state == DANCE && isMoving)
            {
                // stop dance music
				stopSound(danceSound);
                stopSound(winSound);
                stopSound(afkSound);

                // resume bg music
                if (chaosMode)
                {
                    if (!chaosSound)
                    {
                        int r = rand() % 2;
                        std::string chaosTrack = (r == 0) ? chaos_music1 : chaos_music2;

                        chaosSound = engine->play2D(chaosTrack.c_str(), true, false, true);

                        if (chaosSound)
                            chaosSound->setVolume(0.15f);
                    }
                    else {
                        resumeSound(chaosSound);
                    }
                }
                else
                {
                    if (!bgMusic)
                    {
                        bgMusic = engine->play2D(background_music.c_str(), true, false, true);
                        if (bgMusic)
                            bgMusic->setVolume(0.1f);
                    }
                    else {
                        resumeSound(bgMusic);
                    }
                }

                // switch to walking
                state = WALKING;
                animator.PlayAnimation(&walkAnimation_F);
            }

            // afk 
            if (!isMoving && state != DANCE)
            {
                idleTimer += deltaTime;

                if (idleTimer > 180.0f && !isAFK)
                {
                    isAFK = true;

                    // STOP walking/dance
                    state = IDLE;

                    // play AFK pose (use existing pose for now)
                    animator.PlayAnimation(&poseAnimation10); // replace later with laying pose

                    // pause bg music
                    stopSound(danceSound);   
                    pauseSound(bgMusic);
                    pauseSound(chaosSound);

                    // play AFK sound
                    if (afkSound)
                    {
                        afkSound->stop();
                        afkSound = nullptr;
                    }

                    afkSound = engine->play2D(
                        afk_music.c_str(),
                        true,
                        false,
                        true
                    );

                    if (afkSound)
                        afkSound->setVolume(0.075f);
                }
            }
            else
            {
                idleTimer = 0.0f;

                if (isAFK)
                {
                    isAFK = false;

                    // STOP AFK sound
					stopSound(afkSound);

                    // resume bg music
                    if (!chaosMode)
                        resumeSound(bgMusic);
                    if (chaosMode)
                    {
                        if (chaosSound)
                            resumeSound(chaosSound);
                    }
                    else
                    {
                        if (bgMusic)
                            resumeSound(bgMusic);
                    }

                    // return to idle animation
                    animator.PlayAnimation(&standing);
                }
            }
        }
        if (state != DANCE)
        {
            if (isPosing)
            {
                // pose animation play
            }
            else if (isMoving)
            {
                if (state != WALKING)
                {
                    state = WALKING;
                    animator.PlayAnimation(&walkAnimation_F);

                    stepTimer = 0.5f;
                }

                stepTimer += deltaTime;

                if (stepTimer > 0.5f) // step interval
                {
                    ISound* step = engine->play2D(
                        footSteps.c_str(),
                        false,
                        false,
                        true
                    );

                    if (step)
                    {
                        step->setVolume(0.15f);
                    }

                    stepTimer = 0.0f;
                }
            }
            else
            {
                if (state != IDLE)
                {
                    state = IDLE;
                    animator.PlayAnimation(&standing);
                    stepTimer = 0.0f;
                }
            }
        }

        animator.UpdateAnimation(deltaTime);
        wasMoving = isMoving;

        //if (glfwGetKey(window, GLFW_KEY_E) == GLFW_RELEASE)
        //    ePressed = false;

        // Close Window
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClearColor(0.5, 0.5, 0.5, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //////////////////////////////////////////////////////////

        // view/projection transformations
        glm::vec3 front;

        front.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(pitch));

        front = glm::normalize(front);

        glm::vec3 camPos = playerPos - front * cameraDistance + glm::vec3(0, 1.9, 0);
        camPos = clampCamera(camPos);

        glm::mat4 view = glm::lookAt(camPos, playerPos + glm::vec3(0, 1.25, 0), glm::vec3(0, 1, 0));

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);

        if (flashlightMode)
        {
            glClearColor(0.02f, 0.02f, 0.02f, 1.0f); // darker

            float time = glfwGetTime();

            static float blinkTimer = 0.0f;
            static bool blinking = false;
            static int blinkCount = 0;

            blinkTimer -= deltaTime;

            float flicker = 0.6f + sin(time * 1.0f) * 0.1f;

            if (!blinking && blinkTimer <= 0.0f)
            {
                if (rand() % 4 == 0) // chance to start blink
                {
                    blinking = true;
                    blinkCount = 1 + rand() % 3; // 1–3 blinks chain
                    blinkTimer = 0.08f; // short blackout
                }
                else
                {
                    blinkTimer = 2.5f + (rand() % 3); // longer calm time
                }
            }

            if (blinking)
            {
                flicker = 0.0f; // FULL BLACKOUT

                camPos.x += ((rand() % 100) / 100.0f - 0.5f) * 0.02f;
                camPos.y += ((rand() % 100) / 100.0f - 0.5f) * 0.02f;

                if (blinkTimer <= 0.0f)
                {
                    blinkCount--;

                    if (blinkCount > 0)
                    {
                        // small gap before next blink
                        blinkTimer = 0.06f;
                    }
                    else
                    {
                        blinking = false;
                        blinkTimer = 2.0f + (rand() % 3);
                    }
                }
            }

            staticShader.use();
            staticShader.setBool("flashlightMode", true);
            staticShader.setVec3("lightPos", camPos);
            staticShader.setVec3("viewPos", camPos);
            staticShader.setVec3("lightDir", front);
            staticShader.setFloat("lightIntensity", flicker);

            animShader.use();
            animShader.setBool("flashlightMode", true);
            animShader.setVec3("lightPos", camPos);
            animShader.setVec3("viewPos", camPos);
            animShader.setVec3("lightDir", front);
            animShader.setFloat("lightIntensity", flicker);
        }
        else
        {
            glClearColor(0.5f, 0.5f, 0.5f, 1.0f);

            staticShader.use();
            staticShader.setBool("flashlightMode", false);
            staticShader.setFloat("lightIntensity", 1.0f); // restore normal light

            animShader.use();
            animShader.setBool("flashlightMode", false);
            animShader.setFloat("lightIntensity", 1.0f);
        }

        //----------------------------------------
        // RENDER PLAYER 
        //-----------------------------------------
        animShader.use();

        animShader.setMat4("view", view);
        animShader.setMat4("projection", proj);

        auto transforms = animator.GetFinalBoneMatrices();

        for (int i = 0; i < transforms.size(); ++i)
        {
            animShader.setMat4(
                "finalBonesMatrices[" + std::to_string(i) + "]",
                transforms[i]
            );
        }

        // PLAYER
        glm::mat4 anim_model = glm::mat4(1);
        anim_model = glm::translate(anim_model, playerPos);
        anim_model = glm::rotate(anim_model, modelYaw, glm::vec3(0, 1, 0));
        anim_model = glm::scale(anim_model, glm::vec3(1.15f));

        animShader.setMat4("model", anim_model);
        player.Draw(animShader);

        //----------------------------------------
        // RENDER OBJECTS WITHOUT ANIMATION
        //-----------------------------------------
        staticShader.use();

        staticShader.setMat4("view", view);
        staticShader.setMat4("projection", proj);

        // ROOM
        glm::mat4 model = glm::mat4(1);
        model = glm::scale(model, glm::vec3(1.25f));
        staticShader.setMat4("model", model);;

        room3.Draw(staticShader);

        // FURNITURE
        for (auto& f : furnitures)
        {
            glm::mat4 m = glm::mat4(1.0f);

            // choose correct transform
            glm::vec3 pos = f.isFake ? f.fakePos : f.pos;
            float scale;

            if (f.abnormal)
                scale = f.scale;
            else
                scale = f.isFake ? f.fakeScale : f.realScale;

            glm::vec3 rot;

            if (f.abnormal)
                rot = f.rotation;   // use modified rotation
            else
                rot = f.isFake ? f.fakeRotation : f.realRotation;

            // APPLY TRANSFORM ONCE
            m = glm::translate(m, pos);

            m = glm::rotate(m, glm::radians(rot.y), glm::vec3(0, 1, 0));
            m = glm::rotate(m, glm::radians(rot.x), glm::vec3(1, 0, 0));
            m = glm::rotate(m, glm::radians(rot.z), glm::vec3(0, 0, 1));

            m = glm::scale(m, glm::vec3(scale));

            staticShader.setMat4("model", m);

            Model* drawModel = f.isFake ? f.fakeModel : f.model;
            drawModel->Draw(staticShader);
        }

        if (!resultPrint) {
            if (gameWin) {
                //std::cout << "YOU WIN\n";
                resultPrint = true;
            }
            if (gameLose) {
                //std::cout << "YOU LOSE\n";
                resultPrint = true;
            }
        }

        for (auto& e : extraObjects)
        {
            if (!e.active) continue;

            glm::mat4 m = glm::mat4(1.0f);

            m = glm::translate(m, e.pos);
            m = glm::rotate(m, glm::radians(e.rotation.x), glm::vec3(1, 0, 0));
            m = glm::rotate(m, glm::radians(e.rotation.y), glm::vec3(0, 1, 0));
            m = glm::rotate(m, glm::radians(e.rotation.z), glm::vec3(0, 0, 1));
            m = glm::scale(m, glm::vec3(e.scale));

            staticShader.setMat4("model", m);
            e.model->Draw(staticShader);
        }

        // SWITCH UI
        // UI RENDER
        if (gameState == MENU)
        {
                stopSound(chaosSound);
                stopSound(danceSound);
                stopSound(afkSound);
                stopSound(poseSound);
                stopSound(winSound);

            if (!bgMusic) // start if not already playing
            {
                bgMusic = engine->play2D(background_music.c_str(), true, false, true);

                if (bgMusic)
                    bgMusic->setVolume(0.1f);
            }

            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

            double mx, my;
            glfwGetCursorPos(window, &mx, &my);

            glm::vec2 mouse(mx, windowHeight - my);

            bool click = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

            glm::vec2 uiScale = getUIScale();

            float btnW = 280;
            float btnH = 80;

            float startX = (windowWidth - btnW) /2;
            float startY = windowHeight/2;

            //float exitX = (windowWidth - btnW) * 0.5f;
            float exitX = (windowWidth - btnW) / 2;
            float exitY = windowHeight / 4;

            //bool hoverStart = isInside(mouse.x, mouse.y, startX, startY, btnW, btnH);
            //bool hoverExit = isInside(mouse.x, mouse.y, exitX, exitY, btnW, btnH);

            float borderThickness = 4.0f;

            // DRAW ORDER
            // background dim
            DrawRect(rectShader,
                0, 0,
                windowWidth,
                windowHeight,
                glm::vec3(0.0f),
                0.5f,
                true
            );

            // title
            std::string title = "Ha11way";
            float titleScale = 2.5f;

            float textWidth = GetTextWidth(Characters, title, titleScale);
            float titleX = (windowWidth - textWidth) / 2.0f;
            float titleY = (windowHeight/4)*3;

            RenderText(
                textShader,
                Characters,
                title,
                titleX,
                titleY,
                titleScale,
                glm::vec3(0.65f, 0.2f, 0.1f)
            );

            // START BUTTON
            float x = (windowWidth - btnW) / 2.0f;
            float y = windowHeight / 2.0f;
            float w = btnW;
            float h = btnH;

            bool hoverStart = isInside(mouse.x, mouse.y, x, y, w, h);

            glm::vec3 startBorderColor = hoverStart
                ? glm::vec3(1.0f, 0.8f, 0.2f)   // yellow
                : glm::vec3(1.0f);

            glm::vec3 startFillColor = hoverStart
                ? glm::vec3(1.0f, 0.8f, 0.2f)
                : glm::vec3(1.0f);


            DrawRect(rectShader, x, y, w, h, startFillColor, 0.6f, true);
            DrawRectBorder(rectShader, x, y, w, h, borderThickness, startBorderColor);

            std::string startText = "START";

            float textScale = 0.9f;
            float textStart = GetTextWidth(Characters, startText, textScale);

            float textX = x + (w - textStart) * 0.5f;
            float textY = y + (h * 0.5f) - 10.0f;

            RenderText(textShader, Characters,
                startText,
                textX, textY,
                textScale,
                glm::vec3(1.0f));

            // EXIT BUTTON
            //float x = (windowWidth - btnW) / 2.0f;
            //float y = windowHeight / 2.0f;
            //float w = btnW;
            //float h = btnH;

            y = windowHeight / 3.0f;

            bool hoverExit = isInside(mouse.x, mouse.y, x, y, w, h);

            glm::vec3 exitBorderColor = hoverExit
                ? glm::vec3(1.0f, 0.2f, 0.2f)   // red
                : glm::vec3(1.0f);

            glm::vec3 exitFillColor = hoverExit
                ? glm::vec3(1.0f, 0.2f, 0.2f)
                : glm::vec3(1.0f);

            DrawRect(rectShader, x, y, w, h, exitFillColor, 0.6f, true);
            DrawRectBorder(rectShader, x, y, w, h, borderThickness, exitBorderColor);

            std::string exitText = "EXIT";
            float textExit = GetTextWidth(Characters, exitText, textScale);

            textX = x + (w - textExit) * 0.5f;
            textY = y + (h * 0.5f) - 10.0f;

            RenderText(textShader, Characters,
                exitText,
                textX, textY,
                textScale,
                glm::vec3(1.0f));

            // CLICK
            if (hoverStart && click)
            {
                ISound* buttonClickedSound = engine->play2D(
                    buttonClicked.c_str(),
                    false,
                    false,
                    true
                );

                if (buttonClickedSound)
                {
                    buttonClickedSound->setVolume(0.2f);
                }

                resetGame(chaosMode);
                gameState = PLAYING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }

            if (hoverExit && click)
            {
                ISound* buttonClickedSound = engine->play2D(
                    buttonClicked.c_str(),
                    false,
                    false,
                    true
                );

                if (buttonClickedSound)
                {
                    buttonClickedSound->setVolume(0.2f);
                }

                glfwSetWindowShouldClose(window, true);
            }
        }
        else if (gameState == PLAYING)
        {
			glm::vec3 color = glm::vec3(1.0f);

            if (stage < 3) {
				color = glm::vec3(1.0f); // white
			}
			else if (stage < 6) {
				color = glm::vec3(0.95f, 0.87f, 0.32f); // yellow
			}
			else if (stage < 9) {
				color = glm::vec3(0.95f, 0.62f, 0.32f); // orange
            }
            else {
				color = glm::vec3(0.95f, 0.38f, 0.32f); // red
            }
            std::string s = "Stage: " + std::to_string(stage);
            RenderText(textShader, Characters, s, 20, windowHeight-40, 0.7f, color);
        }
        else if (gameState == WIN)
        {
            DrawRect(rectShader,
                0, 0,
                windowWidth,
                windowHeight,
                glm::vec3(0.0f),
                0.5f,
                true
            ); 

            std::string win_text = "!!! YOU WIN !!!";
            std::string win_subtext = "CONGRATULATIONS";
            float scale = 1.5f;
            float subscale = 0.7f;

            float textWidth = GetTextWidth(CharactersWin, win_text, scale);
            float subtextWidth = GetTextWidth(CharactersWin, win_subtext, subscale);

            float x = (windowWidth - textWidth) / 2.0f;
            float subx = (windowWidth - subtextWidth) / 2.0f;
            float y = (windowHeight / 3) * 2;
            float subY = windowHeight / 2;

            RenderText(textShader, CharactersWin, win_text, x, y, scale, glm::vec3(0.16f, 0.81f, 0.16f));
            RenderText(textShader, CharactersWin, win_subtext, subx, subY, subscale, glm::vec3(1.0f));

            winTimer += deltaTime;

            if (winTimer > 13.0f)
            {
                stopSound(bgMusic);
                stopSound(chaosSound);
                stopSound(danceSound);
                stopSound(afkSound);
                stopSound(poseSound);
                stopSound(winSound);

                gameState = MENU;
                winTimer = 0.0f;

                gameWin = false;
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
}