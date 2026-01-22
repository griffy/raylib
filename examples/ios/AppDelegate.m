/*******************************************************************************************
*
*   raylib iOS - Application Delegate
*
*   This file sets up the iOS application and integrates with raylib's iOS platform layer.
*   Add this to your Xcode project along with raylib source files.
*
*   LICENSE: zlib/libpng (same as raylib)
*
********************************************************************************************/

#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <Metal/Metal.h>

// raylib iOS platform integration functions
extern void SetPlatformWindow(void *window);
extern void SetPlatformViewController(void *viewController);
extern void SetPlatformView(void *view);
extern void SetPlatformDisplayLink(void *displayLink);
extern void SetUpdateCallback(void (*callback)(void));

extern void _iosFrameCallback(void);
extern void _iosTouchEvent(int action, int index, float x, float y);
extern void _iosAppPaused(void);
extern void _iosAppResumed(void);

// Forward declaration - implement this in your game code
// Note: When using PLATFORM_IOS, raylib.h renames main() to GameInit() automatically
// so games compile without modification. The int return type is from main().
extern int GameInit(void);

// Starts the game loop on a background thread (allows main thread to handle touch events)
extern void StartGameThread(void);

//----------------------------------------------------------------------------------
// RaylibView - Custom UIView with CAMetalLayer for ANGLE rendering
//----------------------------------------------------------------------------------
@interface RaylibView : UIView
{
    // Touch tracking - map UITouch pointers to indices (0-9)
    NSMutableDictionary<NSValue *, NSNumber *> *touchIndexMap;
    int nextTouchIndex;
}
@end

@implementation RaylibView

+ (Class)layerClass
{
    // ANGLE uses Metal backend, so we need CAMetalLayer
    return [CAMetalLayer class];
}

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
    {
        self.multipleTouchEnabled = YES;
        self.userInteractionEnabled = YES;

        // Initialize touch tracking
        touchIndexMap = [[NSMutableDictionary alloc] init];
        nextTouchIndex = 0;

        // Configure the Metal layer for ANGLE
        CAMetalLayer *metalLayer = (CAMetalLayer *)self.layer;
        metalLayer.opaque = NO;  // Let UIView background show through before first render
        metalLayer.contentsScale = [[UIScreen mainScreen] scale];
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metalLayer.framebufferOnly = NO;

        // Get the default Metal device
        metalLayer.device = MTLCreateSystemDefaultDevice();

        // Match launch screen background color for seamless transition
        self.backgroundColor = [UIColor colorWithRed:1.0/255.0 green:25.0/255.0 blue:44.0/255.0 alpha:1.0];
    }
    return self;
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    // Update Metal layer drawable size when layout changes
    CAMetalLayer *metalLayer = (CAMetalLayer *)self.layer;
    CGFloat scale = [[UIScreen mainScreen] scale];
    metalLayer.drawableSize = CGSizeMake(self.bounds.size.width * scale,
                                          self.bounds.size.height * scale);
}

// Get or assign a touch index for the given UITouch
- (int)indexForTouch:(UITouch *)touch
{
    NSValue *key = [NSValue valueWithPointer:(__bridge const void *)touch];
    NSNumber *existing = touchIndexMap[key];
    if (existing != nil)
    {
        return [existing intValue];
    }

    // Assign new index (max 10 touches, indices 0-9)
    int index = nextTouchIndex % 10;
    nextTouchIndex++;
    touchIndexMap[key] = @(index);
    return index;
}

// Remove touch from tracking
- (void)removeTouch:(UITouch *)touch
{
    NSValue *key = [NSValue valueWithPointer:(__bridge const void *)touch];
    [touchIndexMap removeObjectForKey:key];

    // Reset index counter when no touches remain
    if (touchIndexMap.count == 0)
    {
        nextTouchIndex = 0;
    }
}

// Touch handling - process all touches with proper indices for multi-touch gestures
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches)
    {
        int index = [self indexForTouch:touch];
        CGPoint location = [touch locationInView:self];
        _iosTouchEvent(0, index, location.x, location.y);  // action=began
    }
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches)
    {
        int index = [self indexForTouch:touch];
        CGPoint location = [touch locationInView:self];
        _iosTouchEvent(1, index, location.x, location.y);  // action=moved
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches)
    {
        int index = [self indexForTouch:touch];
        CGPoint location = [touch locationInView:self];
        _iosTouchEvent(2, index, location.x, location.y);  // action=ended
        [self removeTouch:touch];
    }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches)
    {
        int index = [self indexForTouch:touch];
        CGPoint location = [touch locationInView:self];
        _iosTouchEvent(3, index, location.x, location.y);  // action=cancelled
        [self removeTouch:touch];
    }
}

@end

//----------------------------------------------------------------------------------
// RaylibViewController - Main view controller
//----------------------------------------------------------------------------------
@interface RaylibViewController : UIViewController
@property (nonatomic, strong) CADisplayLink *displayLink;
@property (nonatomic, strong) RaylibView *raylibView;
@end

@implementation RaylibViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    NSLog(@"[RAYLIB] viewDidLoad - bounds: %@", NSStringFromCGRect(self.view.bounds));

    // Create raylib view
    self.raylibView = [[RaylibView alloc] initWithFrame:self.view.bounds];
    self.raylibView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:self.raylibView];
    NSLog(@"[RAYLIB] Created RaylibView");

    // Pass view to raylib
    SetPlatformView((__bridge void *)self.raylibView);
    NSLog(@"[RAYLIB] SetPlatformView called");

    // Create display link for frame callbacks
    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(frameCallback:)];
    self.displayLink.preferredFramesPerSecond = 60;
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    SetPlatformDisplayLink((__bridge void *)self.displayLink);
    NSLog(@"[RAYLIB] DisplayLink created and set");

    // Start the game loop on a background thread
    // Game will load assets while launch screen is visible, then call iOSShowWindow()
    NSLog(@"[RAYLIB] About to call StartGameThread");
    StartGameThread();
    NSLog(@"[RAYLIB] StartGameThread returned");
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    NSLog(@"[RAYLIB] viewDidAppear");
}

- (void)frameCallback:(CADisplayLink *)sender
{
    _iosFrameCallback();
}

- (void)viewWillDisappear:(BOOL)animated
{
    [super viewWillDisappear:animated];
    [self.displayLink invalidate];
    self.displayLink = nil;
}

- (BOOL)prefersStatusBarHidden
{
    return YES;
}

- (BOOL)prefersHomeIndicatorAutoHidden
{
    return YES;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
    return UIRectEdgeAll;
}

@end

//----------------------------------------------------------------------------------
// RaylibAppDelegate - Application delegate
//----------------------------------------------------------------------------------
@interface RaylibAppDelegate : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow *window;
@end

@implementation RaylibAppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    NSLog(@"[RAYLIB] application:didFinishLaunchingWithOptions");

    // Create window
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    NSLog(@"[RAYLIB] Window created: %@", NSStringFromCGRect(self.window.bounds));

    // Create view controller
    RaylibViewController *viewController = [[RaylibViewController alloc] init];
    self.window.rootViewController = viewController;
    NSLog(@"[RAYLIB] ViewController created");

    // Pass window/VC to raylib
    SetPlatformWindow((__bridge void *)self.window);
    SetPlatformViewController((__bridge void *)viewController);
    NSLog(@"[RAYLIB] Platform window and VC set");

    // Show window - the RaylibView background matches the launch screen,
    // so the transition is seamless even before the first frame renders
    [self.window makeKeyAndVisible];
    NSLog(@"[RAYLIB] Window made key and visible");

    return YES;
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    _iosAppPaused();
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    _iosAppResumed();
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    // Cleanup handled by raylib
}

@end

//----------------------------------------------------------------------------------
// Main Entry Point
//----------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([RaylibAppDelegate class]));
    }
}
