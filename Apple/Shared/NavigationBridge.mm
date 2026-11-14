#import "NavigationBridge.h"
#include "../../Core/Navigation.hpp"
@implementation GSNavigation { gs::Navigation nav;gs::NavInput input; }
- (NSInteger)page{return int(nav.page);}
- (NSInteger)selected{return nav.selected();}
- (NSInteger)section{return nav.section;}
- (NSInteger)menu{return nav.menu;}
- (NSInteger)browser{return nav.browser;}
- (NSInteger)theme{return nav.theme;}
- (BOOL)editing{return nav.editing||(!input.fn&&(input.numbers&~nav.blockedNumbers));}
- (NSInteger)editAddress{return nav.editAddress;}
- (float)delta{return nav.delta;}
- (NSInteger)loadInstrument{return nav.loadInstrument;}
- (BOOL)saveTheme{return nav.saveTheme;}
- (NSInteger)binding:(NSInteger)slot{return gs::Navigation::binding(nav.page,int(slot));}
- (void)configureTheme:(NSInteger)theme instrument:(NSInteger)instrument{nav.theme=nav.savedTheme=int(theme);nav.instrument=int(instrument);}
- (void)setInstrument:(NSInteger)instrument{nav.instrument=int(instrument);}
- (void)go:(NSInteger)page{nav.go(gs::Page(std::clamp(int(page),0,4)));}
- (void)tick:(uint32_t)time{nav.scan(input,time);}
- (void)key:(NSInteger)key down:(BOOL)down fine:(BOOL)fine time:(uint32_t)time{
    if(key>=0&&key<8){if(down)input.numbers|=1u<<key;else input.numbers&=~(1u<<key);}
    else switch(key){case 8:input.left=down;break;case 9:input.right=down;break;case 10:input.up=down;break;case 11:input.down=down;break;case 12:input.tab=down;input.reverse=fine;break;case 13:input.enter=down;break;case 14:input.escape=down;break;}
    input.fine=fine;nav.scan(input,time);
}
@end
