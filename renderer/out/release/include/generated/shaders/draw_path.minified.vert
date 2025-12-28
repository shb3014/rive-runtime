#undef d6
#ifdef ENABLE_ADVANCED_BLEND
#define d6 !ENABLE_ADVANCED_BLEND
#else
#define d6 true
#endif
#undef c3
#ifdef ENABLE_FEATHER
#define c3 g
#else
#define c3 G
#endif
#ifdef VERTEX
g1(c0)
#if defined(DRAW_INTERIOR_TRIANGLES)||defined(ATLAS_BLIT)
i0(0,R3,KB);
#else
i0(0,g,PB);i0(1,g,QB);
#endif
h1
#endif
D1 q0 N(0,g,o1);
#ifdef ATLAS_BLIT
q0 N(1,c,i1);
#elif!defined(RENDER_MODE_MSAA)
#ifdef DRAW_INTERIOR_TRIANGLES
OPTIONALLY_FLAT N(1,d,W0);
#else
q0 N(2,c3,C);
#endif
OPTIONALLY_FLAT N(3,d,e0);
#endif
#ifdef ENABLE_CLIPPING
#ifdef ATLAS_BLIT
OPTIONALLY_FLAT N(4,d,r3);
#else
OPTIONALLY_FLAT N(4,G,a2);
#endif
#endif
#if defined(ENABLE_CLIP_RECT)&&!defined(RENDER_MODE_MSAA)
q0 N(5,g,S0);
#endif
#ifdef ENABLE_ADVANCED_BLEND
OPTIONALLY_FLAT N(6,d,m2);
#endif
z1
#ifdef VERTEX
p1(TB,c0,D,p,O){
#if defined(DRAW_INTERIOR_TRIANGLES)||defined(ATLAS_BLIT)
j0(p,D,KB,X);
#else
j0(p,D,PB,g);j0(p,D,QB,g);
#endif
L(o1,g);
#ifdef ATLAS_BLIT
L(i1,c);
#elif!defined(RENDER_MODE_MSAA)
#ifdef DRAW_INTERIOR_TRIANGLES
L(W0,d);
#else
L(C,c3);
#endif
L(e0,d);
#endif
#ifdef ENABLE_CLIPPING
#ifdef ATLAS_BLIT
L(r3,d);
#else
L(a2,G);
#endif
#endif
#if defined(ENABLE_CLIP_RECT)&&!defined(RENDER_MODE_MSAA)
L(S0,g);
#endif
#ifdef ENABLE_ADVANCED_BLEND
L(m2,d);
#endif
bool Zc=false;uint Z;c R;
#ifdef RENDER_MODE_MSAA
V H8;
#endif
#ifdef ATLAS_BLIT
R=R8(KB,Z,
#ifdef RENDER_MODE_MSAA
H8,
#endif
i1 o2);
#elif defined(DRAW_INTERIOR_TRIANGLES)
R=S8(KB,Z
#ifdef RENDER_MODE_MSAA
,H8
#else
,W0
#endif
o2);
#else
g B;Zc=!p7(PB,QB,O,Z,R
#ifndef RENDER_MODE_MSAA
,B
#else
,H8
#endif
o2);
#ifndef RENDER_MODE_MSAA
#ifdef ENABLE_FEATHER
C=B;
#else
C.xy=q7(B.xy);
#endif
#endif
#endif
a1 O0=Q4(HC,Z);
#if!defined(ATLAS_BLIT)&&!defined(RENDER_MODE_MSAA)
e0=S7(Z,q.L5);if((O0.x&e9)!=0u)e0=-e0;
#endif
uint S1=O0.x&0xfu;
#ifdef ENABLE_CLIPPING
if(ENABLE_CLIPPING){uint hg=(S1==B7?O0.y:O0.x)>>16;d N0=S7(hg,q.L5);if(S1==B7)N0=-N0;
#ifdef ATLAS_BLIT
r3=N0;
#else
a2.x=N0;
#endif
}
#endif
#ifdef ENABLE_ADVANCED_BLEND
if(ENABLE_ADVANCED_BLEND){m2=float((O0.x>>4)&0xfu);}
#endif
c o7=R;
#ifdef FRAMEBUFFER_BOTTOM_UP
o7.y=float(q.ee)-o7.y;
#endif
#ifdef ENABLE_CLIP_RECT
if(ENABLE_CLIP_RECT){d0 c2=N1(A0(NB,Z*4u+2u));g p2=A0(NB,Z*4u+3u);
#ifndef RENDER_MODE_MSAA
S0=w7(c2,p2.xy,o7);
#else
yb(c2,p2.xy,o7 f5);
#endif
}
#endif
if(S1==f9){i j=unpackUnorm4x8(O0.y);if(d6)j.xyz*=j.w;o1=g(j);}
#if defined(ENABLE_CLIPPING)&&!defined(ATLAS_BLIT)
else if(ENABLE_CLIPPING&&S1==B7){d n5=S7(O0.x>>16,q.L5);a2.y=n5;}
#endif
else{d0 ig=N1(A0(NB,Z*4u));g I8=A0(NB,Z*4u+1u);c S2=H0(ig,o7)+I8.xy;if(S1==C7||S1==re){o1.w=-uintBitsToFloat(O0.y);float jg=I8.z;if(jg>.9){o1.z=2.;}else{o1.z=I8.w;}if(S1==C7){o1.y=.0;o1.x=S2.x;}else{o1.z=-o1.z;o1.xy=S2.xy;}}else{float V2=uintBitsToFloat(O0.y);float O6=I8.z;o1=g(S2.x,S2.y,V2,-2.-O6);}}g J;if(!Zc){J=R2(R);
#ifdef POST_INVERT_Y
J.y=-J.y;
#endif
#ifdef RENDER_MODE_MSAA
J.z=H9(H8);
#endif
}else{J=g(q.M1,q.M1,q.M1,q.M1);}W(o1);
#ifdef ATLAS_BLIT
W(i1);
#elif!defined(RENDER_MODE_MSAA)
#ifdef DRAW_INTERIOR_TRIANGLES
W(W0);
#else
W(C);
#endif
W(e0);
#endif
#ifdef ENABLE_CLIPPING
#ifdef ATLAS_BLIT
W(r3);
#else
W(a2);
#endif
#endif
#if defined(ENABLE_CLIP_RECT)&&!defined(RENDER_MODE_MSAA)
W(S0);
#endif
#ifdef ENABLE_ADVANCED_BLEND
W(m2);
#endif
l1(J);}
#endif
#ifdef FRAGMENT
Y3 Z3 e i J8(g d3,float m i6){i j;if(d3.w>=.0){j=F5(d3);if(d6)j*=m;else j.w*=m;}else if(d3.w>-1.){float t=d3.z>.0?d3.x:length(d3.xy);t=clamp(t,.0,1.);float ad=abs(d3.z);float x=ad>1.?(1.-1./I9)*t+(.5/I9):(1./I9)*t+ad;float kg=-d3.w;j=d2(VC,g9,c(x,kg),.0);j.w*=m;if(d6)j.xyz*=j.w;}else{d O6=-d3.w-2.;j=r8(UB,z3,d3.xy,O6);d V2=d3.z*m;if(d6)j*=V2;else j=T0(H4(j),j.w*V2);}return j;}
#if!defined(DRAW_INTERIOR_TRIANGLES)&&!defined(ATLAS_BLIT)
e d bd(c3 B p3){
#ifdef ENABLE_FEATHER
if(ENABLE_FEATHER&&i9(B))return w3(B P0);else
#endif
return min(B.x,B.y);}e d cd(c3 B p3){
#if defined(ENABLE_FEATHER)
if(ENABLE_FEATHER&&F7(B))return T2(B P0);else
#endif
return B.x;}e d lg(c3 B p3){if(B5(B))return bd(B P0);else return cd(B P0);}e d mg(d G4,c3 B p3){if(B5(B)){d m0=bd(B P0);return max(m0,G4);}else{d m0=cd(B P0);return G4+m0;}}
#endif
#endif
