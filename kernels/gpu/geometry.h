// ======================================================================== //
// Copyright 2009-2019 Intel Corporation                                    //
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

#if defined(EMBREE_DPCPP_SUPPORT)
#include "common.h"

namespace embree
{
  namespace gpu
  {

    struct Triangle
    {
      unsigned int vtx[3];
      //unsigned int primID;
      //unsigned int geomID;
    };

    struct Quad1
    {
      cl::sycl::float4 v0,v2,v1,v3; //v1v3 loaded once

      void init(const cl::sycl::float4 &_v0,
		const cl::sycl::float4 &_v1,
		const cl::sycl::float4 &_v2,
		const cl::sycl::float4 &_v3,
		const uint geomID,
		const uint primID0,
		const uint primID1)
      {
	v0 = _v0;
	v1 = _v1;
	v2 = _v2;
	v3 = _v3;
	v1.w() = as_float(geomID);
	v0.w() = as_float(primID0);
	v2.w() = as_float(primID1);
	v3.w() = 0.0f;
      }
    };
    
  };
};

#endif