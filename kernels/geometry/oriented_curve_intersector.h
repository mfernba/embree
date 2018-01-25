// ======================================================================== //
// Copyright 2009-2018 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "../common/ray.h"
#include "bezier_curve_intersector.h"
#include "../../common/math/interval.h"

#define DBG(x)

namespace embree
{
  namespace isa
  {
    struct CurveCounters
    {
      __forceinline CurveCounters()
        : numRecursions(0), numKrawczyk(0), numSolve(0), numSolveIterations(0) {}
      
      size_t numRecursions;
      size_t numKrawczyk;
      size_t numSolve;
      size_t numSolveIterations;
    };
    
    template<typename V>
      struct LinearBezierCurve
      {
        V p0,p1;
        
        __forceinline LinearBezierCurve () {}
        
        __forceinline LinearBezierCurve (V p0, V p1)
          : p0(p0), p1(p1) {}

        __forceinline V begin() const { return p0; }
        __forceinline V end  () const { return p1; }
        
        bool hasRoot() const;
        vboolx hasRootX() const;
        
        friend std::ostream& operator<<(std::ostream& cout, const LinearBezierCurve& a) {
          return cout << "LinearBezierCurve (" << a.p0 << ", " << a.p1 << ")";
        }
      };
    
    __forceinline int numRoots(const Interval1f& p0, const Interval1f& p1)
    {
      float eps = 1E-4f;
      bool neg0 = p0.lower < eps; bool pos0 = p0.upper > -eps;
      bool neg1 = p1.lower < eps; bool pos1 = p1.upper > -eps;
      return (neg0 && pos1) || (pos0 && neg1) || (neg0 && pos0) || (neg1 && pos1);
    }

    __forceinline vintx numRoots(const Interval<vfloatx>& p0, const Interval<vfloatx>& p1)
    {
      vfloatx eps = 1E-4f;
      vboolx neg0 = p0.lower < eps; vboolx pos0 = p0.upper > -eps;
      vboolx neg1 = p1.lower < eps; vboolx pos1 = p1.upper > -eps;
      return select((neg0 & pos1) | (pos0 & neg1) | (neg0 & pos0) | (neg1 & pos1),vintx(1),vintx(0));
    }
    
    template<> __forceinline bool LinearBezierCurve<Interval1f>::hasRoot() const {
      return numRoots(p0,p1);
    }

    template<> __forceinline vboolx LinearBezierCurve<Interval<vfloatx>>::hasRootX() const {
      return numRoots(p0,p1) != vintx(zero);
    }
    
    template<typename V>
      struct QuadraticBezierCurve
      {
        V p0,p1,p2;
        
        __forceinline QuadraticBezierCurve () {}
        
        __forceinline QuadraticBezierCurve (V p0, V p1, V p2)
          : p0(p0), p1(p1), p2(p2) {}

        __forceinline V begin() const { return p0; }
        __forceinline V end  () const { return p2; }
             
        __forceinline V interval() const {
          return merge(p0,p1,p2);
        }

        __forceinline BBox<V> bounds() const {
          return merge(BBox<V>(p0),BBox<V>(p1),BBox<V>(p2));
        }

        friend std::ostream& operator<<(std::ostream& cout, const QuadraticBezierCurve& a) {
          return cout << "QuadraticBezierCurve ( (" << a.u.lower << ", " << a.u.upper << "), " << a.p0 << ", " << a.p1 << ", " << a.p2 << ")";
        }
      };


    typedef QuadraticBezierCurve<float> QuadraticBezierCurve1f;
    typedef QuadraticBezierCurve<Vec2fa> QuadraticBezierCurve2fa;
    typedef QuadraticBezierCurve<Vec3fa> QuadraticBezierCurve3fa;
  
      template<typename V>
      struct CubicBezierCurve
      {
        V p0,p1,p2,p3;
        
        __forceinline CubicBezierCurve () {}

        template<typename T1>
        __forceinline CubicBezierCurve (const CubicBezierCurve<T1>& other)
          : p0(other.p0), p1(other.p1), p2(other.p2), p3(other.p3) {}
        
        __forceinline CubicBezierCurve (V p0, V p1, V p2, V p3)
          : p0(p0), p1(p1), p2(p2), p3(p3) {}

        __forceinline V begin() const { return p0; }
        __forceinline V end  () const { return p3; }
             
        __forceinline CubicBezierCurve<float> xfm(const V& dx) const {
          return CubicBezierCurve<float>(dot(p0,dx),dot(p1,dx),dot(p2,dx),dot(p3,dx));
        }

        __forceinline CubicBezierCurve<vfloatx> xfmX(const V& dx) const {
          return CubicBezierCurve<vfloatx>(dot(p0,dx),dot(p1,dx),dot(p2,dx),dot(p3,dx));
        }

        __forceinline CubicBezierCurve<float> xfm(const V& dx, const V& p) const {
          return CubicBezierCurve<float>(dot(p0-p,dx),dot(p1-p,dx),dot(p2-p,dx),dot(p3-p,dx));
        }
        
        __forceinline CubicBezierCurve<Vec2fa> xfm(const V& dx, const V& dy, const V& p) const
        {
          const Vec2fa q0(dot(p0-p,dx), dot(p0-p,dy));
          const Vec2fa q1(dot(p1-p,dx), dot(p1-p,dy));
          const Vec2fa q2(dot(p2-p,dx), dot(p2-p,dy));
          const Vec2fa q3(dot(p3-p,dx), dot(p3-p,dy));
          return CubicBezierCurve<Vec2fa>(q0,q1,q2,q3);
        }
        
        __forceinline int maxRoots() const;

        __forceinline BBox<V> bounds() const {
          return merge(BBox<V>(p0),BBox<V>(p1),BBox<V>(p2),BBox<V>(p3));
        }

        __forceinline BBox1f bounds(const V& axis) const {
          return merge(BBox1f(dot(p0,axis)),BBox1f(dot(p1,axis)),BBox1f(dot(p2,axis)),BBox1f(dot(p3,axis)));
        }

        __forceinline BBox<vfloatx> vbounds(const V& axis) const {
          return merge(BBox<vfloatx>(dot(p0,axis)),BBox<vfloatx>(dot(p1,axis)),BBox<vfloatx>(dot(p2,axis)),BBox<vfloatx>(dot(p3,axis)));
        }

        __forceinline friend CubicBezierCurve operator +( const CubicBezierCurve& a, const CubicBezierCurve& b ) {
          return CubicBezierCurve(a.p0+b.p0,a.p1+b.p1,a.p2+b.p2,a.p3+b.p3);
        }
        
        __forceinline friend CubicBezierCurve operator -( const CubicBezierCurve& a, const CubicBezierCurve& b ) {
          return CubicBezierCurve(a.p0-b.p0,a.p1-b.p1,a.p2-b.p2,a.p3-b.p3);
        }

        __forceinline friend CubicBezierCurve operator *( const float a, const CubicBezierCurve& b ) {
          return CubicBezierCurve(a*b.p0,a*b.p1,a*b.p2,a*b.p3);
        }

        __forceinline friend CubicBezierCurve operator *( const vfloatx a, const CubicBezierCurve& b ) {
          return CubicBezierCurve(a*b.p0,a*b.p1,a*b.p2,a*b.p3);
        }

        __forceinline friend CubicBezierCurve lerp ( const CubicBezierCurve& a, const CubicBezierCurve& b, float t ) {
          return (1.0f-t)*a + t*b;
        }

        __forceinline friend CubicBezierCurve lerp ( const CubicBezierCurve& a, const CubicBezierCurve& b, vfloatx t ) {
          return (vfloatx(1.0f)-t)*a + t*b;
        }

        __forceinline friend CubicBezierCurve merge ( const CubicBezierCurve& a, const CubicBezierCurve& b ) {
          return CubicBezierCurve(merge(a.p0,b.p0),merge(a.p1,b.p1),merge(a.p2,b.p2),merge(a.p3,b.p3));
        }

        __forceinline void split(CubicBezierCurve& left, CubicBezierCurve& right, const float t = 0.5f) const
        {
          const V p00 = p0;
          const V p01 = p1;
          const V p02 = p2;
          const V p03 = p3;
          
          const V p10 = lerp(p00,p01,t);
          const V p11 = lerp(p01,p02,t);
          const V p12 = lerp(p02,p03,t);
          const V p20 = lerp(p10,p11,t);
          const V p21 = lerp(p11,p12,t);
          const V p30 = lerp(p20,p21,t);
          
          new (&left ) CubicBezierCurve(p00,p10,p20,p30);
          new (&right) CubicBezierCurve(p30,p21,p12,p03);
        }

        __forceinline CubicBezierCurve<Vec2vfx> split() const
        {
          const float u0 = 0.0f, u1 = 1.0f;
          const float dscale = (u1-u0)*(1.0f/(3.0f*(VSIZEX-1)));
          const vfloatx vu0 = lerp(u0,u1,vfloatx(step)*(1.0f/(VSIZEX-1)));
          Vec2vfx P0, dP0du; evalN(vu0,P0,dP0du); dP0du = dP0du * Vec2vfx(dscale);
          const Vec2vfx P3 = shift_right_1(P0);
          const Vec2vfx dP3du = shift_right_1(dP0du); 
          const Vec2vfx P1 = P0 + dP0du; 
          const Vec2vfx P2 = P3 - dP3du;
          return CubicBezierCurve<Vec2vfx>(P0,P1,P2,P3);
        }

        __forceinline CubicBezierCurve<Vec2vfx> split(const BBox1f& u) const
        {
          const float u0 = u.lower, u1 = u.upper;
          const float dscale = (u1-u0)*(1.0f/(3.0f*(VSIZEX-1)));
          const vfloatx vu0 = lerp(u0,u1,vfloatx(step)*(1.0f/(VSIZEX-1)));
          Vec2vfx P0, dP0du; evalN(vu0,P0,dP0du); dP0du = dP0du * Vec2vfx(dscale);
          const Vec2vfx P3 = shift_right_1(P0);
          const Vec2vfx dP3du = shift_right_1(dP0du); 
          const Vec2vfx P1 = P0 + dP0du; 
          const Vec2vfx P2 = P3 - dP3du;
          return CubicBezierCurve<Vec2vfx>(P0,P1,P2,P3);
        }
        
        __forceinline void eval(float t, V& p, V& dp) const
        {
          const V p00 = p0;
          const V p01 = p1;
          const V p02 = p2;
          const V p03 = p3;
          
          const V p10 = lerp(p00,p01,t);
          const V p11 = lerp(p01,p02,t);
          const V p12 = lerp(p02,p03,t);
          const V p20 = lerp(p10,p11,t);
          const V p21 = lerp(p11,p12,t);
          const V p30 = lerp(p20,p21,t);
          
          p = p30;
          dp = V(3.0f)*(p21-p20);
        }

        __forceinline V eval(float t) const
        {
          const V p00 = p0;
          const V p01 = p1;
          const V p02 = p2;
          const V p03 = p3;
          
          const V p10 = lerp(p00,p01,t);
          const V p11 = lerp(p01,p02,t);
          const V p12 = lerp(p02,p03,t);
          const V p20 = lerp(p10,p11,t);
          const V p21 = lerp(p11,p12,t);
          const V p30 = lerp(p20,p21,t);

          return p30;
        }

        __forceinline V eval_dt(float t) const
        {
          const V p00 = p0;
          const V p01 = p1;
          const V p02 = p2;
          const V p03 = p3;
          
          const V p10 = lerp(p00,p01,t);
          const V p11 = lerp(p01,p02,t);
          const V p12 = lerp(p02,p03,t);
          const V p20 = lerp(p10,p11,t);
          const V p21 = lerp(p11,p12,t);
          //const V p30 = lerp(p20,p21,t);
          
          return V(3.0f)*(p21-p20);
        }
        
        __forceinline void evalN(const vfloatx& t, Vec2vfx& p, Vec2vfx& dp) const
        {
          const Vec2vfx p00 = p0;
          const Vec2vfx p01 = p1;
          const Vec2vfx p02 = p2;
          const Vec2vfx p03 = p3;
          
          const Vec2vfx p10 = lerp(p00,p01,t);
          const Vec2vfx p11 = lerp(p01,p02,t);
          const Vec2vfx p12 = lerp(p02,p03,t);
          const Vec2vfx p20 = lerp(p10,p11,t);
          const Vec2vfx p21 = lerp(p11,p12,t);
          const Vec2vfx p30 = lerp(p20,p21,t);
          
          p = p30;
          dp = vfloatx(3.0f)*(p21-p20);
        }
        
        __forceinline CubicBezierCurve clip(const Interval1f& u1) const
        {
          V f0,df0; eval(u1.lower,f0,df0);
          V f1,df1; eval(u1.upper,f1,df1);
          float s = u1.upper-u1.lower;
          return CubicBezierCurve(f0,f0+s*(1.0f/3.0f)*df0,f1-s*(1.0f/3.0f)*df1,f1);
        }

        __forceinline QuadraticBezierCurve<V> derivative() const
        {
          const V q0 = 3.0f*(p1-p0);
          const V q1 = 3.0f*(p2-p1);
          const V q2 = 3.0f*(p3-p2);
          return QuadraticBezierCurve<V>(q0,q1,q2);
        }

        __forceinline BBox<V> derivative_bounds(const Interval1f& u1) const
        {
          V f0,df0; eval(u1.lower,f0,df0);
          V f3,df3; eval(u1.upper,f3,df3);
          const float s = u1.upper-u1.lower;
          const V f1 = f0+s*(1.0f/3.0f)*df0;
          const V f2 = f3-s*(1.0f/3.0f)*df3;
          const V q0 = s*df0;
          const V q1 = 3.0f*(f2-f1);
          const V q2 = s*df3;
          return merge(BBox<V>(q0),BBox<V>(q1),BBox<V>(q2));
        }
        
        friend std::ostream& operator<<(std::ostream& cout, const CubicBezierCurve& a) {
          return cout << "CubicBezierCurve (" << a.p0 << ", " << a.p1 << ", " << a.p2 << ", " << a.p3 << ")";
        }
      };
    
    typedef CubicBezierCurve<float> CubicBezierCurve1f;
    typedef CubicBezierCurve<Vec2fa> CubicBezierCurve2fa;
    typedef CubicBezierCurve<Vec3fa> CubicBezierCurve3fa;
    
    template<> inline int CubicBezierCurve<float>::maxRoots() const
    {
      float eps = 1E-4f;
      bool neg0 = p0 <= 0.0f; bool zero0 = fabs(p0) < eps;
      bool neg1 = p1 <= 0.0f; bool zero1 = fabs(p1) < eps;
      bool neg2 = p2 <= 0.0f; bool zero2 = fabs(p2) < eps;
      bool neg3 = p3 <= 0.0f; bool zero3 = fabs(p3) < eps;
      return (neg0 != neg1 || zero0) + (neg1 != neg2 || zero1) + (neg2 != neg3 || zero2 || zero3);
    }
    
    template<> inline int CubicBezierCurve<Interval1f>::maxRoots() const {
      return numRoots(p0,p1) + numRoots(p1,p2) + numRoots(p2,p3);
    }

    template<typename V>
      struct TensorLinearQuadraticBezierSurface
      {
        QuadraticBezierCurve<V> L;
        QuadraticBezierCurve<V> R;
        
        __forceinline TensorLinearQuadraticBezierSurface() {}
        
        __forceinline TensorLinearQuadraticBezierSurface(const TensorLinearQuadraticBezierSurface<V>& curve)
          : L(curve.L), R(curve.R) {}
        
        __forceinline TensorLinearQuadraticBezierSurface(const QuadraticBezierCurve<V>& L, const QuadraticBezierCurve<V>& R)
          : L(L), R(R) {}
        
        __forceinline BBox<V> bounds() const {
          return merge(L.bounds(),R.bounds());
        }
      };
    
    template<typename V>
      struct TensorLinearCubicBezierSurface
      {
        CubicBezierCurve<V> L;
        CubicBezierCurve<V> R;
        
        __forceinline TensorLinearCubicBezierSurface() {}
        
        __forceinline TensorLinearCubicBezierSurface(const TensorLinearCubicBezierSurface<V>& curve)
          : L(curve.L), R(curve.R) {}
        
        __forceinline TensorLinearCubicBezierSurface(const CubicBezierCurve<V>& L, const CubicBezierCurve<V>& R)
          : L(L), R(R) {}
        
        __forceinline BBox<V> bounds() const {
          return merge(L.bounds(),R.bounds());
        }

        __forceinline BBox1f bounds(const V& axis) const {
          return merge(L.bounds(axis),R.bounds(axis));
        }

        __forceinline BBox<vfloatx> vbounds(const V& axis) const {
          return merge(L.vbounds(axis),R.vbounds(axis));
        }
        
        __forceinline CubicBezierCurve<Interval1f> reduce_v() const
        {
          const CubicBezierCurve<Interval<V>> Li(L);
          const CubicBezierCurve<Interval<V>> Ri(R);
          return merge(Li,Ri);
        }
        
        __forceinline LinearBezierCurve<Interval1f> reduce_u() const {
          return LinearBezierCurve<Interval1f>(L.bounds(),R.bounds());
        }

        __forceinline LinearBezierCurve<Interval<vfloatx>> vreduce_u() const {
          return LinearBezierCurve<Interval<vfloatx>>(L.bounds(),R.bounds());
        }
        
        __forceinline TensorLinearCubicBezierSurface<float> xfm(const V& dx) const {
          return TensorLinearCubicBezierSurface<float>(L.xfm(dx),R.xfm(dx));
        }

        __forceinline TensorLinearCubicBezierSurface<vfloatx> xfmX(const V& dx) const {
          return TensorLinearCubicBezierSurface<vfloatx>(L.xfmX(dx),R.xfmX(dx));
        }

        __forceinline TensorLinearCubicBezierSurface<float> xfm(const V& dx, const V& p) const {
          return TensorLinearCubicBezierSurface<float>(L.xfm(dx,p),R.xfm(dx,p));
        }
        
        __forceinline TensorLinearCubicBezierSurface<Vec2fa> xfm(const V& dx, const V& dy, const V& p) const {
          return TensorLinearCubicBezierSurface<Vec2fa>(L.xfm(dx,dy,p),R.xfm(dx,dy,p));
        }
        
        __forceinline TensorLinearCubicBezierSurface clip_u(const Interval1f& u) const {
          return TensorLinearCubicBezierSurface(L.clip(u),R.clip(u));
        }
        
        __forceinline TensorLinearCubicBezierSurface clip_v(const Interval1f& v) const {
          return TensorLinearCubicBezierSurface(lerp(L,R,v.lower),lerp(L,R,v.upper));
        }

        __forceinline TensorLinearCubicBezierSurface clip(const Interval1f& u, const Interval1f& v) const {
          return clip_v(v).clip_u(u);
        }

        __forceinline TensorLinearCubicBezierSurface vclip_v(const Interval<vfloatx>& v) const {
          return TensorLinearCubicBezierSurface(lerp(L,R,v.lower),lerp(L,R,v.upper));
        }

        __forceinline void split_u(TensorLinearCubicBezierSurface& left, TensorLinearCubicBezierSurface& right, const float u = 0.5f) const
        {
          CubicBezierCurve<V> L0,L1; L.split(L0,L1,u);
          CubicBezierCurve<V> R0,R1; R.split(R0,R1,u);
          new (&left ) TensorLinearCubicBezierSurface(L0,R0);
          new (&right) TensorLinearCubicBezierSurface(L1,R1);
        }

        __forceinline TensorLinearCubicBezierSurface<Vec2vfx> split_u() const {
          return TensorLinearCubicBezierSurface<Vec2vfx>(L.split(),R.split());
        }

        __forceinline TensorLinearCubicBezierSurface<Vec2vfx> split_u(const BBox1f& u) const {
          return TensorLinearCubicBezierSurface<Vec2vfx>(L.split(u),R.split(u));
        }

        __forceinline V eval(const float u, const float v) const {
          return lerp(L,R,v).eval(u);
        }

        __forceinline V eval_du(const float u, const float v) const {
          return lerp(L,R,v).eval_dt(u);
        }

        __forceinline V eval_dv(const float u, const float v) const {
          return (R-L).eval(u);
        }

        __forceinline void eval(const float u, const float v, V& p, V& dpdu, V& dpdv) const
        {
          V p0, dp0du; L.eval(u,p0,dp0du);
          V p1, dp1du; R.eval(u,p1,dp1du);
          p = lerp(p0,p1,v);
          dpdu = lerp(dp0du,dp1du,v);
          dpdv = p1-p0;
        }

        __forceinline TensorLinearQuadraticBezierSurface<V> derivative_u() const {
          return TensorLinearQuadraticBezierSurface<V>(L.derivative(),R.derivative());
        }

        __forceinline CubicBezierCurve<V> derivative_v() const {
          return R-L;
        }

        __forceinline V axis_u() const {
          return (L.end()-L.begin())+(R.end()-R.begin());
        }

        __forceinline V axis_v() const {
          return (R.begin()-L.begin())+(R.end()-L.end());
        }
        
        friend std::ostream& operator<<(std::ostream& cout, const TensorLinearCubicBezierSurface& a)
        {
          return cout << "TensorLinearCubicBezierSurface" << std::endl
                      << "{" << std::endl
                      << "  L = " << a.L << ", " << std::endl
                      << "  R = " << a.R << std::endl
                      << "}";
        }
      };
    
    typedef TensorLinearCubicBezierSurface<float> TensorLinearCubicBezierSurface1f;
    typedef TensorLinearCubicBezierSurface<Vec2fa> TensorLinearCubicBezierSurface2fa;
    typedef TensorLinearCubicBezierSurface<Vec3fa> TensorLinearCubicBezierSurface3fa;

    template<typename Epilog>
      struct TensorLinearCubicBezierSurfaceIntersector
      {
        Ray& ray;
        LinearSpace3fa space; // FIXME:  calculate improved u,v directions aligned with curve
        TensorLinearCubicBezierSurface3fa curve3d;
        TensorLinearCubicBezierSurface2fa curve2d;
        float eps;
        const Epilog& epilog;
        bool isHit;
        CurveCounters counters;

        __forceinline TensorLinearCubicBezierSurfaceIntersector (Ray& ray, const TensorLinearCubicBezierSurface3fa& curve3d, const Epilog& epilog)
          : ray(ray), space(frame(ray.dir)), curve3d(curve3d), epilog(epilog), isHit(false) {}
        
        __forceinline Interval1f solve_linear(const float u0, const float u1, const float& p0, const float& p1)
        {
          if (p1 == p0) {
            if (p0 == 0.0f) return Interval1f(u0,u1);
            else return Interval1f(empty);
          }
          const float t = -p0/(p1-p0);
          const float tt = lerp(u0,u1,t);
          return Interval1f(tt);
        }

        __forceinline Interval<vfloatx> solve_linear(const vboolx& valid, const vfloatx u0, const vfloatx u1, const vfloatx& p0, const vfloatx& p1)
        {
          const vfloatx t = -p0/(p1-p0);
          const vfloatx tt = lerp(u0,u1,t);
          Interval<vfloatx> r(tt);
          const vboolx div0 = valid & (p1 == p0);
          if (none(div0)) return r;
          
          const vboolx range = valid & (p0 == vfloatx(0.0f));
          Interval<vfloatx> r1 = select(range,Interval<vfloatx>(u0,u1),Interval<vfloatx>(empty));
          return select(div0,r1,r);
        }
        
        __forceinline void solve_linear(const float u0, const float u1, const Interval1f& p0, const Interval1f& p1, Interval1f& u)
        {
          if (sign(p0.lower) != sign(p0.upper)) u.extend(u0);
          if (sign(p0.lower) != sign(p1.lower)) u.extend(solve_linear(u0,u1,p0.lower,p1.lower));
          if (sign(p0.upper) != sign(p1.upper)) u.extend(solve_linear(u0,u1,p0.upper,p1.upper));
          if (sign(p1.lower) != sign(p1.upper)) u.extend(u1);
        }

        __forceinline void solve_linear(const vboolx& valid, const vfloatx u0, const vfloatx u1, const Interval<vfloatx>& p0, const Interval<vfloatx>& p1, Interval<vfloatx>& u)
        {
          const vboolx valid0 = valid & (sign(p0.lower) != sign(p0.upper));
          const vboolx valid1 = valid & (sign(p0.lower) != sign(p1.lower));
          const vboolx valid2 = valid & (sign(p0.upper) != sign(p1.upper));
          const vboolx valid3 = valid & (sign(p1.lower) != sign(p1.upper));
          if (any(valid0)) u = select(valid0,merge(u,u0),u);
          if (any(valid1)) u = select(valid1,merge(u,solve_linear(valid1,u0,u1,p0.lower,p1.lower)),u);
          if (any(valid2)) u = select(valid2,merge(u,solve_linear(valid2,u0,u1,p0.upper,p1.upper)),u);
          if (any(valid3)) u = select(valid3,merge(u,u1),u);
        }
        
        __forceinline Interval1f bezier_clipping(const CubicBezierCurve<Interval1f>& curve)
        {
          Interval1f u = empty;
          solve_linear(0.0f/3.0f,1.0f/3.0f,curve.p0,curve.p1,u);
          solve_linear(0.0f/3.0f,2.0f/3.0f,curve.p0,curve.p2,u);
          solve_linear(0.0f/3.0f,3.0f/3.0f,curve.p0,curve.p3,u);
          solve_linear(1.0f/3.0f,2.0f/3.0f,curve.p1,curve.p2,u);
          solve_linear(1.0f/3.0f,3.0f/3.0f,curve.p1,curve.p3,u);
          solve_linear(2.0f/3.0f,3.0f/3.0f,curve.p2,curve.p3,u);
          return intersect(u,Interval1f(0.0f,1.0f));
        }
        
        __forceinline Interval1f bezier_clipping(const LinearBezierCurve<Interval1f>& curve)
        {
          Interval1f v = empty;
          solve_linear(0.0f,1.0f,curve.p0,curve.p1,v);
          return intersect(v,Interval1f(0.0f,1.0f));
        }

        __forceinline Interval<vfloatx> bezier_clipping(const vboolx& valid, const LinearBezierCurve<Interval<vfloatx>>& curve)
        {
          Interval<vfloatx> v = empty;
          solve_linear(valid,0.0f,1.0f,curve.p0,curve.p1,v);
          return intersect(v,Interval<vfloatx>(0.0f,1.0f));
        }
        
        __forceinline void solve_u(BBox1f cu, BBox1f cv, const TensorLinearCubicBezierSurface2fa& curve2, const Vec2fa& du)
        {
          const TensorLinearCubicBezierSurface1f curve1 = curve2.xfm(du);
          CubicBezierCurve<Interval1f> curve0 = curve1.reduce_v();         
          int roots = curve0.maxRoots();
          if (roots == 0) return;
          
          if (roots == 1)
          {
            const Interval1f u = bezier_clipping(curve0);
            if (isEmpty(u)) return;
            TensorLinearCubicBezierSurface2fa curve2a = curve2.clip_u(u);
            cu = BBox1f(lerp(cu.lower,cu.upper,u.lower),lerp(cu.lower,cu.upper,u.upper));
            solve(cu,cv,curve2a);
            return;
          }
          
          TensorLinearCubicBezierSurface2fa curve2a, curve2b;
          curve2.split_u(curve2a,curve2b);
          solve(BBox1f(cu.lower,cu.center()),cv,curve2a);
          solve(BBox1f(cu.center(),cu.upper),cv,curve2b);
        }
        
        __forceinline void solve_v(BBox1f cu, BBox1f cv, const TensorLinearCubicBezierSurface2fa& curve2, const Vec2fa& dv)
        {
          const TensorLinearCubicBezierSurface1f curve1 = curve2.xfm(dv);
          LinearBezierCurve<Interval1f> curve0 = curve1.reduce_u();       
          if (!curve0.hasRoot()) return;
          
          const Interval1f v = bezier_clipping(curve0);
          if (isEmpty(v)) return;
          TensorLinearCubicBezierSurface2fa curve2a = curve2.clip_v(v);
          cv = BBox1f(lerp(cv.lower,cv.upper,v.lower),lerp(cv.lower,cv.upper,v.upper));
          solve(cu,cv,curve2a);
        }

        __forceinline void solve_uv(BBox1f cu, BBox1f cv, const TensorLinearCubicBezierSurface2fa& curve2)
        {
          const Vec2fa du = normalize(curve2.axis_u());
          const Vec2fa dv = normalize(curve2.axis_v());
          
          const TensorLinearCubicBezierSurface1f curve1v = curve2.xfm(dv);
          LinearBezierCurve<Interval1f> curve0v = curve1v.reduce_u();
          if (!curve0v.hasRoot()) return;
          
          const Interval1f v = bezier_clipping(curve0v);
          if (isEmpty(v)) return;
          TensorLinearCubicBezierSurface2fa curve2a = curve2.clip_v(v);
          cv = BBox1f(lerp(cv.lower,cv.upper,v.lower),lerp(cv.lower,cv.upper,v.upper));

          const TensorLinearCubicBezierSurface1f curve1u = curve2a.xfm(du);
          CubicBezierCurve<Interval1f> curve0u = curve1u.reduce_v();         
          int roots = curve0u.maxRoots();
          if (roots == 0) return;
          
          if (roots == 1)
          {
            const Interval1f u = bezier_clipping(curve0u);
            if (isEmpty(u)) return;
            TensorLinearCubicBezierSurface2fa curve2b = curve2a.clip_u(u);
            cu = BBox1f(lerp(cu.lower,cu.upper,u.lower),lerp(cu.lower,cu.upper,u.upper));
            solve(cu,cv,curve2b);
            return;
          }

          TensorLinearCubicBezierSurface2fa curve2l, curve2r;
          curve2a.split_u(curve2l,curve2r);
          solve(BBox1f(cu.lower,cu.center()),cv,curve2l);
          solve(BBox1f(cu.center(),cu.upper),cv,curve2r);
        }
        
        void solve(BBox1f cu, BBox1f cv, const TensorLinearCubicBezierSurface2fa& curve2)
        {
          BBox2fa bounds = curve2.bounds();
          if (bounds.upper.x < 0.0f) return;
          if (bounds.upper.y < 0.0f) return;
          if (bounds.lower.x > 0.0f) return;
          if (bounds.lower.y > 0.0f) return;
          
          if (max(cu.size(),cv.size()) < 1E-4f)
          {
            const float u = cu.center();
            const float v = cv.center();
            TensorLinearCubicBezierSurface1f curve_z = curve3d.xfm(space.vz,ray.org);
            const float t = curve_z.eval(u,v)/length(ray.dir);
            if (t >= ray.tnear() && t <= ray.tfar()) {
              const Vec3fa Ng = cross(curve3d.eval_du(u,v),curve3d.eval_dv(u,v));
              BezierCurveHit hit(t,u,v,Ng);
              isHit |= epilog(hit);
            }
            return;
          }          
          solve_uv(cu,cv,curve2);
          //if (length(du) > length(dv)) solve_u(curve2,normalize(du));
          //else                         solve_v(curve2,normalize(dv));
        }
        
        bool solve()
        {
          TensorLinearCubicBezierSurface2fa curve2 = curve3d.xfm(space.vx,space.vy,ray.org);
          solve(BBox1f(0.0f,1.0f),BBox1f(0.0f,1.0f),curve2);
          return isHit;
        }

        void solve_newton_raphson1(BBox1f cu, BBox1f cv, const TensorLinearCubicBezierSurface2fa& curve2)
        {
          counters.numSolve++;
          Vec2fa uv(0.5f,0.5f);
                      
          for (size_t i=0; i<20; i++)
          {
            counters.numSolveIterations++;
            const Vec2fa f    = curve2.eval(uv.x,uv.y);
            const Vec2fa dfdu = curve2.eval_du(uv.x,uv.y);
            const Vec2fa dfdv = curve2.eval_dv(uv.x,uv.y);
            const LinearSpace2fa rcp_J = rcp(LinearSpace2fa(dfdu,dfdv));
            const Vec2fa duv = rcp_J*f;
            uv -= duv;

            if (max(fabs(duv.x),fabs(duv.y)) < 1E-4f)
            {
              DBG(PRINT2("solution",curve2));
              const float u = lerp(cu.lower,cu.upper,uv.x);
              const float v = lerp(cv.lower,cv.upper,uv.y);
              if (!(u >= 0.0f && u <= 1.0f)) return; // rejects NaNs
              if (!(v >= 0.0f && v <= 1.0f)) return; // rejects NaNs
              const TensorLinearCubicBezierSurface1f curve_z = curve3d.xfm(space.vz,ray.org);
              const float t = curve_z.eval(u,v)/length(ray.dir);
              if (!(t > ray.tnear() && t < ray.tfar())) return; // rejects NaNs
              const Vec3fa Ng = cross(curve3d.eval_du(u,v),curve3d.eval_dv(u,v));
              BezierCurveHit hit(t,u,v,Ng);
              isHit |= epilog(hit);
              return;
            }
          }       
        }

        void solve_newton_raphson2(BBox1f cu, BBox1f cv, const TensorLinearCubicBezierSurface2fa& curve2)
        {
          counters.numSolve++;
          
          Vec2fa uv(0.5f,0.5f);
          const Vec2fa dfdu = curve2.eval_du(uv.x,uv.y);
          const Vec2fa dfdv = curve2.eval_dv(uv.x,uv.y);
          const LinearSpace2fa rcp_J = rcp(LinearSpace2fa(dfdu,dfdv));
            
          for (size_t i=0; i<20; i++)
          {
            counters.numSolveIterations++;
            const Vec2fa f    = curve2.eval(uv.x,uv.y);
            const Vec2fa duv = rcp_J*f;
            uv -= duv;

            if (max(fabs(duv.x),fabs(duv.y)) < 1E-4f)
            {
              DBG(PRINT2("solution",curve2));
              DBG(PRINT2(cu,cv));
              DBG(PRINT(uv));
              const float u = lerp(cu.lower,cu.upper,uv.x);
              const float v = lerp(cv.lower,cv.upper,uv.y);
              DBG(PRINT2(u,v));
              if (!(u >= 0.0f && u <= 1.0f)) return; // rejects NaNs
              if (!(v >= 0.0f && v <= 1.0f)) return; // rejects NaNs
              const TensorLinearCubicBezierSurface1f curve_z = curve3d.xfm(space.vz,ray.org);
              const float t = curve_z.eval(u,v)/length(ray.dir);
              if (!(t > ray.tnear() && t < ray.tfar())) return; // rejects NaNs
              const Vec3fa Ng = cross(curve3d.eval_du(u,v),curve3d.eval_dv(u,v));
              BezierCurveHit hit(t,u,v,Ng);
              isHit |= epilog(hit);
              return;
            }
          }       
        }

        void solve_newton_raphson2(BBox1f cu, BBox1f cv)
        {
          counters.numSolve++;
          
          Vec2fa uv(cu.center(),cv.center());
          const Vec2fa dfdu = curve2d.eval_du(uv.x,uv.y);
          const Vec2fa dfdv = curve2d.eval_dv(uv.x,uv.y);
          const LinearSpace2fa rcp_J = rcp(LinearSpace2fa(dfdu,dfdv));
            
          for (size_t i=0; i<20; i++)
          {
            counters.numSolveIterations++;
            const Vec2fa f    = curve2d.eval(uv.x,uv.y);
            const Vec2fa duv = rcp_J*f;
            uv -= duv;

            if (max(fabs(duv.x),fabs(duv.y)) < 1E-4f)
            {
              DBG(PRINT("solution"));
              DBG(PRINT2(cu,cv));
              DBG(PRINT(uv));
              const float u = uv.x;
              const float v = uv.y;
              DBG(PRINT2(u,v));
              if (!(u >= 0.0f && u <= 1.0f)) return; // rejects NaNs
              if (!(v >= 0.0f && v <= 1.0f)) return; // rejects NaNs
              const TensorLinearCubicBezierSurface1f curve_z = curve3d.xfm(space.vz,ray.org);
              const float t = curve_z.eval(u,v)/length(ray.dir);
              if (!(t > ray.tnear() && t < ray.tfar())) return; // rejects NaNs
              const Vec3fa Ng = cross(curve3d.eval_du(u,v),curve3d.eval_dv(u,v));
              BezierCurveHit hit(t,u,v,Ng);
              isHit |= epilog(hit);
              return;
            }
          }       
        }

        void solve_newton_raphson2b(BBox1f cu, BBox1f cv, Vec2fa uv, const Vec2fa& dfdu, const Vec2fa& dfdv, const LinearSpace2fa& rcp_J)
        {
          asm("//solve_newton_raphson2b");
          counters.numSolve++;
          
          for (size_t i=0; i<20; i++)
          {
            counters.numSolveIterations++;
            const Vec2fa f    = curve2d.eval(uv.x,uv.y);
            const Vec2fa duv = rcp_J*f;
            uv -= duv;

            if (max(fabs(duv.x),fabs(duv.y)) < 1E-4f)
            {
              DBG(PRINT("solution"));
              DBG(PRINT2(cu,cv));
              DBG(PRINT(uv));
              const float u = uv.x;
              const float v = uv.y;
              DBG(PRINT2(u,v));
              if (!(u >= 0.0f && u <= 1.0f)) return; // rejects NaNs
              if (!(v >= 0.0f && v <= 1.0f)) return; // rejects NaNs
              const TensorLinearCubicBezierSurface1f curve_z = curve3d.xfm(space.vz,ray.org);
              const float t = curve_z.eval(u,v)/length(ray.dir);
              if (!(t > ray.tnear() && t < ray.tfar())) return; // rejects NaNs
              const Vec3fa Ng = cross(curve3d.eval_du(u,v),curve3d.eval_dv(u,v));
              BezierCurveHit hit(t,u,v,Ng);
              isHit |= epilog(hit);
              return;
            }
          }       
        }

        __forceinline bool clip_v(BBox1f& cu, BBox1f& cv)
        {
          const Vec2fa dv = normalize(curve2d.eval_dv(cu.lower,cv.lower));
          const TensorLinearCubicBezierSurface1f curve1v = curve2d.xfm(dv).clip(cu,cv);
          LinearBezierCurve<Interval1f> curve0v = curve1v.reduce_u();
          if (!curve0v.hasRoot()) return false;
          Interval1f v = bezier_clipping(curve0v);
          if (isEmpty(v)) return false;
          //if (v.size()*cv.size() < 0.001f) v = Interval1f(v.center()) + Interval1f(-0.001f/cv.size(),+0.001f/cv.size());
          v = intersect(v + Interval1f(-0.1f,+0.1f),Interval1f(0.0f,1.0f));
          //curve2 = curve2.clip_v(v);
          cv = BBox1f(lerp(cv.lower,cv.upper,v.lower),lerp(cv.lower,cv.upper,v.upper));
          return true;
        }

        bool solve_krawczyk(BBox1f cu, BBox1f cv, int depth)
        {
          DBG(tab(depth); PRINT("solve_krawczyk"));
          asm("//solve_krawczyk");
          counters.numKrawczyk++;
          if (!clip_v(cu,cv)) return true;
          DBG(tab(depth); PRINT2(cu,cv));

          const float d = cu.size();
          DBG(PRINT(Device::debug_int0));
          cu.lower -= float(Device::debug_int0)*0.1f*d;
          cu.upper += float(Device::debug_int0)*0.1f*d;
          DBG(tab(depth); PRINT2(cu,cv));
          const TensorLinearCubicBezierSurface2fa curve2 = curve2d.clip(cu,cv);
          const BBox2fa bounds_du = (1.0f/cu.size())*curve2.derivative_u().bounds();
          const BBox2fa bounds_dv = (1.0f/cv.size())*curve2.derivative_v().bounds();
          DBG(tab(depth); PRINT(bounds_du));
          DBG(tab(depth); PRINT(bounds_dv));

          LinearSpace2<Vec2<Interval1f>> I(Interval1f(1.0f), Interval1f(0.0f),
                                           Interval1f(0.0f), Interval1f(1.0f));

          LinearSpace2<Vec2<Interval1f>> G(Interval1f(bounds_du.lower.x,bounds_du.upper.x), Interval1f(bounds_dv.lower.x,bounds_dv.upper.x),
                                           Interval1f(bounds_du.lower.y,bounds_du.upper.y), Interval1f(bounds_dv.lower.y,bounds_dv.upper.y));

          Vec2fa c(cu.center(),cv.center());
          Vec2fa f,dfdu,dfdv; curve2d.eval(c.x,c.y,f,dfdu,dfdv);
          const LinearSpace2fa rcp_J = rcp(LinearSpace2fa(dfdu,dfdv));
          const Vec2fa c1 = c - rcp_J*f;
          
          //PRINT(rcp_J);
          const LinearSpace2<Vec2f> rcp_J2(rcp_J);
          const LinearSpace2<Vec2<Interval1f>> rcp_Ji(rcp_J2);
          
          const Vec2<Interval1f> x(cu,cv);
          const Vec2<Interval1f> K = Vec2<Interval1f>(Vec2f(c1)) + (I - rcp_Ji*G)*(x-Vec2<Interval1f>(Vec2f(c)));

          DBG(tab(depth); PRINT(K));

          const Vec2<Interval1f> KK = intersect(K,x);
          if (isEmpty(KK.x) || isEmpty(KK.y)) return true;

          if (!subset(K,x)) return false;

          solve_newton_raphson2b(cu,cv,c1,dfdu,dfdv,rcp_J);
          return true;
        }
        
        void solve_newton_raphson(BBox1f cu, BBox1f cv, TensorLinearCubicBezierSurface2fa curve2)
        {
          counters.numRecursions++;
          
          BBox2fa bounds = curve2.bounds();
          if (bounds.upper.x < 0.0f) return;
          if (bounds.upper.y < 0.0f) return;
          if (bounds.lower.x > 0.0f) return;
          if (bounds.lower.y > 0.0f) return;

          Vec2fa du = curve2.axis_u();
          Vec2fa dv = curve2.axis_v();
          Vec2fa ndu = Vec2fa(-du.y,du.x);
          Vec2fa ndv = Vec2fa(-dv.y,dv.x);
          BBox1f boundsu = curve2.bounds(ndu);
          BBox1f boundsv = curve2.bounds(ndv);
          if (boundsu.upper < 0.0f) return;
          if (boundsv.upper < 0.0f) return;
          if (boundsu.lower > 0.0f) return;
          if (boundsv.lower > 0.0f) return;

          {
            const Vec2fa dv = normalize(curve2.axis_v());
            
            const TensorLinearCubicBezierSurface1f curve1v = curve2.xfm(dv);
            LinearBezierCurve<Interval1f> curve0v = curve1v.reduce_u();
            if (!curve0v.hasRoot()) return;       
            const Interval1f v = bezier_clipping(curve0v);
            if (isEmpty(v)) return;
            curve2 = curve2.clip_v(v);
            cv = BBox1f(lerp(cv.lower,cv.upper,v.lower),lerp(cv.lower,cv.upper,v.upper));
          }

          if (cu.size() < 0.0001f)
            return solve_newton_raphson2(cu,cv,curve2);
              
          if (solve_krawczyk(cu,cv,curve2,0))
            return;
          
          TensorLinearCubicBezierSurface2fa curve2l, curve2r;
          curve2.split_u(curve2l,curve2r);
          solve_newton_raphson(BBox1f(cu.lower,cu.center()),cv,curve2l);
          solve_newton_raphson(BBox1f(cu.center(),cu.upper),cv,curve2r);
        }

        void tab(int depth)
        {
          for (int i=0; i<depth; i++)
            std::cout << "  ";
        }

        void solve_newton_raphson_wide(BBox1f cu, BBox1f cv, int depth)
        {
          asm("//solve_newton_raphson_wide_a");
          //TensorLinearCubicBezierSurface2fa curve2 = curve2d.clip(cu,cv);
          counters.numRecursions++;
          asm("//solve_newton_raphson_wide_b");

#if 0
          if (!clip_v(cu,cv))
            return;
#endif

          DBG(tab(depth); PRINT2(cu,cv));

          if (cu.size() < 0.001f) {
            DBG(tab(depth); PRINT("forced newton"));
            if (!clip_v(cu,cv)) return;
            return solve_newton_raphson2(cu,cv);
          }

          /* we assume convergence for small u ranges and verify using krawczyk */
          if (cu.size() < 1.0f/40.0f) {
            if (solve_krawczyk(cu,cv,depth)) {
              DBG(tab(depth); PRINT("krawczyk_cull"));
              return;
            }
          }

          asm("//solve_newton_raphson_wide_c");
          
          DBG(tab(depth); PRINT("split"));
          TensorLinearCubicBezierSurface<Vec2vfx> subcurves = curve2d.clip_v(cv).split_u(cu);
          asm("//solve_newton_raphson_wide_d");
          
          vboolx valid = true; clear(valid,VSIZEX-1);
#if 0
          const Vec2vfx Dv = normalize(subcurves.axis_v());
          const TensorLinearCubicBezierSurface<vfloatx> curve1v = subcurves.xfmX(Dv);
          LinearBezierCurve<Interval<vfloatx>> curve0v = curve1v.vreduce_u();
          valid &= curve0v.hasRootX();
          if (none(valid)) return;
          const Interval<vfloatx> v = bezier_clipping(valid,curve0v);
          valid &= !isEmpty(v);
          if (none(valid)) return;
          subcurves = subcurves.vclip_v(v);
          BBox<vfloatx> vcv(lerp(cv.lower,cv.upper,v.lower),lerp(cv.lower,cv.upper,v.upper));
#endif
#if 0
          BBox<Vec2vfx> bounds = subcurves.bounds();
          valid &= bounds.upper.x >= -eps;
          valid &= bounds.upper.y >= -eps;
          valid &= bounds.lower.x <= eps;
          valid &= bounds.lower.y <= eps;
          asm("//solve_newton_raphson_wide_e");
          if (none(valid)) return;
#endif

          //if (__popcnt(movemask(valid)) > 1)
          {
            Vec2vfx du = subcurves.axis_u();
            //DBG(tab(depth); PRINT(du));
            Vec2vfx ndu = Vec2vfx(-du.y,du.x);
            BBox<vfloatx> boundsu = subcurves.vbounds(ndu);
            //DBG(tab(depth); PRINT(boundsu));
            valid &= boundsu.lower <= eps;
            valid &= boundsu.upper >= -eps;
            //DBG(tab(depth); PRINT(valid));
            asm("//solve_newton_raphson_wide_f");
            if (none(valid)) return;
            
            Vec2vfx dv = subcurves.axis_v();
            //DBG(tab(depth); PRINT(dv));
            Vec2vfx ndv = Vec2vfx(-dv.y,dv.x);
            BBox<vfloatx> boundsv = subcurves.vbounds(ndv);
            valid &= boundsv.lower <= eps;
            valid &= boundsv.upper >= -eps;
            //DBG(tab(depth); PRINT(valid));
            asm("//solve_newton_raphson_wide_g");
            if (none(valid)) return;
          }

          DBG(tab(depth); PRINT(valid));
           
          size_t mask = movemask(valid);
          while (mask)
          {
            const size_t i = __bscf(mask);
            const float u0 = float(i+0)*(1.0f/(VSIZEX-1));
            const float u1 = float(i+1)*(1.0f/(VSIZEX-1));
            const BBox1f cui(lerp(cu.lower,cu.upper,u0),lerp(cu.lower,cu.upper,u1));
            //const BBox1f cvi(vcv.lower[i],vcv.upper[i]);
            const BBox1f cvi = cv;
            solve_newton_raphson_wide(cui,cvi,depth+1);
          }
        }
        
        bool solve_newton_raphson_main()
        {
           asm("//solve_newton_raphson_main");
          curve2d = curve3d.xfm(space.vx,space.vy,ray.org);
          eps = 16.0f*float(ulp)*reduce_max(curve2d.bounds().size());

#if 0
          {
            BBox2fa bounds = curve2.bounds();
            if (bounds.upper.x < 0.0f) return false;
            if (bounds.upper.y < 0.0f) return false;
            if (bounds.lower.x > 0.0f) return false;
            if (bounds.lower.y > 0.0f) return false;
            
            Vec2fa du = curve2.axis_u();
            Vec2fa dv = curve2.axis_v();
            Vec2fa ndu = Vec2fa(-du.y,du.x);
            Vec2fa ndv = Vec2fa(-dv.y,dv.x);
            BBox1f boundsu = curve2.bounds(ndu);
            BBox1f boundsv = curve2.bounds(ndv);
            if (boundsu.upper < 0.0f) return false;
            if (boundsv.upper < 0.0f) return false;
            if (boundsu.lower > 0.0f) return false;
            if (boundsv.lower > 0.0f) return false;
          }
#endif
          
          BBox1f vu(0.0f,1.0f);
          BBox1f vv(0.0f,1.0f);
          //BBox1f vu(0.183673f, 0.204082f); curve2 = curve2.clip_u(vu);
          //BBox1f vv(0.377083f, 0.409252f); curve2 = curve2.clip_v(vv);
            
          //solve_newton_raphson(BBox1f(0.0f,1.0f),BBox1f(0.0f,1.0f),curve2d);
          solve_newton_raphson_wide(vu,vv,0);
          if (isHit) {
            //((RayHit&)ray).u = counters.numRecursions/16.0f;
            //((RayHit&)ray).u = counters.numKrawczyk/4.0f;
            //((RayHit&)ray).u = counters.numSolve/4.0f;
            ((RayHit&)ray).u = counters.numSolveIterations/20.0f;
            ((RayHit&)ray).v = 0.0f;
          }
          return isHit;
        }
      };

    
    struct OrientedBezierCurve1Intersector1
    {
      __forceinline OrientedBezierCurve1Intersector1() {}
      
      __forceinline OrientedBezierCurve1Intersector1(const Ray& ray, const void* ptr) {}
      
      template<typename Epilog>
      __noinline bool intersect(Ray& ray,
                                const Vec3fa& v0, const Vec3fa& v1, const Vec3fa& v2, const Vec3fa& v3,
                                const Vec3fa& n0, const Vec3fa& n1, const Vec3fa& n2, const Vec3fa& n3,
                                const Epilog& epilog) const
      {
        asm("//intersect");
        STAT3(normal.trav_prims,1,1,1);
        
        CubicBezierCurve3fa center(v0,v1,v2,v3);
        Vec3fa d0 = v0.w*normalize(cross(n0,center.eval_dt(0.0f/3.0f)));
        Vec3fa d1 = v1.w*normalize(cross(n1,center.eval_dt(1.0f/3.0f)));
        Vec3fa d2 = v2.w*normalize(cross(n2,center.eval_dt(2.0f/3.0f)));
        Vec3fa d3 = v3.w*normalize(cross(n3,center.eval_dt(3.0f/3.0f)));

        CubicBezierCurve3fa L(v0-d0,v1-d1,v2-d2,v3-d3);
        CubicBezierCurve3fa R(v0+d0,v1+d1,v2+d2,v3+d3);
        TensorLinearCubicBezierSurface3fa curve(L,R);
        //return TensorLinearCubicBezierSurfaceIntersector<Epilog>(ray,curve,epilog).solve();
        return TensorLinearCubicBezierSurfaceIntersector<Epilog>(ray,curve,epilog).solve_newton_raphson_main();
      }
    };
  }
}