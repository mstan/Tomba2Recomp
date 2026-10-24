#pragma once

#include "smash_assets.h"
#include <algorithm>
#include <cmath>

namespace tomba2::mario {
struct Matrix {float m[3][4]{};};
inline Matrix local_matrix(const Transform& t) {
    const float a=std::cos(t.rotation.x),b=std::sin(t.rotation.x);
    const float c=std::cos(t.rotation.y),d=std::sin(t.rotation.y);
    const float e=std::cos(t.rotation.z),f=std::sin(t.rotation.z);
    return {{{c*e*t.scale.x,(b*d*e-a*f)*t.scale.y,(a*d*e+b*f)*t.scale.z,t.translation.x},
             {c*f*t.scale.x,(b*d*f+a*e)*t.scale.y,(a*d*f-b*e)*t.scale.z,t.translation.y},
             {-d*t.scale.x,b*c*t.scale.y,a*c*t.scale.z,t.translation.z}}};
}
inline Matrix multiply(const Matrix& a,const Matrix& b) {
    Matrix c;
    for(unsigned r=0;r<3;++r) {
        for(unsigned j=0;j<3;++j)for(unsigned k=0;k<3;++k)c.m[r][j]+=a.m[r][k]*b.m[k][j];
        c.m[r][3]=a.m[r][3];
        for(unsigned k=0;k<3;++k)c.m[r][3]+=a.m[r][k]*b.m[k][3];
    }
    return c;
}
inline Vec3 transform(const Matrix& m,const Vec3& v) {
    return {m.m[0][0]*v.x+m.m[0][1]*v.y+m.m[0][2]*v.z+m.m[0][3],
            m.m[1][0]*v.x+m.m[1][1]*v.y+m.m[1][2]*v.z+m.m[1][3],
            m.m[2][0]*v.x+m.m[2][1]*v.y+m.m[2][2]*v.z+m.m[2][3]};
}
using PoseBones=std::array<Matrix,25>;
inline PoseBones pose_bones(const Model& model,const std::array<Transform,25>& pose) {
    PoseBones bones;
    for(size_t i=0;i<bones.size();++i) {
        Matrix local=local_matrix(pose[i]);int parent=model.joints[i].parent;
        bones[i]=parent<0?local:multiply(bones[size_t(parent)],local);
    }
    return bones;
}
inline Matrix mario_world(uint16_t heading,Vec3 position) {
    // The imported model faces +Z (arms are mirrored across source X).
    // Native headings face (cos h, sin h) in the world's X/Z plane.
    float h=float(heading&4095)*6.28318530718f/4096;
    float c=std::sin(h),s=std::cos(h);
    return {{{c*0.72f,0,s*0.72f,position.x},
             {0,-0.72f,0,position.y+128},
             {-s*0.72f,0,c*0.72f,position.z}}};
}
inline void raise_capture_arms(PoseBones& bones) {
    // Rigidly raise the imported arm chains around their shoulders. Preserve
    // all source segment lengths and elbow/hand articulation. These are
    // presentation matrices; the native skeleton still drives gameplay.
    for(auto chain: {std::array<unsigned,3>{3,6,6}, {9,12,13}}) {
        unsigned root=chain[0],hand=chain[1],last=chain[2];
        Vec3 p=transform(bones[root],{}),q=transform(bones[hand],{});
        Vec3 v{q.x-p.x,q.y-p.y,q.z-p.z};
        float length=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
        if(length<0.001f)continue;
        v={v.x/length,v.y/length,v.z/length};
        float x=-v.z,z=v.x,s2=x*x+z*z;
        Matrix r{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
        if(s2<0.000001f) {
            if(v.y<0) {r.m[0][0]=-1;r.m[1][1]=-1;}
        } else {
            // Rodrigues: I + [v cross up] + [v cross up]^2/(1+v.y).
            float k=(1-v.y)/s2;
            r.m[0][0]=1-z*z*k;r.m[0][1]=-z;r.m[0][2]=x*z*k;
            r.m[1][0]=z;r.m[1][1]=v.y;r.m[1][2]=-x;
            r.m[2][0]=x*z*k;r.m[2][1]=x;r.m[2][2]=1-x*x*k;
        }
        Vec3 rotated=transform(r,p);
        r.m[0][3]=p.x-rotated.x;r.m[1][3]=p.y-rotated.y;r.m[2][3]=p.z-rotated.z;
        for(unsigned i=root;i<=last;++i)bones[i]=multiply(r,bones[i]);
    }
}
inline Vec3 capture_hands(const PoseBones& bones,const Matrix& world) {
    Vec3 a=transform(bones[6],{}),b=transform(bones[12],{});
    return transform(world,{(a.x+b.x)*0.5f,(a.y+b.y)*0.5f,(a.z+b.z)*0.5f});
}
} // namespace tomba2::mario
