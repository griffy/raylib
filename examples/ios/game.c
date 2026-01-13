/*******************************************************************************************
*
*   raylib iOS Example - Bouncing Ball Game
*
*   This example uses standard raylib code - no iOS-specific changes needed!
*   Touch input is automatically mapped to mouse input.
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
} game = { 0 };

//----------------------------------------------------------------------------------
// Main Entry Point
// NOTE: On iOS, raylib.h automatically renames main() to GameInit() via a macro,
// so your game code works without modification on both desktop and iOS!
//----------------------------------------------------------------------------------
int main(void)
{
    // Initialize raylib
    // NOTE: On iOS, window size is ignored (always fullscreen)
    InitWindow(800, 600, "raylib iOS Example");
    SetTargetFPS(60);

    // Initialize game state
    game.ballPos = (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    game.ballVel = (Vector2){ 400.0f, 350.0f };
    game.ballRadius = 40.0f;
    game.ballColor = RED;
    game.score = 0;

    // Standard raylib game loop - works on iOS just like desktop!
    while (!WindowShouldClose())
    {
        // Update
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

        // Touch/tap to hit the ball (touch is mapped to mouse)
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

    CloseWindow();

    return 0;
}
