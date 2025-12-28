#ifdef FRAGMENT
#ifdef DRAW_IMAGE_MESH
W2 z2(l3,w6,UB);
#ifdef ENABLE_ADVANCED_BLEND
Q6(ID);
#endif
X2 i4 A3(l3,x6,z3)j4
#endif
U1(i,JB){
#ifdef DRAW_IMAGE_MESH
I(r0,c);
#else
I(o1,g);
#ifdef ATLAS_BLIT
I(i1,c);
#endif
#ifdef ENABLE_ADVANCED_BLEND
I(m2,d);
#endif
#endif
#ifdef DRAW_IMAGE_MESH
i j=c7(UB,z3,r0,q.vb)*l0.V2;
#else
d m=
#ifdef ATLAS_BLIT
I7(i1,q.d4 P0);
#else
1.;
#endif
i j=J8(o1,m U2);
#endif
#ifdef ENABLE_ADVANCED_BLEND
if(ENABLE_ADVANCED_BLEND){
#ifndef FIXED_FUNCTION_COLOR_OUTPUT
#ifdef DRAW_IMAGE_MESH
j.xyz=H4(j);V e2=R1(l0.e2);
#else
V e2=C6(m2);
#endif
i E1=x8(ID);j.xyz=v5(j.xyz,E1,e2);
#endif
j.xyz*=j.w;}
#endif
#ifdef NEEDS_GAMMA_CORRECTION
if(NEEDS_GAMMA_CORRECTION){j=h3(j);}
#endif
V1(j);}
#endif
