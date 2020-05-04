#if defined(Hiro_Widget)

namespace hiro {

auto pWidget::construct() -> void {
  @autoreleasepool {
    if(!cocoaView) {
      abstract = true;
      cocoaView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)];
      [cocoaView setHidden:true];
    }

    if(auto window = self().parentWindow(true)) {
      if(auto p = window->self()) p->_append(self());
      setDroppable(self().droppable());
      setEnabled(self().enabled(true));
      setFocusable(self().focusable());
      setFont(self().font(true));
      setMouseCursor(self().mouseCursor());
      setToolTip(self().toolTip());
      setVisible(self().visible(true));
    }
  }
}

auto pWidget::destruct() -> void {
  @autoreleasepool {
    [cocoaView removeFromSuperview];
    cocoaView = nil;
  }
}

auto pWidget::focused() const -> bool {
  @autoreleasepool {
    return cocoaView == [[cocoaView window] firstResponder];
  }
}

auto pWidget::setDroppable(bool droppable) -> void {
  //virtual
}

auto pWidget::setEnabled(bool enabled) -> void {
  if(abstract) enabled = false;

  @autoreleasepool {
    SEL aSelector = @selector(setEnabled:);
    if([cocoaView respondsToSelector:aSelector]) {
      NSMethodSignature *methodSignature = [cocoaView methodSignatureForSelector:aSelector];
      NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:methodSignature];
      [invocation setSelector:aSelector];
      [invocation setArgument:&enabled atIndex:2];
      [invocation invokeWithTarget:cocoaView];
    }
  }
}

auto pWidget::setFocusable(bool focusable) -> void {
  //virtual
}

auto pWidget::setFocused() -> void {
  @autoreleasepool {
    [[cocoaView window] makeFirstResponder:cocoaView];
  }
}

auto pWidget::setFont(const Font& font) -> void {
  @autoreleasepool {
    SEL aSelector = @selector(setFont:);
    if([cocoaView respondsToSelector:aSelector]) {
      NSFont* nsfont = pFont::create(font);
      NSMethodSignature *methodSignature = [cocoaView methodSignatureForSelector:aSelector];
      NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:methodSignature];
      [invocation setSelector:aSelector];
      [invocation setArgument:&nsfont atIndex:2];
      [invocation invokeWithTarget:cocoaView];
    }
  }
}

auto pWidget::setGeometry(Geometry geometry) -> void {
  @autoreleasepool {
    CGFloat windowHeight = [[cocoaView superview] frame].size.height;
    //round coordinates
    u32 x = geometry.x();
    u32 y = windowHeight - geometry.y() - geometry.height();
    u32 width = geometry.width();
    u32 height = geometry.height();
    [cocoaView setFrame:NSMakeRect(x, y, width, height)];
    [[cocoaView superview] setNeedsDisplay:YES];
  }
  pSizable::setGeometry(geometry);
}

auto pWidget::setMouseCursor(const MouseCursor& mouseCursor) -> void {
  //TODO
}

auto pWidget::setToolTip(const string& toolTip) -> void {
  //TODO
}

auto pWidget::setVisible(bool visible) -> void {
  if(abstract) visible = false;

  @autoreleasepool {
    [cocoaView setHidden:!visible];
  }
}

}

#endif
