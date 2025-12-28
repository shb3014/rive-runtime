#ifdef VERTEX
g1(c0)i0(0,g,PB);i0(1,g,QB);h1
#endif
D1 q0 N(0,g,C);z1
#ifdef VERTEX
p1(EF,c0,D,p,O){j0(p,D,PB,g);j0(p,D,QB,g);L(C,g);g J;uint Z;c R;if(p7(PB,QB,O,Z,R,C o2)){P B4=A0(LB,Z*4u+2u);X Z6=uintBitsToFloat(B4.yzw);R=R*Z6.x+Z6.yz;J=P7(R,q.ub.x,q.ub.y);
#ifdef POST_INVERT_Y
J.y=-J.y;
#endif
}else{J=g(q.M1,q.M1,q.M1,q.M1);}W(C);l1(J);}
#endif
#ifdef FRAGMENT
#ifdef ATLAS_RENDER_TARGET_R32UI_FRAMEBUFFER_FETCH
layout(location=0)inout highp uvec4 Z5;
#ifdef ATLAS_FEATHERED_FILL
void main(){float m=uintBitsToFloat(Z5.x);m+=T2(C);Z5.x=floatBitsToUint(m);}
#endif
#ifdef ATLAS_FEATHERED_STROKE
void main(){float m=uintBitsToFloat(Z5.x);m=max(m,w3(C));Z5.x=floatBitsToUint(m);}
#endif
#elif defined(ATLAS_RENDER_TARGET_R32UI_PLS_EXT)
__pixel_localEXT m1{layout(r32f)highp float Q2;};
#ifdef ATLAS_FEATHERED_FILL
void main(){Q2+=T2(C);}
#endif
#ifdef ATLAS_FEATHERED_STROKE
void main(){Q2=max(Q2,w3(C));}
#endif
#elif defined(ATLAS_RENDER_TARGET_R32UI_PLS_ANGLE)
layout(binding=0,r32ui)uniform highp upixelLocalANGLE Q2;
#ifdef ATLAS_FEATHERED_FILL
void main(){float m=uintBitsToFloat(pixelLocalLoadANGLE(Q2).x);m+=T2(C);pixelLocalStoreANGLE(Q2,P(floatBitsToUint(m)));}
#endif
#ifdef ATLAS_FEATHERED_STROKE
void main(){float m=uintBitsToFloat(pixelLocalLoadANGLE(Q2).x);m=max(m,w3(C));pixelLocalStoreANGLE(Q2,P(floatBitsToUint(m)));}
#endif
#elif defined(ATLAS_RENDER_TARGET_R32I_ATOMIC_TEXTURE)
layout(binding=0,r32i)uniform highp coherent iimage2D Fc;ivec2 Gc(){return ivec2(floor(v0));}int Hc(float m){return int(m*Pb);}
#ifdef ATLAS_FEATHERED_FILL
void main(){int m=Hc(T2(C));imageAtomicAdd(Fc,Gc(),m);}
#endif
#ifdef ATLAS_FEATHERED_STROKE
void main(){int m=Hc(w3(C));imageAtomicMax(Fc,Gc(),m);}
#endif
#elif defined(ATLAS_RENDER_TARGET_RGBA8_UNORM)
#ifdef ATLAS_FEATHERED_FILL
U1(i,IE){I(C,g);d m=T2(C P0);if(abs(m)>le-1e-3){V1(m>.0?T0(.0,.0,1./255.,.0):T0(.0,.0,.0,1./255.));}else{m*=1./a8;V1(T0(max(m,.0),max(-m,.0),.0,.0));}}
#endif
#ifdef ATLAS_FEATHERED_STROKE
U1(i,JE){I(C,g);d m=w3(C P0);m*=1./a8;V1(T0(m,.0,.0,.0));}
#endif
#else
#ifdef ATLAS_FEATHERED_FILL
U1(float,IE){I(C,g);V1(T2(C P0));}
#endif
#ifdef ATLAS_FEATHERED_STROKE
U1(float,JE){I(C,g);V1(w3(C P0));}
#endif
#endif
#endif
