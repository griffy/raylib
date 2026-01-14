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

        // Configure the Metal layer for ANGLE
        CAMetalLayer *metalLayer = (CAMetalLayer *)self.layer;
        metalLayer.opaque = NO;  // Let UIView background show through before first render
        metalLayer.contentsScale = [[UIScreen mainScreen] scale];
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metalLayer.framebufferOnly = NO;

        // Get the default Metal device
        metalLayer.device = MTLCreateSystemDefaultDevice();
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

// Touch handling - only process touches that changed, use index 0 for primary touch
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches)
    {
        CGPoint location = [touch locationInView:self];
        _iosTouchEvent(0, 0, location.x, location.y);  // action=began, index=0
        break;  // Only handle first touch for now
    }
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches)
    {
        CGPoint location = [touch locationInView:self];
        _iosTouchEvent(1, 0, location.x, location.y);  // action=moved, index=0
        break;
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches)
    {
        CGPoint location = [touch locationInView:self];
        _iosTouchEvent(2, 0, location.x, location.y);  // action=ended, index=0
        break;
    }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches)
    {
        CGPoint location = [touch locationInView:self];
        _iosTouchEvent(3, 0, location.x, location.y);  // action=cancelled, index=0
        break;
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
