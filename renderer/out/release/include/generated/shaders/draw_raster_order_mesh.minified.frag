#ifdef FRAGMENT
#if defined(FIXED_FUNCTION_COLOR_OUTPUT)&&!defined(ENABLE_CLIPPING)
#undef Aa
#else
#define Aa
#endif
v2
#ifndef FIXED_FUNCTION_COLOR_OUTPUT
F0(T3,z0);
#endif
Z0(L4,M0);
#ifndef FIXED_FUNCTION_COLOR_OUTPUT
F0(M6,H2);
#endif
Z0(h6,B1);w2
#ifdef DRAW_IMAGE_MESH
W2 z2(l3,w6,UB);X2 i4 A3(l3,x6,z3)j4 Y3 Z3
#endif
#ifdef FIXED_FUNCTION_COLOR_OUTPUT
#ifdef DRAW_IMAGE_MESH
c4(JB)
#else
M2(JB)
#endif
#else
#ifdef DRAW_IMAGE_MESH
y5(JB)
#else
F1(JB)
#endif
#endif
{
#ifdef ATLAS_BLIT
I(o1,g);I(i1,c);
#endif
#ifdef DRAW_IMAGE_MESH
I(r0,c);
#endif
#ifdef ENABLE_CLIPPING
I(r3,d);
#endif
#ifdef ENABLE_CLIP_RECT
I(S0,g);
#endif
#if defined(ATLAS_BLIT)&&defined(ENABLE_ADVANCED_BLEND)
I(m2,d);
#endif
#ifdef ATLAS_BLIT
i j=J8(o1,1. U2);d m=I7(i1,q.d4 P0);
#endif
#ifdef DRAW_IMAGE_MESH
i j=c7(UB,z3,r0,q.vb);d m=1.;
#endif
#ifdef ENABLE_CLIP_RECT
if(ENABLE_CLIP_RECT){d R4=max(J7(F5(S0)),k1(.0));m=min(R4,m);}
#endif
#ifdef Aa
i2;
#endif
#ifdef ENABLE_CLIPPING
if(ENABLE_CLIPPING&&r3!=.0){G I0=unpackHalf2x16(Y0(M0));d e6=I0.y;d p5=max(e6==r3?I0.x:k1(.0),k1(.0));m=min(m,p5);}
#endif
#ifdef DRAW_IMAGE_MESH
m*=l0.V2;
#endif
#ifndef FIXED_FUNCTION_COLOR_OUTPUT
i E1=X0(z0);
#ifdef ENABLE_ADVANCED_BLEND
if(ENABLE_ADVANCED_BLEND){
#ifdef ATLAS_BLIT
V e2=C6(m2);
#endif
#ifdef DRAW_IMAGE_MESH
j.xyz=H4(j);V e2=R1(l0.e2);
#endif
if(e2!=r5){j.xyz=v5(j.xyz,E1,e2);}j.w*=m;j.xyz*=j.w;}else
#endif
{j*=m;}
#ifdef NEEDS_GAMMA_CORRECTION
if(NEEDS_GAMMA_CORRECTION){j=h3(j);}
#endif
c1(z0,E1*(1.-j.w)+j);
#endif
Q1(M0);Q1(B1);
#ifdef Aa
j2;
#endif
#ifdef FIXED_FUNCTION_COLOR_OUTPUT
r1=j*m;
#endif
x2;}
#endif
