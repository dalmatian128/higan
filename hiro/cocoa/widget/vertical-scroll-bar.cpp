#if defined(Hiro_VerticalScrollBar)

@implementation CocoaVerticalScrollBar : NSScroller

-(id) initWith:(hiro::mVerticalScrollBar&)verticalScrollBarReference {
  if(self = [super initWithFrame:NSMakeRect(0, 0, 0, 1)]) {
    verticalScrollBar = &verticalScrollBarReference;

    [self setTarget:self];
    [self setAction:@selector(scroll:)];

    [self setControlSize:NSControlSizeRegular];
    [self setScrollerStyle:NSScrollerStyleLegacy];
    [self setEnabled:YES];

    [self update];
  }
  return self;
}

-(void) update {
  f64 d = 1.0 / verticalScrollBar->state.length;
  f64 f = d * verticalScrollBar->state.position;

  [self setDoubleValue:f];
  [self setKnobProportion:d];
}

-(IBAction) scroll:(id)sender {
  auto& state = verticalScrollBar->state;

  switch([self hitPart]) {
  case NSScrollerIncrementLine:
  case NSScrollerIncrementPage:
    if(state.position < state.length - 1) state.position++;
    [self update];
    break;

  case NSScrollerDecrementLine:
  case NSScrollerDecrementPage:
    if(state.position) state.position--;
    [self update];
    break;

  case NSScrollerKnob:
    state.position = [self doubleValue] * state.length;
    break;
  }

  verticalScrollBar->doChange();
}

@end

namespace hiro {

auto pVerticalScrollBar::construct() -> void {
  @autoreleasepool {
    cocoaView = cocoaVerticalScrollBar = [[CocoaVerticalScrollBar alloc] initWith:self()];
    pWidget::construct();

    setLength(state().length);
    setPosition(state().position);
  }
}

auto pVerticalScrollBar::destruct() -> void {
  @autoreleasepool {
    [cocoaView removeFromSuperview];
    cocoaView = cocoaVerticalScrollBar = nil;
  }
}

auto pVerticalScrollBar::minimumSize() const -> Size {
  @autoreleasepool {
    return {(s32)[NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy], 32};
  }
}

auto pVerticalScrollBar::setLength(u32 length) -> void {
  @autoreleasepool {
    [cocoaVerticalScrollBar update];
  }
}

auto pVerticalScrollBar::setPosition(u32 position) -> void {
  @autoreleasepool {
    [cocoaVerticalScrollBar update];
  }
}

}

#endif
