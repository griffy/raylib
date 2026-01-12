// rcore_objc.m - Objective-C wrapper for rcore.c on iOS
// This file exists to compile rcore.c as Objective-C, which is required
// because the iOS platform layer (rcore_ios.c) uses UIKit/Objective-C.
// Simply include the main rcore.c file - it will be compiled as Objective-C.

#include "rcore.c"
