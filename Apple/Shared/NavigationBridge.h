#import <Foundation/Foundation.h>
NS_ASSUME_NONNULL_BEGIN
@interface GSNavigation : NSObject
@property(nonatomic,readonly) NSInteger page;
@property(nonatomic,readonly) NSInteger selected;
@property(nonatomic,readonly) NSInteger section;
@property(nonatomic,readonly) NSInteger menu;
@property(nonatomic,readonly) NSInteger browser;
@property(nonatomic,readonly) NSInteger theme;
@property(nonatomic,readonly) BOOL editing;
@property(nonatomic,readonly) NSInteger editAddress;
@property(nonatomic,readonly) float delta;
@property(nonatomic,readonly) NSInteger loadInstrument;
@property(nonatomic,readonly) BOOL saveTheme;
- (void)configureTheme:(NSInteger)theme instrument:(NSInteger)instrument;
- (void)setInstrument:(NSInteger)instrument;
- (void)go:(NSInteger)page;
- (void)key:(NSInteger)key down:(BOOL)down fine:(BOOL)fine time:(uint32_t)time;
- (void)tick:(uint32_t)time;
- (NSInteger)binding:(NSInteger)slot;
@end
NS_ASSUME_NONNULL_END
