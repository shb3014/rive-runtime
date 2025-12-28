#ifdef FRAGMENT
layout(input_attachment_index=0,binding=T3,set=q4)uniform lowp subpassInputMS L8;layout(location=0)out i ya;void main(){ya=(subpassLoad(L8,0)+subpassLoad(L8,1)+subpassLoad(L8,2)+subpassLoad(L8,3))*.25;}
#endif
