/**********************************************************************************************
*
*   rcore_ios - Functions to manage window, graphics device and inputs
*
*   PLATFORM: IOS
*       - iOS (iPhone, iPad) via ANGLE for OpenGL ES
*
*   LIMITATIONS:
*       - Single window only (iOS limitation)
*       - No physical keyboard by default (external keyboard requires additional implementation)
*       - Always fullscreen (iOS limitation)
*
*   POSSIBLE IMPROVEMENTS:
*       - External keyboard support via UIKeyInput
*       - Game controller support via GCController framework
*       - Haptic feedback via UIFeedbackGenerator
*
*   CONFIGURATION:
*       #define RCORE_PLATFORM_IOS_ANGLE
*           Use ANGLE for OpenGL ES to Metal translation (recommended for iOS 12+)
*
*   DEPENDENCIES:
*       - ANGLE: OpenGL ES to Metal translation layer (libEGL, libGLESv2)
*       - UIKit: iOS user interface framework
*       - QuartzCore: Core Animation (CADisplayLink, CAEAGLLayer)
*       - gestures: Gestures system for touch-ready devices
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5) and contributors
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

// Compile with: -x objective-c (or rename to .m)

#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <EGL/egl.h>                    // ANGLE EGL interface
#include <EGL/eglext.h>

#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>                     // For chdir()
#include <pthread.h>                    // For game thread

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Touch event for thread-safe queue
typedef struct {
    int action;     // 0=began, 1=moved, 2=ended, 3=cancelled
    int index;      // Touch index (0-9)
    float x, y;     // Touch position in points
} TouchEvent;

// Thread-safe touch event queue
#define TOUCH_QUEUE_SIZE 64
typedef struct {
    TouchEvent events[TOUCH_QUEUE_SIZE];
    int head;                           // Read position
    int tail;                           // Write position
    pthread_mutex_t mutex;              // Thread safety
} TouchEventQueue;

typedef struct {
    // UIKit objects (stored as __unsafe_unretained to avoid ARC issues in struct)
    UIWindow *window;
    UIViewController *viewController;
    UIView *view;
    CADisplayLink *displayLink;

    // EGL/ANGLE context
    EGLDisplay device;                  // Native display device (EGL display)
    EGLSurface surface;                 // Surface to draw on (EGL surface)
    EGLContext context;                 // Graphic context (EGL context)
    EGLConfig config;                   // Graphic config (EGL config)

    // Threading
    pthread_t gameThread;               // Game loop thread
    bool gameThreadRunning;             // Game thread is active
    TouchEventQueue touchQueue;         // Thread-safe touch event queue

    // State
    bool appActive;                     // App is in foreground
    bool contextRebindRequired;         // Context needs rebinding after background
    bool frameReady;                    // Frame is ready (set by CADisplayLink callback)
} PlatformData;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
extern CoreData CORE;                   // Global CORE state context

static PlatformData platform = { 0 };   // Platform specific data

//----------------------------------------------------------------------------------
// Local Variables Definition
//----------------------------------------------------------------------------------
static void (*gameUpdateCallback)(void) = NULL;     // User game loop callback

// Game entry point (defined by user's main.c via #define main GameInit)
extern void GameInit(void);

//----------------------------------------------------------------------------------
// Touch Queue Functions (thread-safe)
//----------------------------------------------------------------------------------

// Initialize the touch event queue
static void InitTouchQueue(void)
{
    platform.touchQueue.head = 0;
    platform.touchQueue.tail = 0;
    pthread_mutex_init(&platform.touchQueue.mutex, NULL);
}

// Push a touch event to the queue (called from main thread)
static void PushTouchEvent(int action, int index, float x, float y)
{
    pthread_mutex_lock(&platform.touchQueue.mutex);

    int nextTail = (platform.touchQueue.tail + 1) % TOUCH_QUEUE_SIZE;
    if (nextTail != platform.touchQueue.head)  // Check if queue is full
    {
        TouchEvent *event = &platform.touchQueue.events[platform.touchQueue.tail];
        event->action = action;
        event->index = index;
        event->x = x;
        event->y = y;
        platform.touchQueue.tail = nextTail;
    }

    pthread_mutex_unlock(&platform.touchQueue.mutex);
}

// Process a single touch event (updates CORE.Input)
static void ProcessTouchEvent(TouchEvent *event)
{
    int action = event->action;
    int index = event->index;
    float x = event->x;
    float y = event->y;

    if (index >= MAX_TOUCH_POINTS) return;

    switch (action)
    {
        case 0: // Touch began
        {
            CORE.Input.Touch.position[index] = (Vector2){ x, y };
            CORE.Input.Touch.currentTouchState[index] = 1;
            CORE.Input.Touch.pointId[index] = index;
            CORE.Input.Touch.pointCount++;
            if (CORE.Input.Touch.pointCount > MAX_TOUCH_POINTS)
                CORE.Input.Touch.pointCount = MAX_TOUCH_POINTS;

            // Map to mouse for single touch
            if (index == 0)
            {
                CORE.Input.Mouse.currentPosition = (Vector2){ x, y };
                CORE.Input.Mouse.currentButtonState[MOUSE_BUTTON_LEFT] = 1;
            }

#if defined(SUPPORT_GESTURES_SYSTEM)
            GestureEvent gestureEvent = { 0 };
            gestureEvent.touchAction = TOUCH_ACTION_DOWN;
            gestureEvent.pointCount = CORE.Input.Touch.pointCount;
            for (int i = 0; i < gestureEvent.pointCount; i++)
            {
                gestureEvent.pointId[i] = CORE.Input.Touch.pointId[i];
                gestureEvent.position[i] = CORE.Input.Touch.position[i];
            }
            ProcessGestureEvent(gestureEvent);
#endif
        } break;

        case 1: // Touch moved
        {
            CORE.Input.Touch.position[index] = (Vector2){ x, y };

            if (index == 0)
            {
                CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
                CORE.Input.Mouse.currentPosition = (Vector2){ x, y };
            }

#if defined(SUPPORT_GESTURES_SYSTEM)
            GestureEvent gestureEvent = { 0 };
            gestureEvent.touchAction = TOUCH_ACTION_MOVE;
            gestureEvent.pointCount = CORE.Input.Touch.pointCount;
            for (int i = 0; i < gestureEvent.pointCount; i++)
            {
                gestureEvent.pointId[i] = CORE.Input.Touch.pointId[i];
                gestureEvent.position[i] = CORE.Input.Touch.position[i];
            }
            ProcessGestureEvent(gestureEvent);
#endif
        } break;

        case 2: // Touch ended
        case 3: // Touch cancelled
        {
            CORE.Input.Touch.position[index] = (Vector2){ x, y };
            CORE.Input.Touch.currentTouchState[index] = 0;
            CORE.Input.Touch.pointCount--;
            if (CORE.Input.Touch.pointCount < 0) CORE.Input.Touch.pointCount = 0;

            if (index == 0)
            {
                CORE.Input.Mouse.currentButtonState[MOUSE_BUTTON_LEFT] = 0;
            }

#if defined(SUPPORT_GESTURES_SYSTEM)
            GestureEvent gestureEvent = { 0 };
            gestureEvent.touchAction = TOUCH_ACTION_UP;
            gestureEvent.pointCount = CORE.Input.Touch.pointCount;
            for (int i = 0; i < gestureEvent.pointCount; i++)
            {
                gestureEvent.pointId[i] = CORE.Input.Touch.pointId[i];
                gestureEvent.position[i] = CORE.Input.Touch.position[i];
            }
            ProcessGestureEvent(gestureEvent);
#endif
        } break;
    }
}

// Drain all pending touch events from the queue (called from game thread)
static void DrainTouchQueue(void)
{
    pthread_mutex_lock(&platform.touchQueue.mutex);

    while (platform.touchQueue.head != platform.touchQueue.tail)
    {
        TouchEvent *event = &platform.touchQueue.events[platform.touchQueue.head];
        ProcessTouchEvent(event);
        platform.touchQueue.head = (platform.touchQueue.head + 1) % TOUCH_QUEUE_SIZE;
    }

    pthread_mutex_unlock(&platform.touchQueue.mutex);
}

//----------------------------------------------------------------------------------
// Game Thread
//----------------------------------------------------------------------------------

// Game thread entry point
static void *GameThreadFunc(void *arg)
{
    // Make EGL context current on this thread
    if (platform.device != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(platform.device, platform.surface, platform.surface, platform.context);
        TRACELOG(LOG_INFO, "THREAD: EGL context made current on game thread");
    }

    // Run the game (this contains the while(!WindowShouldClose()) loop)
    GameInit();

    platform.gameThreadRunning = false;
    return NULL;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
int InitPlatform(void);                 // Initialize platform (graphics, inputs and more)
void ClosePlatform(void);               // Close platform

static int InitGraphicsDevice(void);    // Initialize graphics device (EGL/ANGLE)
static void CloseGraphicsDevice(void);  // Close graphics device

// Callbacks from Objective-C
void _iosFrameCallback(void);
void _iosTouchEvent(int action, int index, float x, float y);
void _iosAppPaused(void);
void _iosAppResumed(void);

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// NOTE: Functions declaration is provided by raylib.h

//----------------------------------------------------------------------------------
// Module Functions Definition: Window and Graphics Device
//----------------------------------------------------------------------------------

// Check if application should close
bool WindowShouldClose(void)
{
    if (CORE.Window.ready) return CORE.Window.shouldClose;
    else return true;
}

// Toggle fullscreen mode (not available on iOS)
void ToggleFullscreen(void)
{
    TRACELOG(LOG_WARNING, "ToggleFullscreen() not available on iOS");
}

// Toggle borderless windowed mode (not available on iOS)
void ToggleBorderlessWindowed(void)
{
    TRACELOG(LOG_WARNING, "ToggleBorderlessWindowed() not available on iOS");
}

// Set window state: maximized (not available on iOS)
void MaximizeWindow(void)
{
    TRACELOG(LOG_WARNING, "MaximizeWindow() not available on iOS");
}

// Set window state: minimized (not available on iOS)
void MinimizeWindow(void)
{
    TRACELOG(LOG_WARNING, "MinimizeWindow() not available on iOS");
}

// Restore window (not available on iOS)
void RestoreWindow(void)
{
    TRACELOG(LOG_WARNING, "RestoreWindow() not available on iOS");
}

// Set window configuration state using flags
void SetWindowState(unsigned int flags)
{
    // Most window states not applicable on iOS
    // Just update the flags for compatibility
    CORE.Window.flags |= flags;
}

// Clear window configuration state flags
void ClearWindowState(unsigned int flags)
{
    CORE.Window.flags &= ~flags;
}

// Set icon for window (not available on iOS - app icon set in Info.plist)
void SetWindowIcon(Image image)
{
    TRACELOG(LOG_WARNING, "SetWindowIcon() not available on iOS");
}

// Set icon for window (not available on iOS)
void SetWindowIcons(Image *images, int count)
{
    TRACELOG(LOG_WARNING, "SetWindowIcons() not available on iOS");
}

// Set title for window (stored but not displayed on iOS)
void SetWindowTitle(const char *title)
{
    CORE.Window.title = title;
}

// Set window position (not available on iOS)
void SetWindowPosition(int x, int y)
{
    TRACELOG(LOG_WARNING, "SetWindowPosition() not available on iOS");
}

// Set monitor for current window (not available on iOS)
void SetWindowMonitor(int monitor)
{
    TRACELOG(LOG_WARNING, "SetWindowMonitor() not available on iOS");
}

// Set window minimum dimensions (not available on iOS)
void SetWindowMinSize(int width, int height)
{
    CORE.Window.screenMin.width = width;
    CORE.Window.screenMin.height = height;
}

// Set window maximum dimensions (not available on iOS)
void SetWindowMaxSize(int width, int height)
{
    CORE.Window.screenMax.width = width;
    CORE.Window.screenMax.height = height;
}

// Set window dimensions (not available on iOS)
void SetWindowSize(int width, int height)
{
    TRACELOG(LOG_WARNING, "SetWindowSize() not available on iOS");
}

// Set window opacity (not available on iOS)
void SetWindowOpacity(float opacity)
{
    TRACELOG(LOG_WARNING, "SetWindowOpacity() not available on iOS");
}

// Set window focused (not available on iOS)
void SetWindowFocused(void)
{
    TRACELOG(LOG_WARNING, "SetWindowFocused() not available on iOS");
}

// Get native window handle
void *GetWindowHandle(void)
{
    return (__bridge void *)platform.window;
}

// Get number of monitors (always 1 on iOS)
int GetMonitorCount(void)
{
    return 1;
}

// Get current monitor (always 0 on iOS)
int GetCurrentMonitor(void)
{
    return 0;
}

// Get selected monitor position
Vector2 GetMonitorPosition(int monitor)
{
    return (Vector2){ 0, 0 };
}

// Get selected monitor width
int GetMonitorWidth(int monitor)
{
    if (monitor == 0)
    {
        CGRect bounds = [[UIScreen mainScreen] bounds];
        CGFloat scale = [[UIScreen mainScreen] scale];
        return (int)(bounds.size.width * scale);
    }
    return 0;
}

// Get selected monitor height
int GetMonitorHeight(int monitor)
{
    if (monitor == 0)
    {
        CGRect bounds = [[UIScreen mainScreen] bounds];
        CGFloat scale = [[UIScreen mainScreen] scale];
        return (int)(bounds.size.height * scale);
    }
    return 0;
}

// Get selected monitor physical width in millimetres
int GetMonitorPhysicalWidth(int monitor)
{
    // iOS doesn't expose physical dimensions
    // Would need device-specific lookup table
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalWidth() not implemented on iOS");
    return 0;
}

// Get selected monitor physical height in millimetres
int GetMonitorPhysicalHeight(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalHeight() not implemented on iOS");
    return 0;
}

// Get selected monitor refresh rate
int GetMonitorRefreshRate(int monitor)
{
    if (monitor == 0)
    {
        // Check for ProMotion display (120Hz)
        if (@available(iOS 10.3, *))
        {
            return (int)[[UIScreen mainScreen] maximumFramesPerSecond];
        }
    }
    return 60;
}

// Get the human-readable name of the monitor
const char *GetMonitorName(int monitor)
{
    if (monitor == 0) return "iOS Display";
    return "";
}

// Get window position (always 0,0 on iOS)
Vector2 GetWindowPosition(void)
{
    return (Vector2){ 0, 0 };
}

// Get window scale DPI factor
Vector2 GetWindowScaleDPI(void)
{
    CGFloat scale = [[UIScreen mainScreen] scale];
    return (Vector2){ scale, scale };
}

// Set clipboard text content
void SetClipboardText(const char *text)
{
    @autoreleasepool {
        UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
        pasteboard.string = [NSString stringWithUTF8String:text];
    }
}

// Get clipboard text content
const char *GetClipboardText(void)
{
    static char clipboardBuffer[4096] = { 0 };

    @autoreleasepool {
        UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
        NSString *text = pasteboard.string;
        if (text != nil)
        {
            const char *utf8 = [text UTF8String];
            if (utf8 != NULL)
            {
                strncpy(clipboardBuffer, utf8, sizeof(clipboardBuffer) - 1);
                clipboardBuffer[sizeof(clipboardBuffer) - 1] = '\0';
            }
        }
        else
        {
            clipboardBuffer[0] = '\0';
        }
    }

    return clipboardBuffer;
}

// Get clipboard image (not implemented)
Image GetClipboardImage(void)
{
    Image image = { 0 };
    TRACELOG(LOG_WARNING, "GetClipboardImage() not implemented on iOS");
    return image;
}

// Show mouse cursor (no cursor on iOS)
void ShowCursor(void)
{
    CORE.Input.Mouse.cursorHidden = false;
}

// Hides mouse cursor (no cursor on iOS)
void HideCursor(void)
{
    CORE.Input.Mouse.cursorHidden = true;
}

// Enables cursor (no cursor on iOS)
void EnableCursor(void)
{
    CORE.Input.Mouse.cursorHidden = false;
}

// Disables cursor (no cursor on iOS)
void DisableCursor(void)
{
    CORE.Input.Mouse.cursorHidden = true;
}

// Swap back buffer with front buffer (screen drawing)
void SwapScreenBuffer(void)
{
    if ((platform.device != EGL_NO_DISPLAY) && (platform.surface != EGL_NO_SURFACE))
    {
        eglSwapBuffers(platform.device, platform.surface);
    }

    // On iOS, we need to run the run loop after swapping to process events
    // and allow CADisplayLink to signal the next frame
    PollInputEvents();
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Misc
//----------------------------------------------------------------------------------

// Get elapsed time measure in seconds since InitTimer()
double GetTime(void)
{
    double time = 0.0;
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned long long int nanoSeconds = (unsigned long long int)ts.tv_sec*1000000000LLU + (unsigned long long int)ts.tv_nsec;
    time = (double)(nanoSeconds - CORE.Time.base)*1e-9;
    return time;
}

// Open URL with default system browser
void OpenURL(const char *url)
{
    // Security check
    if (strchr(url, '\'') != NULL)
    {
        TRACELOG(LOG_WARNING, "SYSTEM: Provided URL could be potentially malicious, avoid [\'] character");
        return;
    }

    @autoreleasepool {
        NSURL *nsUrl = [NSURL URLWithString:[NSString stringWithUTF8String:url]];
        if (nsUrl != nil)
        {
            [[UIApplication sharedApplication] openURL:nsUrl options:@{} completionHandler:nil];
        }
    }
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Inputs
//----------------------------------------------------------------------------------

// Set internal gamepad mappings
int SetGamepadMappings(const char *mappings)
{
    TRACELOG(LOG_WARNING, "SetGamepadMappings() not implemented on iOS");
    return 0;
}

// Set gamepad vibration
void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration)
{
    TRACELOG(LOG_WARNING, "SetGamepadVibration() not implemented on iOS");
}

// Set mouse position XY
void SetMousePosition(int x, int y)
{
    CORE.Input.Mouse.currentPosition = (Vector2){ (float)x, (float)y };
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
}

// Set mouse cursor
void SetMouseCursor(int cursor)
{
    // No cursor on iOS
}

// Get physical key name
const char *GetKeyName(int key)
{
    TRACELOG(LOG_WARNING, "GetKeyName() not implemented on iOS");
    return "";
}

// Register all input events
void PollInputEvents(void)
{
#if defined(SUPPORT_GESTURES_SYSTEM)
    // NOTE: Gestures update must be called every frame to reset gestures correctly
    UpdateGestures();
#endif

    // Reset keys/chars pressed registered
    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;

    // Reset key repeats
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;

    // Reset last gamepad button/axis registered state
    CORE.Input.Gamepad.lastButtonPressed = 0;

    // IMPORTANT: Register ALL previous states BEFORE processing new events
    // This ensures IsMouseButtonPressed() etc. can detect state changes correctly

    // Register previous touch states
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.previousTouchState[i] = CORE.Input.Touch.currentTouchState[i];

    // Register previous keys states
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++)
    {
        CORE.Input.Keyboard.previousKeyState[i] = CORE.Input.Keyboard.currentKeyState[i];
        CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;
    }

    // Register previous mouse button states
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++)
    {
        CORE.Input.Mouse.previousButtonState[i] = CORE.Input.Mouse.currentButtonState[i];
    }

    // Drain touch events from the thread-safe queue
    // Touch events are pushed by the main thread and processed here on the game thread
    DrainTouchQueue();
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------

// Initialize platform: graphics, inputs and more
int InitPlatform(void)
{
    // Initialize touch event queue for thread-safe touch handling
    InitTouchQueue();

    // iOS is always fullscreen (use flags, not separate field)
    CORE.Window.flags |= FLAG_FULLSCREEN_MODE;

    // Get screen dimensions from view bounds (more reliable than screen bounds for orientation)
    CGRect viewBounds = platform.view ? platform.view.bounds : [[UIScreen mainScreen] bounds];
    CGFloat scale = [[UIScreen mainScreen] scale];

    // Screen size in points
    CORE.Window.screen.width = (int)viewBounds.size.width;
    CORE.Window.screen.height = (int)viewBounds.size.height;

    // Display/render size in pixels
    CORE.Window.display.width = (int)(viewBounds.size.width * scale);
    CORE.Window.display.height = (int)(viewBounds.size.height * scale);

    TRACELOG(LOG_INFO, "DISPLAY: Screen size: %d x %d (scale: %.1f)",
             CORE.Window.display.width, CORE.Window.display.height, scale);

    // Initialize graphics device (EGL/ANGLE)
    if (InitGraphicsDevice() < 0)
    {
        TRACELOG(LOG_ERROR, "PLATFORM: Failed to initialize graphics device");
        return -1;
    }

    // Set render dimensions
    CORE.Window.render.width = CORE.Window.display.width;
    CORE.Window.render.height = CORE.Window.display.height;
    CORE.Window.currentFbo.width = CORE.Window.render.width;
    CORE.Window.currentFbo.height = CORE.Window.render.height;

    TRACELOG(LOG_INFO, "DISPLAY: Device initialized successfully");
    TRACELOG(LOG_INFO, "    > Display size: %i x %i", CORE.Window.display.width, CORE.Window.display.height);
    TRACELOG(LOG_INFO, "    > Screen size:  %i x %i", CORE.Window.screen.width, CORE.Window.screen.height);
    TRACELOG(LOG_INFO, "    > Render size:  %i x %i", CORE.Window.render.width, CORE.Window.render.height);

    // Load OpenGL extensions using ANGLE
    rlLoadExtensions(eglGetProcAddress);

    // Initialize timing system
    InitTimer();

    // Initialize storage system
    // On iOS, change working directory to the app bundle's resource path
    // so relative paths work correctly
    @autoreleasepool {
        NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
        if (resourcePath != nil)
        {
            // Use bundle resource path as base path
            static char basePath[1024] = { 0 };
            strncpy(basePath, [resourcePath UTF8String], sizeof(basePath) - 1);
            CORE.Storage.basePath = basePath;

            // Change working directory to bundle resource path
            // This makes relative paths like "assets/title.png" work correctly
            chdir(basePath);
            TRACELOG(LOG_INFO, "STORAGE: Working directory set to: %s", basePath);
        }
        else
        {
            CORE.Storage.basePath = GetWorkingDirectory();
        }
    }

    TRACELOG(LOG_INFO, "PLATFORM: iOS: Initialized successfully");

    platform.appActive = true;
    CORE.Window.ready = true;

    return 0;
}

// Close platform
void ClosePlatform(void)
{
    CloseGraphicsDevice();

    // Invalidate display link
    if (platform.displayLink != nil)
    {
        [platform.displayLink invalidate];
        platform.displayLink = nil;
    }

    CORE.Window.ready = false;
    TRACELOG(LOG_INFO, "PLATFORM: iOS: Platform closed");
}

// Initialize graphics device (EGL/ANGLE)
static int InitGraphicsDevice(void)
{
    // NOTE: UIKit objects (window, view, viewController) must be created
    // by the application's UIApplicationDelegate before calling InitPlatform()

    if (platform.view == nil)
    {
        TRACELOG(LOG_ERROR, "DISPLAY: iOS view not set. Create UIWindow/UIView in AppDelegate first.");
        return -1;
    }

    // Get EGL display
    platform.device = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (platform.device == EGL_NO_DISPLAY)
    {
        TRACELOG(LOG_ERROR, "DISPLAY: Failed to get EGL display");
        return -1;
    }

    // Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(platform.device, &major, &minor))
    {
        TRACELOG(LOG_ERROR, "DISPLAY: Failed to initialize EGL");
        return -1;
    }
    TRACELOG(LOG_INFO, "DISPLAY: EGL version: %d.%d", major, minor);

    // Configure EGL
    EGLint samples = 0;
    if (CORE.Window.flags & FLAG_MSAA_4X_HINT) samples = 4;

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_SAMPLE_BUFFERS, (samples > 0) ? 1 : 0,
        EGL_SAMPLES, samples,
        EGL_NONE
    };

    EGLint numConfigs;
    if (!eglChooseConfig(platform.device, configAttribs, &platform.config, 1, &numConfigs) || numConfigs == 0)
    {
        TRACELOG(LOG_ERROR, "DISPLAY: Failed to choose EGL config");
        return -1;
    }

    // Create window surface from iOS view's layer
    // ANGLE uses Metal backend, so we expect CAMetalLayer
    CAMetalLayer *layer = (CAMetalLayer *)platform.view.layer;
    CGFloat scale = [[UIScreen mainScreen] scale];
    layer.contentsScale = scale;

    // Explicitly set the drawable size to match the expected render size
    // This ensures the Metal framebuffer matches our expected dimensions
    CGSize drawableSize = CGSizeMake(layer.bounds.size.width * scale,
                                      layer.bounds.size.height * scale);
    layer.drawableSize = drawableSize;

    platform.surface = eglCreateWindowSurface(platform.device, platform.config,
                                               (__bridge EGLNativeWindowType)layer, NULL);
    if (platform.surface == EGL_NO_SURFACE)
    {
        TRACELOG(LOG_ERROR, "DISPLAY: Failed to create EGL surface: 0x%04x", eglGetError());
        return -1;
    }

    // Create EGL context
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    platform.context = eglCreateContext(platform.device, platform.config, EGL_NO_CONTEXT, contextAttribs);
    if (platform.context == EGL_NO_CONTEXT)
    {
        // Try OpenGL ES 2.0 fallback
        const EGLint contextAttribs2[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };
        platform.context = eglCreateContext(platform.device, platform.config, EGL_NO_CONTEXT, contextAttribs2);

        if (platform.context == EGL_NO_CONTEXT)
        {
            TRACELOG(LOG_ERROR, "DISPLAY: Failed to create EGL context");
            return -1;
        }
        TRACELOG(LOG_INFO, "DISPLAY: Using OpenGL ES 2.0");
    }
    else
    {
        TRACELOG(LOG_INFO, "DISPLAY: Using OpenGL ES 3.0");
    }

    // Make context current
    if (!eglMakeCurrent(platform.device, platform.surface, platform.surface, platform.context))
    {
        TRACELOG(LOG_ERROR, "DISPLAY: Failed to make EGL context current");
        return -1;
    }

    // Query actual EGL surface size
    EGLint surfaceWidth, surfaceHeight;
    eglQuerySurface(platform.device, platform.surface, EGL_WIDTH, &surfaceWidth);
    eglQuerySurface(platform.device, platform.surface, EGL_HEIGHT, &surfaceHeight);
    TRACELOG(LOG_INFO, "DISPLAY: EGL surface size: %d x %d", surfaceWidth, surfaceHeight);

    // Set swap interval (vsync)
    eglSwapInterval(platform.device, (CORE.Window.flags & FLAG_VSYNC_HINT) ? 1 : 0);

    return 0;
}

// Close graphics device
static void CloseGraphicsDevice(void)
{
    if (platform.device != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(platform.device, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (platform.context != EGL_NO_CONTEXT)
        {
            eglDestroyContext(platform.device, platform.context);
            platform.context = EGL_NO_CONTEXT;
        }

        if (platform.surface != EGL_NO_SURFACE)
        {
            eglDestroySurface(platform.device, platform.surface);
            platform.surface = EGL_NO_SURFACE;
        }

        eglTerminate(platform.device);
        platform.device = EGL_NO_DISPLAY;
    }
}

//----------------------------------------------------------------------------------
// iOS Platform Callbacks (called from Objective-C)
//----------------------------------------------------------------------------------

// Frame callback - called by CADisplayLink on main thread
void _iosFrameCallback(void)
{
    // With threaded game loop, this callback is only used for callback mode
    // The game thread runs independently and doesn't wait for display link
    if (platform.appActive && gameUpdateCallback != NULL)
    {
        gameUpdateCallback();
    }
}

// Touch event callback - called from main thread, pushes to queue for game thread
// action: 0 = began, 1 = moved, 2 = ended, 3 = cancelled
void _iosTouchEvent(int action, int index, float x, float y)
{
    // Push to thread-safe queue - will be processed by game thread in PollInputEvents
    PushTouchEvent(action, index, x, y);
}

// App paused callback
void _iosAppPaused(void)
{
    platform.appActive = false;
    TRACELOG(LOG_INFO, "PLATFORM: iOS: App paused");
}

// App resumed callback
void _iosAppResumed(void)
{
    platform.appActive = true;
    platform.contextRebindRequired = true;

    // Rebind EGL context
    if (platform.device != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(platform.device, platform.surface, platform.surface, platform.context);
    }

    TRACELOG(LOG_INFO, "PLATFORM: iOS: App resumed");
}

//----------------------------------------------------------------------------------
// iOS-Specific API Functions
//----------------------------------------------------------------------------------

// Set the game update callback (required for iOS)
// This function is called every frame by CADisplayLink
void SetUpdateCallback(void (*callback)(void))
{
    gameUpdateCallback = callback;
}

// Set platform UIKit objects (must be called from AppDelegate)
void SetPlatformWindow(void *window)
{
    platform.window = (__bridge UIWindow *)window;
}

void SetPlatformViewController(void *viewController)
{
    platform.viewController = (__bridge UIViewController *)viewController;
}

void SetPlatformView(void *view)
{
    platform.view = (__bridge UIView *)view;
}

void SetPlatformDisplayLink(void *displayLink)
{
    platform.displayLink = (__bridge CADisplayLink *)displayLink;
}

// Start the game thread - call this from AppDelegate instead of calling GameInit directly
// This spawns the game loop on a background thread, allowing the main thread to handle touches
void StartGameThread(void)
{
    if (platform.gameThreadRunning) return;  // Already running

    platform.gameThreadRunning = true;

    // Detach EGL context from current thread before spawning game thread
    // The game thread will make it current
    if (platform.device != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(platform.device, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    // Spawn game thread
    int result = pthread_create(&platform.gameThread, NULL, GameThreadFunc, NULL);
    if (result != 0)
    {
        TRACELOG(LOG_ERROR, "THREAD: Failed to create game thread: %d", result);
        platform.gameThreadRunning = false;
    }
    else
    {
        // Detach thread so it cleans up automatically when done
        pthread_detach(platform.gameThread);
        TRACELOG(LOG_INFO, "THREAD: Game thread started successfully");
    }
}

// EOF
