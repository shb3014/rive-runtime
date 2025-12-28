#ifdef VERTEX
g1(q2)i0(0,c,XB);h1 g1(J2)i0(1,c,YB);h1
#endif
D1 q0 N(0,c,r0);
#ifdef ENABLE_CLIPPING
OPTIONALLY_FLAT N(1,d,r3);
#endif
#if defined(ENABLE_CLIP_RECT)&&!defined(RENDER_MODE_MSAA)
q0 N(2,g,S0);
#endif
z1
#ifdef VERTEX
j3 k3 w5(TB,q2,r2,J2,K2,p){j0(p,r2,XB,c);j0(p,K2,YB,c);L(r0,c);
#ifdef ENABLE_CLIPPING
L(r3,d);
#endif
#if defined(ENABLE_CLIP_RECT)&&!defined(RENDER_MODE_MSAA)
L(S0,g);
#endif
c R=H0(N1(l0.v7),XB)+l0.j1;r0=YB;
#ifdef ENABLE_CLIPPING
if(ENABLE_CLIPPING){r3=S7(l0.N0,q.L5);}
#endif
#ifdef ENABLE_CLIP_RECT
if(ENABLE_CLIP_RECT){
#ifndef RENDER_MODE_MSAA
S0=w7(N1(l0.c2),l0.p2,R f5);
#else
yb(N1(l0.c2),l0.p2,R f5);
#endif
}
#endif
g J=R2(R);
#ifdef POST_INVERT_Y
J.y=-J.y;
#endif
#ifdef RENDER_MODE_MSAA
J.z=H9(l0.H6);
#endif
W(r0);
#ifdef ENABLE_CLIPPING
W(r3);
#endif
#if defined(ENABLE_CLIP_RECT)&&!defined(RENDER_MODE_MSAA)
W(S0);
#endif
l1(J);}
#endif
