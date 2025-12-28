#ifdef FRAGMENT
v2
#ifndef FIXED_FUNCTION_COLOR_OUTPUT
F0(T3,z0);
#endif
Z0(L4,M0);
#ifndef FIXED_FUNCTION_COLOR_OUTPUT
F0(M6,H2);
#endif
Z0(h6,B1);w2
#ifdef FIXED_FUNCTION_COLOR_OUTPUT
M2(JB)
#else
F1(JB)
#endif
{I(o1,g);
#ifdef DRAW_INTERIOR_TRIANGLES
L(W0,d);
#else
L(C,c3);
#endif
I(e0,d);
#ifdef ENABLE_CLIPPING
I(a2,G);
#endif
#ifdef ENABLE_CLIP_RECT
I(S0,g);
#endif
#ifdef ENABLE_ADVANCED_BLEND
I(m2,d);
#endif
d m0=
#ifdef DRAW_INTERIOR_TRIANGLES
W0;
#else
lg(C);
#endif
i D0;d v3;
#if defined(DRAW_INTERIOR_TRIANGLES)&&defined(BORROWED_COVERAGE_PASS)
if(!BORROWED_COVERAGE_PASS)
#endif
{D0=J8(o1,1. U2);v3=1.;
#ifdef ENABLE_CLIP_RECT
if(ENABLE_CLIP_RECT){d og=J7(F5(S0));v3=min(og,v3);}
#endif
}i2;
#if defined(DRAW_INTERIOR_TRIANGLES)&&defined(BORROWED_COVERAGE_PASS)
if(BORROWED_COVERAGE_PASS){d1(B1,packHalf2x16(f3(m0,e0)));
#ifndef FIXED_FUNCTION_COLOR_OUTPUT
h2(z0);
#endif
}else
#endif
{G Z2=unpackHalf2x16(Y0(B1));d K8=Z2.y;d G4=K8==e0?Z2.x:k1(.0);d dd=
#ifndef DRAW_INTERIOR_TRIANGLES
B5(C)?max(G4,m0):
#endif
G4+m0;
#ifdef ENABLE_CLIPPING
if(ENABLE_CLIPPING&&a2.x!=.0){G I0=unpackHalf2x16(Y0(M0));d o5=I0.y;d pg=o5==a2.x?I0.x:k1(.0);v3=min(pg,v3);}
#endif
v3=max(v3,.0);d za=C9(G4,.0,v3);d H1=C9(dd,.0,v3);
#ifndef FIXED_FUNCTION_COLOR_OUTPUT
i E1=X0(z0);
#ifdef ENABLE_ADVANCED_BLEND
if(ENABLE_ADVANCED_BLEND){if(m2!=J5(r5)&&H1!=.0){if(za==.0){D0.xyz=v5(D0.xyz,E1,C6(m2));
#ifndef DRAW_INTERIOR_TRIANGLES
if(H1<v3){c1(H2,D0);}
#endif
}else{D0=X0(H2);h2(H2);}}D0.xyz*=D0.w;}
#endif
#endif
D0*=(H1-za)/max(1.-za*D0.w,xe);
#ifndef DRAW_INTERIOR_TRIANGLES
#ifdef ENABLE_ADVANCED_BLEND
#define ed (!ENABLE_ADVANCED_BLEND||m2==J5(r5))&&D0.w>=1.
#else
#define ed D0.w>=1.
#endif
zc(ed,B1,packHalf2x16(f3(dd,e0)));
#else
Q1(B1);
#endif
#ifndef FIXED_FUNCTION_COLOR_OUTPUT
yc(D0.w==.0,z0,E1*(1.-D0.w)+D0);
#endif
}Q1(M0);j2;
#ifdef FIXED_FUNCTION_COLOR_OUTPUT
r1=D0;
#endif
x2;}
#endif
