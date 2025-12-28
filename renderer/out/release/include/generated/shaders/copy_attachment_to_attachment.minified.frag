#ifdef FRAGMENT
layout(input_attachment_index=0,binding=we,set=q4)uniform lowp subpassInput ng;layout(location=0)out i ya;void main(){ya=subpassLoad(ng);}
#endif
