#import <MetalKit/MetalKit.h>
#import <CoreMedia/CoreMedia.h>

struct MetalVertex
{
  simd_float2 deviceCoordinate;
  simd_float2 textureCoordinate;
};

struct MetalViewport
{
  simd_float2 drawableSize;
  simd_float2 viewportSize;
};

static const NSUInteger kMaxInflightTextures = 3;

@interface RubyVideoMetalRenderer : NSObject<MTKViewDelegate>
@property(readonly ) CGSize drawableSize;
@property(readwrite) CGSize viewportSize;
- (instancetype) initWithView:(MTKView *)view;
- (void) setVertex:(const MetalVertex*)vertices length:(NSUInteger)length;
- (void) setShader:(const string&)name;
- (id<MTLTexture>) dequeueWithTextureSize:(CGSize)size;
- (void) enqueue:(id<MTLTexture>)texture;
- (void) flush;
@end

struct VideoMetal : VideoDriver {
  VideoMetal& self = *this;
  VideoMetal(Video& super) : VideoDriver(super) {}
  ~VideoMetal() { terminate(); }

  auto create() -> bool override {
    return initialize();
  }

  auto driver() -> string override { return "Metal"; }
  auto ready() -> bool override { return _ready; }

  auto hasFullScreen() -> bool override { return true; }
  auto hasContext() -> bool override { return true; }
  auto hasBlocking() -> bool override { return true; }
  auto hasShader() -> bool override { return true; }

  auto setFullScreen(bool fullScreen) -> bool override {
#if 1
    [[_view window] toggleFullScreen:nil];
#else
    NSArray* obj = [NSArray arrayWithObjects:[NSNumber numberWithBool:!fullScreen], nil];
    NSArray* key = [NSArray arrayWithObjects:NSFullScreenModeAllScreens, nil];
    NSDictionary* options = [NSDictionary dictionaryWithObjects:obj forKeys:key];
    if(fullScreen) {
      [_view enterFullScreenMode:[NSScreen mainScreen] withOptions:options];
    } else {
      [_view exitFullScreenModeWithOptions:options];
    }
#endif
    return true;
  }

  auto setContext(uintptr context) -> bool override {
    return initialize();
  }

  auto setBlocking(bool blocking) -> bool override {
    @autoreleasepool {
      CAMetalLayer* metalLayer = (CAMetalLayer*)[_view layer];
      [metalLayer setDisplaySyncEnabled:blocking];
    }
    return true;
  }

  auto setShader(string shader) -> bool override {
    @autoreleasepool {
      [_renderer setShader:shader];
    }
    return true;
  }

  auto focused() -> bool override {
    return true;
  }

  auto clear() -> void override {
    [_renderer setViewportSize:[_renderer drawableSize]];
    [_view draw];
  }

  auto size(u32& width, u32& height) -> void override {
    @autoreleasepool {
      auto size = [_renderer drawableSize];
      width = size.width;
      height = size.height;
    }
  }

  auto acquire(u32*& data, u32& pitch, u32 width, u32 height) -> bool override {
    @autoreleasepool {
      _texture = [_renderer dequeueWithTextureSize:NSMakeSize(width, height)];
      u32 textureWidth = static_cast<u32>([_texture width]);
      u32 textureAlign = textureWidth > width ? (textureWidth - width) / 2 : 0;
      data = static_cast<u32*>([[_texture buffer] contents]) + textureAlign;
      pitch = textureWidth * sizeof(u32);
      return data != NULL;
    }
  }

  auto release() -> void override {
    @autoreleasepool {
      [_renderer enqueue:_texture];
      _texture = nil;
    }
  }

  auto output(uint width, uint height) -> void override {
    [_renderer setViewportSize:NSMakeSize(width, height)];
    [_view draw];
  }

private:
  auto initialize() -> bool {
    terminate();
    if(!self.context) return false;

    @autoreleasepool {
      auto context = (__bridge NSView*)((void *)self.context);
      auto frameRect = [context bounds];
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();

      _view = [[MTKView alloc] initWithFrame:frameRect device:device];
      [_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
      [_view setPaused:YES];
      [_view setEnableSetNeedsDisplay:NO];

      _renderer = [[RubyVideoMetalRenderer alloc] initWithView:_view];
      [_renderer setVertex:_vertices length:sizeof(_vertices)];
      [_renderer setShader:self.shader];
      [_renderer mtkView:_view drawableSizeWillChange:[_view drawableSize]];
      [_view setDelegate:_renderer];

      [context addSubview:_view];
      [[_view window] makeFirstResponder:_view];

      setBlocking(self.blocking);
    }

    clear();
    return _ready = true;
  }

  auto terminate() -> void {
    _ready = false;

    @autoreleasepool {
      [_renderer flush];
      _renderer = nil;

      if(_view) {
        [_view removeFromSuperview];
        _view = nil;
      }
    }
  }

  MTKView* _view = nullptr;
  RubyVideoMetalRenderer* _renderer = nullptr;

  static constexpr MetalVertex _vertices[] = {
    // device coordinates, texture coordinates
    { {  1.0, -1.0 }, { 1.0, 1.0 }, },
    { { -1.0, -1.0 }, { 0.0, 1.0 }, },
    { { -1.0,  1.0 }, { 0.0, 0.0 }, },

    { {  1.0, -1.0 }, { 1.0, 1.0 }, },
    { { -1.0,  1.0 }, { 0.0, 0.0 }, },
    { {  1.0,  1.0 }, { 1.0, 0.0 }, },
  };

  id<MTLTexture> _texture = nil;

  bool _ready = false;
};

@interface RubyVideoMetalRenderer ()
@property(readwrite) CGSize drawableSize;
@property id<MTLBuffer> vertexBuffer;
@property id<MTLSamplerState> samplerState;
@end

@implementation RubyVideoMetalRenderer
{
  id<MTLDevice> _device;
  id<MTLCommandQueue> _commandQueue;
  id<MTLRenderPipelineState> _renderPipelineState;
  MTLPixelFormat _pixelFormat;

  CMSimpleQueueRef _dynamicDataTextures;
  CMSimpleQueueRef _dynamicFreeTextures;
  dispatch_semaphore_t _frameBoundarySemaphore;
}
@synthesize drawableSize;
@synthesize viewportSize;
@synthesize vertexBuffer;
@synthesize samplerState;

- (void) mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size
{
  @autoreleasepool {
    [self setDrawableSize:size];
  }
}

- (void) drawInMTKView:(MTKView *)view
{
  @autoreleasepool {
    [self render:view];
  }
}

- (instancetype) initWithView:(MTKView *)view
{
  self = [super init];
  if(self) {
    _device = [view device];
    _pixelFormat = [view colorPixelFormat];
    _commandQueue = [_device newCommandQueue];

    id<MTLLibrary> defaultLibrary = [_device newDefaultLibrary];
    MTLRenderPipelineDescriptor *renderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
    renderPipelineDescriptor.sampleCount = [view sampleCount];
    renderPipelineDescriptor.colorAttachments[0].pixelFormat = _pixelFormat;
    renderPipelineDescriptor.vertexFunction = [defaultLibrary newFunctionWithName:@"MetalVertexShader"];
    renderPipelineDescriptor.fragmentFunction = [defaultLibrary newFunctionWithName:@"MetalFragmentShader"];

    NSError *error;
    _renderPipelineState = [_device newRenderPipelineStateWithDescriptor:renderPipelineDescriptor error:&error];
    if(_renderPipelineState == nil) {
      NSLog(@"%@", error);
      return nil;
    }

    _frameBoundarySemaphore = dispatch_semaphore_create(kMaxInflightTextures);

    OSStatus status;
    status = CMSimpleQueueCreate(CFAllocatorGetDefault(), kMaxInflightTextures, &_dynamicDataTextures);
    if(status != noErr) {
      return nil;
    }
    status = CMSimpleQueueCreate(CFAllocatorGetDefault(), kMaxInflightTextures, &_dynamicFreeTextures);
    if(status != noErr) {
      return nil;
    }

    NSUInteger align = [_device minimumLinearTextureAlignmentForPixelFormat:_pixelFormat];
    NSUInteger pitch = [_device minimumTextureBufferAlignmentForPixelFormat:_pixelFormat];
    CGSize size = NSMakeSize(align, 1);
    NSUInteger bytesPerRow = pitch * size.width;
    for(int i = 0; i < kMaxInflightTextures; i++) {
      id<MTLBuffer> buffer = [_device newBufferWithLength:bytesPerRow * size.height options:MTLResourceStorageModeManaged];

      MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:_pixelFormat width:size.width height:size.height mipmapped:NO];

      id<MTLTexture> texture = [buffer newTextureWithDescriptor:textureDescriptor offset:0 bytesPerRow:bytesPerRow];

      CMSimpleQueueEnqueue(_dynamicFreeTextures, (const void *)CFBridgingRetain(texture));
    }
  }

  return self;
}

- (void) setVertex:(const MetalVertex *)vertices length:(NSUInteger)length
{
  [self setVertexBuffer:[_device newBufferWithBytes:vertices length:length options:MTLResourceStorageModeShared]];
}

- (void) setShader:(const string&)name
{
  MTLSamplerMinMagFilter filter;
  if(name == "None") {
    filter = MTLSamplerMinMagFilterNearest;
  } else if(name == "Blur") {
    filter = MTLSamplerMinMagFilterLinear;
  } else {
    return;	// XXX
  }

  MTLSamplerDescriptor *samplerDescriptor = [MTLSamplerDescriptor alloc];
  [samplerDescriptor setNormalizedCoordinates:YES];
  [samplerDescriptor setMaxAnisotropy:16];
  [samplerDescriptor setMinFilter:filter];
  [samplerDescriptor setMagFilter:filter];

  [self setSamplerState:[_device newSamplerStateWithDescriptor:samplerDescriptor]];
}

- (id<MTLTexture>) dequeueWithTextureSize:(CGSize)size
{
  dispatch_semaphore_wait(_frameBoundarySemaphore, DISPATCH_TIME_FOREVER);

  id<MTLTexture> texture = (id<MTLTexture>)CFBridgingRelease(CMSimpleQueueDequeue(_dynamicFreeTextures));
  if(texture != nil) {
    CGSize textureSize = NSMakeSize([texture width], [texture height]);
    if(size.width == 0 || size.height == 0) size = textureSize;
    if(!CGSizeEqualToSize(textureSize, size)) {
      NSUInteger align = [_device minimumLinearTextureAlignmentForPixelFormat:_pixelFormat];
      NSUInteger pitch = [_device minimumTextureBufferAlignmentForPixelFormat:_pixelFormat];
      NSUInteger bytesPerRow = pitch * size.width;
      if(bytesPerRow % align) {
        bytesPerRow &= ~(align - 1);
        bytesPerRow += align;
        size.width = bytesPerRow / pitch;
        size.height += 1;
      }

      id<MTLBuffer> buffer = [_device newBufferWithLength:bytesPerRow * size.height options:MTLResourceStorageModeManaged];

      MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:_pixelFormat width:size.width height:size.height mipmapped:NO];

      texture = [buffer newTextureWithDescriptor:textureDescriptor offset:0 bytesPerRow:bytesPerRow];
    }
    return texture;
  }
  NSLog(@"No buffer available");
  return nil;
}

- (void) enqueue:(id<MTLTexture>)texture
{
  if(texture != nil) {
    CMSimpleQueueEnqueue(_dynamicDataTextures, (const void *)CFBridgingRetain(texture));
  }
}

- (void) flush
{
  while(CMSimpleQueueGetCount(_dynamicFreeTextures) < kMaxInflightTextures) {
    usleep(1);
  }
}

- (MetalViewport) makeViewportBytes
{
  CGSize dSize = [self drawableSize];
  CGSize vSize = [self viewportSize];
  return {
    .drawableSize = simd_make_float2(dSize.width, dSize.height),
    .viewportSize = simd_make_float2(vSize.width, vSize.height),
  };
}

- (void) render:(MTKView *)view
{
  MetalViewport viewportBytes = [self makeViewportBytes];
  id<MTLTexture> texture = (id<MTLTexture>)CFBridgingRelease(CMSimpleQueueDequeue(_dynamicDataTextures));

  id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];

  MTLRenderPassDescriptor *renderPassDescriptor = [view currentRenderPassDescriptor];
  if(renderPassDescriptor != nil) {
    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

    id<MTLRenderCommandEncoder> renderCommandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    if(texture != nil) {
      [renderCommandEncoder setRenderPipelineState:_renderPipelineState];
      [renderCommandEncoder setVertexBuffer:[self vertexBuffer] offset:0 atIndex:0];
      [renderCommandEncoder setVertexBytes:&viewportBytes length:sizeof(viewportBytes) atIndex:1];
      [renderCommandEncoder setFragmentTexture:texture atIndex:0];
      [renderCommandEncoder setFragmentSamplerState:[self samplerState] atIndex:1];
      [renderCommandEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    }
    [renderCommandEncoder endEncoding];

    [commandBuffer presentDrawable:[view currentDrawable]];

    __block CMSimpleQueueRef queue = _dynamicFreeTextures;
    __block dispatch_semaphore_t semaphore = _frameBoundarySemaphore;
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
      if(texture != nil) {
        CMSimpleQueueEnqueue(queue, (const void *)CFBridgingRetain(texture));
      }
      dispatch_semaphore_signal(semaphore);
      queue = nil;
      semaphore = nil;
    }];
  }

  [commandBuffer commit];
}

@end
