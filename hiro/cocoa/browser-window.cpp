#if defined(Hiro_BrowserWindow)

namespace hiro {

auto pBrowserWindow::directory(BrowserWindow::State& state) -> string {
  string result;

  @autoreleasepool {
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    if(state.title) [panel setTitle:[NSString stringWithUTF8String:state.title]];
    [panel setCanChooseDirectories:YES];
    [panel setCanChooseFiles:NO];
    [panel setDirectoryURL:[NSURL URLWithString:[NSString stringWithUTF8String:state.path]]];
    if([panel runModal] == NSModalResponseOK) {
      const char* name = [[[panel URL] absoluteString] UTF8String];
      if(name) result = name;
    }
  }

  return result;
}

auto pBrowserWindow::open(BrowserWindow::State& state) -> string {
  string result;

  @autoreleasepool {
    NSMutableArray* filters = [[NSMutableArray alloc] init];
    for(auto& rule : state.filters) {
      string pattern = rule.split("|", 1L)(1).transform(":", ";");
      if(pattern) [filters addObject:[NSString stringWithUTF8String:pattern]];
    }
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    if(state.title) [panel setTitle:[NSString stringWithUTF8String:state.title]];
    [panel setCanChooseDirectories:NO];
    [panel setCanChooseFiles:YES];
    [panel setAllowedFileTypes:filters];
    [panel setDirectoryURL:[NSURL URLWithString:[NSString stringWithUTF8String:state.path]]];
    if([panel runModal] == NSModalResponseOK) {
      const char* name = [[[panel URL] absoluteString] UTF8String];
      if(name) result = name;
    }
    filters = nil;
  }

  return result;
}

auto pBrowserWindow::save(BrowserWindow::State& state) -> string {
  string result;

  @autoreleasepool {
    NSMutableArray* filters = [[NSMutableArray alloc] init];
    for(auto& rule : state.filters) {
      string pattern = rule.split("|", 1L)(1).transform(":", ";");
      if(pattern) [filters addObject:[NSString stringWithUTF8String:pattern]];
    }
    NSSavePanel* panel = [NSSavePanel savePanel];
    if(state.title) [panel setTitle:[NSString stringWithUTF8String:state.title]];
    [panel setAllowedFileTypes:filters];
    [panel setDirectoryURL:[NSURL URLWithString:[NSString stringWithUTF8String:state.path]]];
    if([panel runModal] == NSModalResponseOK) {
      const char* name = [[[panel URL] absoluteString] UTF8String];
      if(name) result = name;
    }
    filters = nil;
  }

  return result;
}

}

#endif
