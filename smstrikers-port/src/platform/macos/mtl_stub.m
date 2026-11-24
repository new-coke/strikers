// Metal gained MTLLogStateDescriptor in macOS 14. The prebuilt Dawn library references the class
// behind @available, so the linker still wants the symbol whenever the deployment target is older
// than macOS 14, and Metal.framework cannot provide it there. The empty implementation below
// satisfies the linker; @available guarantees Dawn never instantiates it on systems where the real
// class is absent, and the guard keeps it from colliding with the genuine class when targeting
// macOS 14 or newer.

#import <Availability.h>
#import <Foundation/Foundation.h>

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 140000

@interface MTLLogStateDescriptor : NSObject
@end

@implementation MTLLogStateDescriptor
@end

#endif
