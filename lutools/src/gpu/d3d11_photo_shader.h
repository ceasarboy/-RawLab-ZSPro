#pragma once
namespace sony2fuji {
inline constexpr const char* kD3DPhotoShader = R"hlsl(
cbuffer Params : register(b0) {
    uint4 dimensions; // source width/height, destination width/height
    uint4 control; // stage, count, LUT mode (0 none, 1 F-Log2 input, 2 display input), LUT size
    uint4 lutBInfo; // stacked-look slot: enabled, size, unused, unused
    float4 toSrgb[3];
    float4 toFilm[3];
    float4 exposureStrength;
    float4 tone; // contrast, saturation, highlights, shadows
    float4 detail; // curve, noise, sharpening, unused
    float4 domainLow;
    float4 domainHigh;
};
StructuredBuffer<float3> source : register(t0);
StructuredBuffer<float3> film : register(t1);
StructuredBuffer<float3> filmB : register(t2);
RWStructuredBuffer<float3> destination : register(u0);
float neutral(float x) {
    if (x <= 0) return 0;
    float gray=0.1845;
    float scene=1/(1+(1-gray)/gray*pow(gray/x,1.5));
    return scene<=0.0031308 ? scene*12.92 : 1.055*pow(scene,1/2.4)-0.055;
}
float flog2(float x) {
    return saturate(x<0.00088899597 ? 8.799461*x+0.092864 : log10(x*5.555556+0.064829)*0.245281+0.384316);
}
float3 sampleLut(uint3 p) { return film[(p.z*control.w+p.y)*control.w+p.x]; }
float3 sampleLutB(uint3 p) { return filmB[(p.z*lutBInfo.y+p.y)*lutBInfo.y+p.x]; }
// tetrahedral: a>=b>=c sorted fractional coords; 4 corners along the cell main diagonal
float3 tetra(float3 c0,float3 c1,float3 c2,float3 c3,float a,float b,float c) {
    return (1-a)*c0+(a-b)*c1+(b-c)*c2+c*c3;
}
float3 lookupB(float3 x) {
    float3 p=saturate((x-domainLow.xyz)/(domainHigh.xyz-domainLow.xyz))*(lutBInfo.y-1);
    uint3 low=(uint3)floor(p),high=min(low+1,lutBInfo.y-1);float3 t=p-low;
    if(t.x>=t.y) {
        if(t.y>=t.z) return tetra(sampleLutB(low),sampleLutB(uint3(high.x,low.y,low.z)),sampleLutB(uint3(high.x,high.y,low.z)),sampleLutB(high),t.x,t.y,t.z);
        if(t.x>=t.z) return tetra(sampleLutB(low),sampleLutB(uint3(high.x,low.y,low.z)),sampleLutB(uint3(high.x,low.y,high.z)),sampleLutB(high),t.x,t.z,t.y);
        return tetra(sampleLutB(low),sampleLutB(uint3(low.x,low.y,high.z)),sampleLutB(uint3(high.x,low.y,high.z)),sampleLutB(high),t.z,t.x,t.y);
    }
    if(t.x>=t.z) return tetra(sampleLutB(low),sampleLutB(uint3(low.x,high.y,low.z)),sampleLutB(uint3(high.x,high.y,low.z)),sampleLutB(high),t.y,t.x,t.z);
    if(t.y>=t.z) return tetra(sampleLutB(low),sampleLutB(uint3(low.x,high.y,low.z)),sampleLutB(uint3(low.x,high.y,high.z)),sampleLutB(high),t.y,t.z,t.x);
    return tetra(sampleLutB(low),sampleLutB(uint3(low.x,low.y,high.z)),sampleLutB(uint3(low.x,high.y,high.z)),sampleLutB(high),t.z,t.y,t.x);
}
float3 lookup(float3 x) {
    float3 p=saturate((x-domainLow.xyz)/(domainHigh.xyz-domainLow.xyz))*(control.w-1);
    uint3 low=(uint3)floor(p),high=min(low+1,control.w-1); float3 t=p-low;
    if(t.x>=t.y) {
        if(t.y>=t.z) return tetra(sampleLut(low),sampleLut(uint3(high.x,low.y,low.z)),sampleLut(uint3(high.x,high.y,low.z)),sampleLut(high),t.x,t.y,t.z);
        if(t.x>=t.z) return tetra(sampleLut(low),sampleLut(uint3(high.x,low.y,low.z)),sampleLut(uint3(high.x,low.y,high.z)),sampleLut(high),t.x,t.z,t.y);
        return tetra(sampleLut(low),sampleLut(uint3(low.x,low.y,high.z)),sampleLut(uint3(high.x,low.y,high.z)),sampleLut(high),t.z,t.x,t.y);
    }
    if(t.x>=t.z) return tetra(sampleLut(low),sampleLut(uint3(low.x,high.y,low.z)),sampleLut(uint3(high.x,high.y,low.z)),sampleLut(high),t.y,t.x,t.z);
    if(t.y>=t.z) return tetra(sampleLut(low),sampleLut(uint3(low.x,high.y,low.z)),sampleLut(uint3(low.x,high.y,high.z)),sampleLut(high),t.y,t.z,t.x);
    return tetra(sampleLut(low),sampleLut(uint3(low.x,low.y,high.z)),sampleLut(uint3(low.x,high.y,high.z)),sampleLut(high),t.z,t.y,t.x);
}
float curve(float x) {
    float s=sign(detail.x)*pow(abs(detail.x),1.2);
    float3 p=saturate(float3(.25-s*.2,.5+s*.05,.75+s*.2));x=saturate(x);
    if(x<=.25)return x*4*p.x;
    if(x<=.5)return lerp(p.x,p.y,(x-.25)*4);
    if(x<=.75)return lerp(p.y,p.z,(x-.5)*4);
    return lerp(p.z,1,(x-.75)*4);
}
float3 evaluate(float3 inputColor) {
    float3 scene=float3(dot(toSrgb[0].xyz,inputColor),dot(toSrgb[1].xyz,inputColor),dot(toSrgb[2].xyz,inputColor))*exposureStrength.xyz;
    float3 pixel=float3(neutral(scene.r),neutral(scene.g),neutral(scene.b));
    if(control.z!=0) {
        float3 src;
        if(control.z==2) { src=pixel; }
        else {
            float3 fg=float3(dot(toFilm[0].xyz,scene),dot(toFilm[1].xyz,scene),dot(toFilm[2].xyz,scene));
            src=float3(flog2(fg.r),flog2(fg.g),flog2(fg.b));
        }
        pixel=lerp(pixel,lookup(src),exposureStrength.w);
    }
    if(lutBInfo.x!=0) pixel=lerp(pixel,lookupB(pixel),detail.w);
    if(tone.z!=0 || tone.w!=0) {
        float light=dot(pixel,float3(.2126,.7152,.0722)),adjusted=light;
        if(tone.w!=0)adjusted=pow(saturate(adjusted),1-tone.w*.5);
        if(tone.z!=0)adjusted=1-pow(1-saturate(adjusted),1-tone.z*.5);
        pixel=light>0 ? pixel*(adjusted/light) : adjusted.xxx;
    }
    if(detail.x!=0)pixel=float3(curve(pixel.r),curve(pixel.g),curve(pixel.b));
    if(tone.x!=1 || tone.y!=1) {
        pixel=(pixel-.5)*tone.x+.5;float light=dot(pixel,float3(.2126,.7152,.0722));
        pixel=light+(pixel-light)*tone.y;
    }
    return pixel;
}
float3 readPixel(uint2 p) { return source[p.y*dimensions.x+p.x]; }
float3 resize(uint2 xy) {
    float2 scale=float2(dimensions.z==1 ? 0 : float(dimensions.x-1)/float(dimensions.z-1),
        dimensions.w==1 ? 0 : float(dimensions.y-1)/float(dimensions.w-1));
    float2 p=min(xy*scale,float2(dimensions.xy-1));uint2 low=(uint2)floor(p),high=min(low+1,dimensions.xy-1);float2 t=p-low;
    return lerp(lerp(readPixel(low),readPixel(uint2(high.x,low.y)),t.x),lerp(readPixel(uint2(low.x,high.y)),readPixel(high),t.x),t.y);
}
[numthreads(16,16,1)]
void main(uint3 id : SV_DispatchThreadID) {
    if(id.x>=dimensions.z || id.y>=dimensions.w)return;
    float3 pixel;
    if(control.x==0)pixel=evaluate(all(dimensions.xy==dimensions.zw) ? readPixel(id.xy) : resize(id.xy));
    else if(control.x==1)pixel=resize(id.xy);
    else {
        float3 sum=0; float weight=0;
        [unroll] for(int y=-1;y<=1;y++) [unroll] for(int x=-1;x<=1;x++) {
            uint2 p=(uint2)clamp(int2(id.xy)+int2(x,y),int2(0,0),int2(dimensions.xy)-1);
            sum+=readPixel(p);weight+=1;
        }
        pixel=readPixel(id.xy);float3 blurred=sum/weight;
        pixel=control.x==2 ? lerp(pixel,blurred,saturate(detail.y)*.4) : saturate(pixel+clamp(detail.z,0,2)*(pixel-blurred));
    }
    destination[id.y*dimensions.z+id.x]=pixel;
}
)hlsl";
}
