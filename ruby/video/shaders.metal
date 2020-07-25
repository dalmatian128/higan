#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct MetalVertexIn {
  simd_float4 deviceCoordinate;
  simd_float2 textureCoordinate;
};

struct MetalViewport
{
  simd_float2 drawableSize;
  simd_float2 viewportSize;
};

struct MetalVertexOut {
  // The [[position]] attribute qualifier of this member indicates this value is
  // the clip space position of the vertex when this structure is returned from
  // the vertex shader
  float4 position [[position]];

  // Since this member does not have a special attribute qualifier, the rasterizer
  // will interpolate its value with values of other vertices making up the triangle
  // and pass that interpolated value to the fragment shader for each fragment in
  // that triangle.
  float2 textureCoordinate;

};

// Vertex Function
vertex MetalVertexOut
MetalVertexShader(uint id [[ vertex_id ]], constant MetalVertexIn *vertices [[ buffer(0) ]], constant MetalViewport &viewport [[ buffer(1) ]])
{
  MetalVertexOut out;
  out.position.xy = vertices[id].deviceCoordinate.xy * viewport.viewportSize / viewport.drawableSize;
  out.position.zw = vertices[id].deviceCoordinate.zw;
  out.textureCoordinate = vertices[id].textureCoordinate;
  return out;
}

fragment float4
MetalFragmentShader(MetalVertexOut in [[stage_in]], texture2d<float> colorTexture [[ texture(0) ]], sampler textureSampler [[ sampler(1) ]])
{
  return colorTexture.sample(textureSampler, in.textureCoordinate);
}
