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
 * File:   SchedulerBase.h
 * Author: jbarbosa
 *
 * Created on January 21, 2014, 4:07 PM
 */

#ifndef GVT_CORE_SCHEDULE_SCHEDULER_BASE_H
#define GVT_CORE_SCHEDULE_SCHEDULER_BASE_H

#include <gvt/core/Debug.h>
#include <gvt/core/comm/message.h>

namespace gvt {
namespace tracer {

/// base class for work schedulers
class SchedulerBase {
public:
  SchedulerBase() {}

  virtual ~SchedulerBase() {}

  virtual void operator()() { GVT_ASSERT_BACKTRACE(false, "schedule not implemented"); }
};
}
}
#endif /* GVT_CORE_SCHEDULE_SCHEDULER_BASE_H */
