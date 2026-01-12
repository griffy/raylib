// raudio_objc.m - Objective-C wrapper for raudio.c on iOS
// This file exists to compile raudio.c as Objective-C, which is required
// because miniaudio uses AVFoundation on iOS, which requires Objective-C.
// Simply include the main raudio.c file - it will be compiled as Objective-C.

#include "raudio.c"
