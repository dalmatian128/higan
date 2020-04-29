#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct MetalVertexIn {
  simd_float2 deviceCoordinate;
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
MetalVertexShader(uint vertexID [[ vertex_id ]], constant MetalVertexIn *vertices [[ buffer(0) ]], constant MetalViewport &viewport [[ buffer(1) ]])
{
  MetalVertexOut out;
  out.position.xy = vertices[vertexID].deviceCoordinate * viewport.viewportSize / viewport.drawableSize;
  out.position.zw = simd_float2(0.0, 1.0);
  out.textureCoordinate = vertices[vertexID].textureCoordinate;
  return out;
}

fragment float4
MetalFragmentShader(MetalVertexOut in [[stage_in]], texture2d<float> colorTexture [[ texture(0) ]], sampler textureSampler [[ sampler(1) ]])
{
  return colorTexture.sample(textureSampler, in.textureCoordinate);
}
