#ifdef VERTEX
void main(){gl_Position=vec4(mix(vec2(-1,1),vec2(1,-1),equal(gl_VertexID&ivec2(1,2),ivec2(0))),0,1);
#ifdef POST_INVERT_Y
gl_Position.y=-gl_Position.y;
#endif
}
#endif
#ifdef FRAGMENT
#extension GL_EXT_shader_pixel_local_storage:require
#ifdef GL_ARM_shader_framebuffer_fetch
#extension GL_ARM_shader_framebuffer_fetch:require
#else
#extension GL_EXT_shader_framebuffer_fetch:require
#endif
#ifdef CLEAR_COLOR
#if __VERSION__>=310
layout(binding=0,std140)uniform dh{uniform highp vec4 Af;}Bf;
#else
uniform mediump vec4 GE;
#endif
#endif
#ifdef GL_EXT_shader_pixel_local_storage
#ifdef STORE_COLOR
__pixel_local_inEXT m1
#else
__pixel_local_outEXT m1
#endif
{layout(rgba8)mediump vec4 z0;layout(r32ui)highp uint M0;layout(rgba8)mediump vec4 H2;layout(r32ui)highp uint j7;};
#ifndef GL_ARM_shader_framebuffer_fetch
#ifdef LOAD_COLOR
layout(location=0)inout mediump vec4 ma;
#endif
#endif
#ifdef STORE_COLOR
layout(location=0)out mediump vec4 ma;
#endif
void main(){
#ifdef CLEAR_COLOR
#if __VERSION__>=310
z0=Bf.Af;
#else
z0=GE;
#endif
#endif
#ifdef LOAD_COLOR
#ifdef GL_ARM_shader_framebuffer_fetch
z0=gl_LastFragColorARM;
#else
z0=ma;
#endif
#endif
#ifdef CLEAR_COVERAGE
j7=0u;
#endif
#ifdef CLEAR_CLIP
M0=0u;
#endif
#ifdef STORE_COLOR
ma=z0;
#endif
}
#else
layout(location=0)out mediump vec4 Cf;void main(){Cf=vec4(0,1,0,1);}
#endif
#endif
