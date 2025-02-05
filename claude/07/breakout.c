#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_mixer.h>
#include <GL/glu.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 480
#define BRICK_WIDTH 80
#define BRICK_HEIGHT 20
#define NUM_BRICK_COLUMNS 10
#define NUM_BRICK_ROWS 5
#define PADDLE_WIDTH 100
#define PADDLE_HEIGHT 20
#define BALL_SIZE 15
#define BALL_SPEED 5.0f
#define INITIAL_LIVES 3

typedef struct {
    float x, y;
    float width, height;
    bool active;
    GLuint texture;
} GameObject;

typedef struct {
    float x, y;
    float dx, dy;
    float size;
    GLuint texture;
} Ball;

// Global variables
SDL_Window* window = NULL;
SDL_GLContext glContext;
GLuint backgroundTexture;
GLuint gameOverTexture;
GLuint winTexture;
GLuint fontTexture;
GLuint startScreenTexture;
GameObject paddle;
GameObject bricks[NUM_BRICK_ROWS][NUM_BRICK_COLUMNS];
Ball ball;
bool gameRunning = true;
int lives;
bool gameOver = false;
bool gameWon = false;
bool gameStarted = false;
int score = 0;
int consecutiveHits = 0;
Uint32 countdownStartTime = 0;
const int COUNTDOWN_DURATION = 3000; // 3 seconds in milliseconds
bool firstGame = true;

// Audio variables
Mix_Music *backgroundMusic = NULL;
Mix_Chunk *paddleHitSound = NULL;
Mix_Chunk *brickHitSound = NULL;
Mix_Chunk *gameOverSound = NULL;
Mix_Chunk *gameWonSound = NULL;

// Function declarations
bool initSDL(void);
void cleanup(void);
GLuint loadTexture(const char* filename);
void handleInput(void);
void updateGame(void);
void renderGame(void);
void initializeGame(void);
bool checkCollision(GameObject* obj, float x, float y, float size);
void renderTexturedQuad(float x, float y, float width, float height, GLuint texture);
float randomFloat(float min, float max);
void renderScore(void);
void drawDigit(int digit, float x, float y, float width, float height);
bool allBricksBroken(void);
void renderCountdown(int remainingTime);

float randomFloat(float min, float max) {
    return min + (rand() / (float)RAND_MAX) * (max - min);
}

GLuint loadTexture(const char* filename) {
    printf("Loading texture: %s\n", filename);

    SDL_Surface* surface = IMG_Load(filename);
    if (!surface) {
        printf("Failed to load texture %s: %s\n", filename, IMG_GetError());
        return 0;
    }
    printf("Texture loaded successfully: %s (%dx%d)\n", filename, surface->w, surface->h);

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    int mode = GL_RGB;
    if (surface->format->BytesPerPixel == 4) {
        mode = GL_RGBA;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, mode, surface->w, surface->h, 0, mode, GL_UNSIGNED_BYTE, surface->pixels);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("OpenGL error while creating texture: %d\n", error);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    SDL_FreeSurface(surface);
    return textureID;
}

bool initSDL(void) {
    printf("Initializing SDL...\n");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
        return false;
    }
    printf("SDL initialized successfully\n");

    int imgFlags = IMG_INIT_PNG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        printf("SDL_image initialization failed: %s\n", IMG_GetError());
        return false;
    }
    printf("SDL_image initialized successfully\n");

    // Initialize SDL_mixer with OGG support
    if (Mix_Init(MIX_INIT_OGG) != MIX_INIT_OGG) {
        printf("SDL_mixer OGG initialization failed: %s\n", Mix_GetError());
        return false;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("SDL_mixer audio opening failed: %s\n", Mix_GetError());
        return false;
    }
    printf("SDL_mixer initialized successfully with OGG support\n");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    window = SDL_CreateWindow("Breakout",
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            WINDOW_WIDTH, WINDOW_HEIGHT,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    if (!window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        return false;
    }

    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        printf("OpenGL context creation failed: %s\n", SDL_GetError());
        return false;
    }

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return true;
}

void initializeGame(void) {
    printf("Initializing game objects...\n");

    // Initialize random seed
    srand(time(NULL));

    // Initialize lives and score
    lives = INITIAL_LIVES;
    score = 0;
    consecutiveHits = 0;
    gameOver = false;
    gameWon = false;

    if (firstGame) {
        gameStarted = false;
    } else {
        gameStarted = true;
        countdownStartTime = SDL_GetTicks();
    }

    // Initialize paddle
    paddle.x = WINDOW_WIDTH / 2 - PADDLE_WIDTH / 2;
    paddle.y = WINDOW_HEIGHT - 40;
    paddle.width = PADDLE_WIDTH;
    paddle.height = PADDLE_HEIGHT;

    // Initialize ball position but don't set velocity yet
    ball.x = WINDOW_WIDTH / 2;
    ball.y = WINDOW_HEIGHT / 2;
    ball.dx = 0;
    ball.dy = 0;
    ball.size = BALL_SIZE;

    // Initialize bricks
    for (int row = 0; row < NUM_BRICK_ROWS; row++) {
        for (int col = 0; col < NUM_BRICK_COLUMNS; col++) {
            bricks[row][col].x = col * BRICK_WIDTH;
            bricks[row][col].y = row * BRICK_HEIGHT + 50;
            bricks[row][col].width = BRICK_WIDTH;
            bricks[row][col].height = BRICK_HEIGHT;
            bricks[row][col].active = true;
        }
    }

    printf("Game initialization complete.\n");
}

void renderTexturedQuad(float x, float y, float width, float height, GLuint texture) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(1, 0); glVertex2f(x + width, y);
    glTexCoord2f(1, 1); glVertex2f(x + width, y + height);
    glTexCoord2f(0, 1); glVertex2f(x, y + height);
    glEnd();
}

bool checkCollision(GameObject* obj, float x, float y, float size) {
    return (x + size > obj->x &&
            x - size < obj->x + obj->width &&
            y + size > obj->y &&
            y - size < obj->y + obj->height);
}

void handleInput(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            gameRunning = false;
        }
        else if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    gameRunning = false;
                    break;
                case SDLK_SPACE:
                    if (!gameStarted) {
                        gameStarted = true;
                        countdownStartTime = SDL_GetTicks();
                        printf("Game started. Countdown begins.\n");
                    } else if (gameOver || gameWon) {
                        firstGame = false;
                        initializeGame();
                        printf("Game restarted.\n");
                    }
                    break;
            }
        }
    }

    if (gameStarted && !gameOver && !gameWon) {
        const Uint8* keyState = SDL_GetKeyboardState(NULL);
        float paddleSpeed = 7.0f;

        if (keyState[SDL_SCANCODE_LEFT] || keyState[SDL_SCANCODE_A]) {
            paddle.x -= paddleSpeed;
            if (paddle.x < 0) paddle.x = 0;
        }
        if (keyState[SDL_SCANCODE_RIGHT] || keyState[SDL_SCANCODE_D]) {
            paddle.x += paddleSpeed;
            if (paddle.x > WINDOW_WIDTH - paddle.width) {
                paddle.x = WINDOW_WIDTH - paddle.width;
            }
        }
    }
}

bool allBricksBroken() {
    for (int row = 0; row < NUM_BRICK_ROWS; row++) {
        for (int col = 0; col < NUM_BRICK_COLUMNS; col++) {
            if (bricks[row][col].active) {
                return false;
            }
        }
    }
    return true;
}

void updateGame(void) {
    if (!gameStarted || gameOver || gameWon) {
        return;
    }

    Uint32 currentTime = SDL_GetTicks();
    if (currentTime - countdownStartTime < COUNTDOWN_DURATION) {
        // Countdown is still going, don't update ball position
        printf("Countdown: %d\n", (COUNTDOWN_DURATION - (currentTime - countdownStartTime)) / 1000 + 1);
        return;
    }

    // If this is the first update after countdown, initialize ball velocity
    if (ball.dx == 0 && ball.dy == 0) {
        ball.dx = randomFloat(-BALL_SPEED, BALL_SPEED);
        ball.dy = -BALL_SPEED;
        // Ensure the ball is not moving too slowly horizontally
        if (fabs(ball.dx) < BALL_SPEED / 2) {
            ball.dx = (ball.dx > 0) ? BALL_SPEED / 2 : -BALL_SPEED / 2;
        }
        printf("Ball velocity initialized: dx=%f, dy=%f\n", ball.dx, ball.dy);
    }

    // Update ball position
    ball.x += ball.dx;
    ball.y += ball.dy;
    printf("Ball position: x=%f, y=%f\n", ball.x, ball.y);

    // Ball collision with walls
    if (ball.x - ball.size < 0) {
        ball.x = ball.size;
        ball.dx = fabs(ball.dx);
    }
    if (ball.x + ball.size > WINDOW_WIDTH) {
        ball.x = WINDOW_WIDTH - ball.size;
        ball.dx = -fabs(ball.dx);
    }
    if (ball.y - ball.size < 0) {
        ball.y = ball.size;
        ball.dy = fabs(ball.dy);
    }

    // Ball out of bounds (bottom)
    if (ball.y + ball.size > WINDOW_HEIGHT) {
        lives--;
        printf("Life lost. Remaining lives: %d\n", lives);
        if (lives <= 0) {
            gameOver = true;
            printf("Game Over\n");
            if (gameOverSound) {
                Mix_PlayChannel(-1, gameOverSound, 0);
            }
        } else {
            // Reset ball position with random x velocity
            ball.x = WINDOW_WIDTH / 2;
            ball.y = WINDOW_HEIGHT / 2;
            ball.dx = randomFloat(-BALL_SPEED, BALL_SPEED);
            ball.dy = -BALL_SPEED;

            // Ensure the ball is not moving too slowly horizontally
            if (fabs(ball.dx) < BALL_SPEED / 2) {
                ball.dx = (ball.dx > 0) ? BALL_SPEED / 2 : -BALL_SPEED / 2;
            }

            // Reset paddle position
            paddle.x = WINDOW_WIDTH / 2 - PADDLE_WIDTH / 2;
            printf("Ball reset: dx=%f, dy=%f\n", ball.dx, ball.dy);
        }
    }

    // Ball collision with paddle
    if (checkCollision(&paddle, ball.x, ball.y, ball.size)) {
        // Calculate where on the paddle the ball hit
        float relativeIntersectX = (paddle.x + (paddle.width / 2)) - ball.x;
        float normalizedIntersect = relativeIntersectX / (paddle.width / 2);

        // Calculate new angle (between -60 and 60 degrees)
        float maxAngle = 60.0f * M_PI / 180.0f;
        float angle = normalizedIntersect * maxAngle;

        // Update ball velocity
        float speed = sqrt(ball.dx * ball.dx + ball.dy * ball.dy);
        ball.dx = speed * -sin(angle);
        ball.dy = -fabs(speed * cos(angle));

        // Move ball out of paddle to prevent multiple collisions
        ball.y = paddle.y - ball.size;

        // Reset consecutive hits when ball touches paddle
        consecutiveHits = 0;
        printf("Ball hit paddle. New velocity: dx=%f, dy=%f\n", ball.dx, ball.dy);

        if (paddleHitSound) {
            Mix_PlayChannel(-1, paddleHitSound, 0);
        }
    }

    // Ball collision with bricks
    bool hitBrick = false;
    for (int row = 0; row < NUM_BRICK_ROWS; row++) {
        for (int col = 0; col < NUM_BRICK_COLUMNS; col++) {
            if (bricks[row][col].active &&
                checkCollision(&bricks[row][col], ball.x, ball.y, ball.size)) {
                bricks[row][col].active = false;
                hitBrick = true;

                // Update score
                score += 10 + (consecutiveHits * 5);
                consecutiveHits++;
                printf("Brick hit. Score: %d, Consecutive hits: %d\n", score, consecutiveHits);

                // Determine which side of the brick was hit
                float brickCenterX = bricks[row][col].x + bricks[row][col].width / 2;
                float brickCenterY = bricks[row][col].y + bricks[row][col].height / 2;

                float dx = ball.x - brickCenterX;
                float dy = ball.y - brickCenterY;

                if (fabs(dx) * bricks[row][col].height > fabs(dy) * bricks[row][col].width) {
                    ball.dx = -ball.dx;
                } else {
                    ball.dy = -ball.dy;
                }

                if (brickHitSound) {
                    Mix_PlayChannel(-1, brickHitSound, 0);
                }

                printf("Ball deflected. New velocity: dx=%f, dy=%f\n", ball.dx, ball.dy);
                break;
            }
        }
        if (hitBrick) break;
    }

    // Check for win condition
    if (allBricksBroken()) {
        gameWon = true;
        printf("Game Won!\n");
        if (gameWonSound) {
            Mix_PlayChannel(-1, gameWonSound, 0);
        }
    }
}


void renderScore(void) {
    int tempScore = score;
    int digitCount = (tempScore == 0) ? 1 : log10(tempScore) + 1;
    float digitWidth = 20;
    float digitHeight = 30;
    float xPos = 10;
    float yPos = 10;

    for (int i = digitCount - 1; i >= 0; i--) {
        int digit = tempScore % 10;
        drawDigit(digit, xPos + i * (digitWidth + 5), yPos, digitWidth, digitHeight);
        tempScore /= 10;
    }
}

void drawDigit(int digit, float x, float y, float width, float height) {
    float textureX = (digit % 4) * 0.25f;
    float textureY = (digit / 4) * 0.25f;

    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(textureX, textureY); glVertex2f(x, y);
    glTexCoord2f(textureX + 0.25f, textureY); glVertex2f(x + width, y);
    glTexCoord2f(textureX + 0.25f, textureY + 0.25f); glVertex2f(x + width, y + height);
    glTexCoord2f(textureX, textureY + 0.25f); glVertex2f(x, y + height);
    glEnd();
}

void renderCountdown(int remainingTime) {
    char countdownText[2];
    snprintf(countdownText, sizeof(countdownText), "%d", remainingTime);

    float digitWidth = 60;
    float digitHeight = 90;
    float xPos = WINDOW_WIDTH / 2 - digitWidth / 2;
    float yPos = WINDOW_HEIGHT / 2 - digitHeight / 2;

    drawDigit(remainingTime, xPos, yPos, digitWidth, digitHeight);
}

void renderGame(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    if (!gameStarted && firstGame) {
        // Render start screen only for the first game
        renderTexturedQuad(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, startScreenTexture);
        printf("Rendering start screen\n");
    } else if (!gameOver && !gameWon) {
        // Render background
        renderTexturedQuad(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, backgroundTexture);

        // Render bricks
        for (int row = 0; row < NUM_BRICK_ROWS; row++) {
            for (int col = 0; col < NUM_BRICK_COLUMNS; col++) {
                if (bricks[row][col].active) {
                    renderTexturedQuad(bricks[row][col].x, bricks[row][col].y,
                                     bricks[row][col].width, bricks[row][col].height,
                                     bricks[row][col].texture);
                }
            }
        }

        // Render paddle
        renderTexturedQuad(paddle.x, paddle.y, paddle.width, paddle.height, paddle.texture);

        // Render ball
        renderTexturedQuad(ball.x - ball.size, ball.y - ball.size,
                          ball.size * 2, ball.size * 2, ball.texture);

        // Render remaining lives in top right corner
        for (int i = 0; i < lives; i++) {
            renderTexturedQuad(WINDOW_WIDTH - 40 - (i * 35), 10,
                             BALL_SIZE * 2, BALL_SIZE * 2, ball.texture);
        }

        // Render score
        renderScore();

        // Render countdown if necessary
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - countdownStartTime < COUNTDOWN_DURATION) {
            int remainingTime = (COUNTDOWN_DURATION - (currentTime - countdownStartTime)) / 1000 + 1;
            renderCountdown(remainingTime);
            printf("Rendering countdown: %d\n", remainingTime);
        }
    } else if (gameOver) {
        // Render game over screen
        renderTexturedQuad(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, gameOverTexture);
        printf("Rendering game over screen\n");
    } else if (gameWon) {
        // Render win screen
        renderTexturedQuad(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, winTexture);
        renderScore();
        printf("Rendering win screen\n");
    }

    SDL_GL_SwapWindow(window);
}

void cleanup(void) {
    printf("Cleaning up resources...\n");

    glDeleteTextures(1, &backgroundTexture);
    glDeleteTextures(1, &gameOverTexture);
    glDeleteTextures(1, &winTexture);
    glDeleteTextures(1, &fontTexture);
    glDeleteTextures(1, &startScreenTexture);
    glDeleteTextures(1, &paddle.texture);
    glDeleteTextures(1, &ball.texture);
    if (NUM_BRICK_ROWS > 0 && NUM_BRICK_COLUMNS > 0) {
        glDeleteTextures(1, &bricks[0][0].texture);
    }

    // Clean up audio resources
    if (backgroundMusic) {
        Mix_FreeMusic(backgroundMusic);
    }
    if (paddleHitSound) {
        Mix_FreeChunk(paddleHitSound);
    }
    if (brickHitSound) {
        Mix_FreeChunk(brickHitSound);
    }
    if (gameOverSound) {
        Mix_FreeChunk(gameOverSound);
    }
    if (gameWonSound) {
        Mix_FreeChunk(gameWonSound);
    }

    Mix_CloseAudio();
    Mix_Quit();

    if (glContext) {
        SDL_GL_DeleteContext(glContext);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    IMG_Quit();
    SDL_Quit();
    printf("Cleanup complete\n");
}

int main(int argc, char* argv[]) {
    printf("Starting Breakout game...\n");

    if (!initSDL()) {
        printf("Failed to initialize SDL. Exiting...\n");
        cleanup();
        return 1;
    }

    // Load all textures
    backgroundTexture = loadTexture("background.png");
    gameOverTexture = loadTexture("gameover.png");
    winTexture = loadTexture("youwin.png");
    fontTexture = loadTexture("font.png");
    startScreenTexture = loadTexture("startscreen.png");
    paddle.texture = loadTexture("paddle.png");
    ball.texture = loadTexture("ball.png");
    GLuint brickTexture = loadTexture("brick.png");

    if (!backgroundTexture || !gameOverTexture || !winTexture || !fontTexture ||
        !startScreenTexture || !paddle.texture || !ball.texture || !brickTexture) {
        printf("Failed to load textures. Exiting...\n");
        cleanup();
        return 1;
    }

    // Set brick textures
    for (int row = 0; row < NUM_BRICK_ROWS; row++) {
        for (int col = 0; col < NUM_BRICK_COLUMNS; col++) {
            bricks[row][col].texture = brickTexture;
        }
    }

    // Load audio files
    backgroundMusic = Mix_LoadMUS("background_music.ogg");
    paddleHitSound = Mix_LoadWAV("paddle_hit.ogg");
    brickHitSound = Mix_LoadWAV("brick_hit.ogg");
    gameOverSound = Mix_LoadWAV("game_over.ogg");
    gameWonSound = Mix_LoadWAV("game_won.ogg");

    if (!backgroundMusic || !paddleHitSound || !brickHitSound || !gameOverSound || !gameWonSound) {
        printf("Failed to load audio files. Continuing without audio.\n");
    }

    firstGame = true;
    initializeGame();
    printf("Game initialized, entering main loop...\n");

    // Start playing background music
    if (backgroundMusic) {
        Mix_PlayMusic(backgroundMusic, -1);  // -1 means loop indefinitely
    }

    while (gameRunning) {
        handleInput();
        updateGame();
        renderGame();
        SDL_Delay(16);  // Cap at roughly 60 FPS
    }

    printf("Game loop ended, cleaning up...\n");
    cleanup();
    return 0;
}
