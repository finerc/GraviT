/* =======================================================================================
   This file is released as part of GraviT - scalable, platform independent ray tracing
   tacc.github.io/GraviT

   Copyright 2013-2015 Texas Advanced Computing Center, The University of Texas at Austin
   All rights reserved.

   Licensed under the BSD 3-Clause License, (the "License"); you may not use this file
   except in compliance with the License.
   A copy of the License is included with this software in the file LICENSE.
   If your copy does not contain the License, you may obtain a copy of the License at:

       http://opensource.org/licenses/BSD-3-Clause

   Unless required by applicable law or agreed to in writing, software distributed under
   the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
   KIND, either express or implied.
   See the License for the specific language governing permissions and limitations under
   limitations under the License.

   GraviT is funded in part by the US National Science Foundation under awards ACI-1339863,
   ACI-1339881 and ACI-1339840
   ======================================================================================= */
/*
 * File:   Ray.cu
 * Author: Roberto Ribeiro
 *
 * Created on February 20, 2016, 19:00 PM
 */

#include "Ray.cuh"
#include "cutil_math.h"

namespace gvt {
namespace render {
namespace data {
namespace cuda_primitives {
/// surface material properties
/** surface material properties used to shade intersected geometry
*/

__device__ void Ray::setDirection(float4 dir) {
		//direction = dir;
	  dir.w = 0;
	  direction = normalize(dir);
	  //for (int i = 0; i < 3; i++) {
//	    if (direction.x != 0)
//	      inverseDirection.x = 1.0 / direction.x;
//	    else
//	      inverseDirection.x = 0.;
//
//	    if (direction.y != 0)
//	    	      inverseDirection.y = 1.0 / direction.y;
//	    	    else
//	    	      inverseDirection.y = 0.;
//
//	    if (direction.z != 0)
//	    	      inverseDirection.z = 1.0 / direction.z;
//	    	    else
//	    	      inverseDirection.z = 0.;
	  //}
	}

}
}
}
}
