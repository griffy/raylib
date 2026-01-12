/*******************************************************************************************
*
*   raylib iOS Example - Bouncing Ball Game
*
*   This demonstrates how to structure a raylib game for iOS.
*   Key differences from desktop:
*   - Uses callback-based rendering (SetUpdateCallback)
*   - Touch input mapped to mouse
*   - Always fullscreen
*
*   LICENSE: zlib/libpng (same as raylib)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>

//----------------------------------------------------------------------------------
// Game State
//----------------------------------------------------------------------------------
static struct {
    Vector2 ballPos;
    Vector2 ballVel;
    float ballRadius;
    Color ballColor;
    int score;
    bool initialized;
} game = { 0 };

//----------------------------------------------------------------------------------
// Game Update (called every frame)
//----------------------------------------------------------------------------------
void GameUpdate(void)
{
    // Initialize on first frame
    if (!game.initialized)
    {
        game.ballPos = (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
        game.ballVel = (Vector2){ 400.0f, 350.0f };
        game.ballRadius = 40.0f;
        game.ballColor = RED;
        game.score = 0;
        game.initialized = true;
    }

    float dt = GetFrameTime();
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    // Move ball
    game.ballPos.x += game.ballVel.x * dt;
    game.ballPos.y += game.ballVel.y * dt;

    // Bounce off walls
    if (game.ballPos.x >= screenWidth - game.ballRadius)
    {
        game.ballPos.x = screenWidth - game.ballRadius;
        game.ballVel.x *= -1.0f;
        game.score++;
    }
    if (game.ballPos.x <= game.ballRadius)
    {
        game.ballPos.x = game.ballRadius;
        game.ballVel.x *= -1.0f;
        game.score++;
    }
    if (game.ballPos.y >= screenHeight - game.ballRadius)
    {
        game.ballPos.y = screenHeight - game.ballRadius;
        game.ballVel.y *= -1.0f;
        game.score++;
    }
    if (game.ballPos.y <= game.ballRadius)
    {
        game.ballPos.y = game.ballRadius;
        game.ballVel.y *= -1.0f;
        game.score++;
    }

    // Touch/tap to hit the ball
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        Vector2 touchPos = GetMousePosition();
        float dx = touchPos.x - game.ballPos.x;
        float dy = touchPos.y - game.ballPos.y;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist < game.ballRadius * 3)
        {
            // Push ball away from touch
            if (dist > 0)
            {
                game.ballVel.x = -dx / dist * 500.0f;
                game.ballVel.y = -dy / dist * 500.0f;
            }

            // Change color
            game.ballColor = (Color){
                GetRandomValue(50, 255),
                GetRandomValue(50, 255),
                GetRandomValue(50, 255),
                255
            };

            game.score += 10;
        }
    }

    // Draw
    BeginDrawing();

    ClearBackground(RAYWHITE);

    // Ball with shadow
    DrawCircle((int)game.ballPos.x + 4, (int)game.ballPos.y + 4, game.ballRadius, Fade(BLACK, 0.2f));
    DrawCircleV(game.ballPos, game.ballRadius, game.ballColor);
    DrawCircleLines((int)game.ballPos.x, (int)game.ballPos.y, game.ballRadius, WHITE);

    // UI
    DrawText(TextFormat("Score: %d", game.score), 40, 40, 60, DARKGRAY);
    DrawText("Tap the ball!", screenWidth / 2 - 150, 120, 40, GRAY);

    // Touch indicator
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        Vector2 pos = GetMousePosition();
        DrawCircle((int)pos.x, (int)pos.y, 30, Fade(BLUE, 0.3f));
    }

    // FPS counter
    DrawFPS(40, screenHeight - 60);

    EndDrawing();
}

//----------------------------------------------------------------------------------
// Game Initialization (called from AppDelegate)
//----------------------------------------------------------------------------------
extern void SetUpdateCallback(void (*callback)(void));

void GameInit(void)
{
    // Initialize raylib
    // Note: Size is ignored on iOS - always fullscreen
    InitWindow(0, 0, "raylib iOS Example");

    // Set target FPS
    SetTargetFPS(60);

    // Set the update callback - this is called every frame
    SetUpdateCallback(GameUpdate);
}
