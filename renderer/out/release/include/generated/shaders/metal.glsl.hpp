#pragma once

#include "metal.glsl.exports.h"

namespace rive {
namespace gpu {
namespace glsl {
const char metal[] = R"===(#ifndef _ARE_TOKEN_NAMES_PRESERVED
#define d half
#define G half2
#define r half3
#define i half4
#define V ushort
#define c float2
#define X float3
#define R3 packed_float3
#define g float4
#define n4 bool2
#define X5 bool3
#define a7 bool4
#define a1 uint2
#define P uint4
#define Y int2
#define p4 int4
#define V ushort
#define d0 float2x2
#define D6 half3x3
#define E6 half2x3
#define o4 half4x4
#endif
#define e inline
#define A1(Z1) thread Z1&
#define O4(Z1) thread Z1&
#define equal(A,F) ((A)==(F))
#define notEqual(A,F) ((A)!=(F))
#define lessThan(A,F) ((A)<(F))
#define greaterThan(A,F) ((A)>(F))
#define H0(A,F) ((A)*(F))
#define inversesqrt rsqrt
#define M5(f,a) struct a{
#define G6(a) };
#define g1(a) struct a{
#define i0(f,a0,a) a0 a
#define h1 };
#define j0(o8,D,a,a0) a0 a=D[o8].a
#define D1 struct k0{
#define N(f,a0,a) a0 a
#define e3 [[flat]]
#define q0 [[center_no_perspective]]
#ifndef RB
#define RB
#endif
#define z1 g U0[[position]][[invariant]];};
#define L(a,a0) thread a0&a=U.a
#define W(a)
#define I(a,a0) a0 a=U.a
#define g4 struct y8{
#define h4 };
#define Y3 struct k5{
#define Z3 };
#define M4(f,x1,a) constant a1*a[[buffer(J0(f))]]
#define v4(f,x1,a) constant P*a[[buffer(J0(f))]]
#define N4(f,x1,a) constant g*a[[buffer(J0(f))]]
#define A0(a,w0) k2.a[w0]
#define Q4(a,w0) k2.a[w0]
#define j3 struct z8{
#define k3 };
#define W2 struct M3{
#define X2 };
#define i4 struct g7{
#define j4 };
#define l4(M,f,a) [[texture(f)]]texture2d<uint>a
#define T4(M,f,a) [[texture(f)]]texture2d<float>a
#define z2(M,f,a) [[texture(f)]]texture2d<d>a
#define W4(M,f,a) [[texture(f)]]texture2d<d>a
#define Q5(M,f,a) [[texture(f)]]texture1d_array<d>a
#define w4(c5,a) constexpr sampler a(filter::linear,mip_filter::none);
#define A3(M,f,a) [[sampler(f)]]sampler a;
#define G1(g0,k) K0.g0.read(a1(k))
#define d5(g0,n,k) K0.g0.sample(n,k)
#define d2(g0,n,k,Q0) K0.g0.sample(n,k,level(Q0))
#define e5(g0,n,k,K1) K0.g0.sample(n,k,bias(K1))
#define x4(g0,n,k,q3) K0.g0.gather(n,(k)*(q3))
#define l6(g0,n,k) K0.g0.sample(D4.n,k)
#define r8(g0,n,k,Q0) K0.g0.sample(D4.n,k,level(Q0))
#define c7(g0,n,k,K1) K0.g0.sample(D4.n,k,bias(K1))
#define B6(g0,n,o,Y5,v8,Q0) K0.g0.sample(n,o,Y5)
#define U5 ,constant SB&q,z8 K0,y8 k2
#define o2 ,q,K0,k2
#ifdef CE
#define p1(a,c0,D,p,O) __attribute__((visibility("default")))k0 vertex a(uint p[[vertex_id]],uint O[[instance_id]],constant uint&wf[[buffer(J0(Kb))]],constant SB&q[[buffer(J0(Y2))]],constant c0*D[[buffer(0)]],z8 K0,y8 k2){O+=wf;k0 U;
#else
#define p1(a,c0,D,p,O) __attribute__((visibility("default")))k0 vertex a(uint p[[vertex_id]],uint O[[instance_id]],constant SB&q[[buffer(J0(Y2))]],constant c0*D[[buffer(0)]],z8 K0,y8 k2){k0 U;
#endif
#define r7(a,c0,D,p,O) __attribute__((visibility("default")))k0 vertex a(uint p[[vertex_id]],constant SB&q[[buffer(J0(Y2))]],constant KC&l0[[buffer(J0(N5))]],constant c0*D[[buffer(0)]],z8 K0,y8 k2){k0 U;
#define w5(a,q2,r2,J2,K2,p) __attribute__((visibility("default")))k0 vertex a(uint p[[vertex_id]],constant SB&q[[buffer(J0(Y2))]],constant KC&l0[[buffer(J0(N5))]],constant q2*r2[[buffer(0)]],constant J2*K2[[buffer(1)]]){k0 U;
#define l1(j5) U.U0=j5;}return U;
#define U1(L3,a) L3 __attribute__((visibility("default")))fragment a(k0 U[[stage_in]],M3 K0){
#define V1(E) return E;}
#define i6 ,c v0,M3 K0,k5 k2,g7 D4
#define U2 ,v0,K0,k2,D4
#define p3 ,M3 K0
#define P0 ,K0
#define I6
#define f5
#ifdef BF
#define v2 struct m1{
#ifdef CF
#define F0(f,a) device uint*a[[buffer(J0(f+O5)),raster_order_group(0)]]
#define Z0(f,a) device uint*a[[buffer(J0(f+O5)),raster_order_group(0)]]
#define W3(f,a) device atomic_uint*a[[buffer(J0(f+O5)),raster_order_group(0)]]
#else
#define F0(f,a) device uint*a[[buffer(J0(f+O5))]]
#define Z0(f,a) device uint*a[[buffer(J0(f+O5))]]
#define W3(f,a) device atomic_uint*a[[buffer(J0(f+O5))]]
#endif
#define w2 };
#define a4 ,m1 L0,uint f1
#define P1 ,L0,f1
#define X0(h) unpackUnorm4x8(L0.h[f1])
#define Y0(h) L0.h[f1]
#define x3(h) atomic_load_explicit(&L0.h[f1],memory_order::memory_order_relaxed)
#define c1(h,E) L0.h[f1]=packUnorm4x8(E)
#define d1(h,E) L0.h[f1]=(E)
#define y3(h,E) atomic_store_explicit(&L0.h[f1],E,memory_order::memory_order_relaxed)
#define h2(h)
#define Q1(h)
#define A5(h,o) atomic_fetch_max_explicit(&L0.h[f1],o,memory_order::memory_order_relaxed)
#define C5(h,o) atomic_fetch_add_explicit(&L0.h[f1],o,memory_order::memory_order_relaxed)
#define i2
#define j2
#define h7(a) __attribute__((visibility("default")))fragment a(m1 L0,constant SB&q[[buffer(J0(Y2))]],k0 U[[stage_in]],M3 K0,g7 D4,k5 k2){c v0=U.U0.xy;a1 H=a1(metal::floor(v0));uint f1=H.y*q.F6+H.x;
#define Dc(a) __attribute__((visibility("default")))fragment a(m1 L0,constant SB&q[[buffer(J0(Y2))]],constant KC&l0[[buffer(J0(N5))]],k0 U[[stage_in]],g7 D4,M3 K0,k5 k2){c v0=U.U0.xy;a1 H=a1(metal::floor(v0));uint f1=H.y*q.F6+H.x;
#define F1(a) void h7(a)
#define y5(a) void Dc(a)
#define x2 }
#define M2(a) i h7(a){i r1;
#define c4(a) i Dc(a){i r1;
#define S4 }return r1;x2
#else
#define v2 struct m1{
#define F0(f,a) [[color(f)]]i a
#define Z0(f,a) [[color(f)]]uint a
#define W3 Z0
#define w2 };
#define a4 ,thread m1&N3,thread m1&L0
#define P1 ,N3,L0
#define X0(h) N3.h
#define Y0(h) N3.h
#define x3(h) Y0
#define c1(h,E) L0.h=(E)
#define d1(h,E) L0.h=(E)
#define y3(h) d1
#define h2(h) L0.h=N3.h
#define Q1(h) L0.h=N3.h
e uint h5(thread uint&n0,uint x){uint V0=n0;n0=metal::max(V0,x);return V0;}
#define A5(h,o) h5(L0.h,o)
e uint i5(thread uint&n0,uint x){uint V0=n0;n0=V0+x;return V0;}
#define C5(h,o) i5(L0.h,o)
#define i2
#define j2
#define h7(a,...) m1 __attribute__((visibility("default")))fragment a(__VA_ARGS__){c v0[[maybe_unused]]=U.U0.xy;m1 L0;
#define F1(a,...) h7(a,m1 N3,constant SB&q[[buffer(J0(Y2))]],k0 U[[stage_in]],g7 D4,M3 K0,k5 k2)
#define y5(a) h7(a,m1 N3,constant SB&q[[buffer(J0(Y2))]],k0 U[[stage_in]],M3 K0,k5 k2,g7 D4,constant KC&l0[[buffer(J0(N5))]])
#define x2 }return L0;
#define Ec(a,...) struct xf{i yf[[j(0)]];m1 L0;};xf __attribute__((visibility("default")))fragment a(__VA_ARGS__){c v0[[maybe_unused]]=U.U0.xy;i r1;m1 L0;
#define M2(a) Ec(a,m1 N3,constant SB&q[[buffer(J0(Y2))]],k0 U[[stage_in]],M3 K0,k5 k2)
#define c4(a) Ec(a,m1 N3,constant SB&q[[buffer(J0(Y2))]],k0 U[[stage_in]],M3 K0,k5 k2,__VA_ARGS__ constant KC&l0[[buffer(J0(N5))]])
#define S4 }return{.yf=r1,.L0=L0};
#endif
#define U3 F0
#define discard discard_fragment()
using namespace metal;template<int J1>e vec<uint,J1>floatBitsToUint(vec<float,J1>x){return as_type<vec<uint,J1>>(x);}template<int J1>e vec<int,J1>floatBitsToInt(vec<float,J1>x){return as_type<vec<int,J1>>(x);}e uint floatBitsToUint(float x){return as_type<uint>(x);}e int floatBitsToInt(float x){return as_type<int>(x);}template<int J1>e vec<float,J1>uintBitsToFloat(vec<uint,J1>x){return as_type<vec<float,J1>>(x);}e float uintBitsToFloat(uint x){return as_type<float>(x);}e G unpackHalf2x16(uint x){return as_type<G>(x);}e uint packHalf2x16(G x){return as_type<uint>(x);}e i unpackUnorm4x8(uint x){return unpack_unorm4x8_to_half(x);}e uint packUnorm4x8(i x){return pack_half_to_unorm4x8(x);}e d0 inverse(d0 n1){d0 la=d0(n1[1][1],-n1[0][1],-n1[1][0],n1[0][0]);float zf=(la[0][0]*n1[0][0])+(la[0][1]*n1[1][0]);return la*(1/zf);}e r mix(r l,r b,X5 x0){r i7;for(int y0=0;y0<3;++y0)i7[y0]=x0[y0]?b[y0]:l[y0];return i7;}e c mix(c l,c b,n4 x0){c i7;for(int y0=0;y0<2;++y0)i7[y0]=x0[y0]?b[y0]:l[y0];return i7;}e c mix(c l,c b,float t){return mix(l,b,c(t));}e float mod(float x,float y){return fmod(x,y);}
)===";
} // namespace glsl
} // namespace gpu
} // namespace rive