#ifdef FRAGMENT
v2
#ifndef FIXED_FUNCTION_COLOR_OUTPUT
F0(T3,z0);
#endif
Z0(L4,M0);
#ifndef FIXED_FUNCTION_COLOR_OUTPUT
F0(M6,H2);
#endif
Z0(h6,B1);w2 F1(JB){I(a2,G);d N0=-a2.x;
#ifdef DRAW_INTERIOR_TRIANGLES
L(W0,d);d m0=W0;
#else
L(C,c3);d m0=C.x;
#endif
i2;G I0;d o5,p5;
#if defined(DRAW_INTERIOR_TRIANGLES)&&defined(BORROWED_COVERAGE_PASS)
if(BORROWED_COVERAGE_PASS){p5=m0;}else
#endif
{I0=unpackHalf2x16(Y0(M0));o5=I0.y;d G4=o5==N0?I0.x:k1(.0);p5=G4+m0;}
#ifdef ENABLE_NESTED_CLIPPING
d n5=a2.y;if(ENABLE_NESTED_CLIPPING&&n5!=.0){d P3=.0;
#if defined(DRAW_INTERIOR_TRIANGLES)&&defined(BORROWED_COVERAGE_PASS)
if(BORROWED_COVERAGE_PASS){I0=unpackHalf2x16(Y0(M0));o5=I0.y;}
#endif
if(o5!=N0){P3=o5==n5?I0.x:.0;d1(B1,packHalf2x16(f3(P3,qe)));}else{P3=unpackHalf2x16(Y0(B1)).x;Q1(B1);}p5=min(p5,P3);}else
#endif
{Q1(B1);}d1(M0,packHalf2x16(f3(p5,N0)));
#ifndef FIXED_FUNCTION_COLOR_OUTPUT
h2(z0);
#endif
j2;x2;}
#endif
