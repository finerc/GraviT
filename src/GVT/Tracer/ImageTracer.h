/*
 * ImageTracer.h
 *
 *  Created on: Nov 27, 2013
 *      Author: jbarbosa
 */

#ifndef GVT_IMAGETRACER_H
#define GVT_IMAGETRACER_H

#include <mpi.h>


#include <GVT/MPI/mpi_wrappers.h>
#include <GVT/Tracer/Tracer.h>
#include <GVT/Backend/MetaProcessQueue.h>
#include <GVT/Scheduler/schedulers.h>
#include <GVT/Concurrency/TaskScheduling.h>

#include <boost/foreach.hpp>
#include <boost/timer/timer.hpp>

namespace GVT {

    namespace Trace {
        /// Tracer Image (ImageSchedule) based decomposition implementation

        template<class DomainType, class MPIW> class Tracer<DomainType, MPIW, ImageSchedule> : public Tracer_base<MPIW> {
        public:

            Tracer(GVT::Data::RayVector& rays, Image& image) : Tracer_base<MPIW>(rays, image) {
                int ray_portion = this->rays.size() / this->world_size;
                this->rays_start = this->rank * ray_portion;
                this->rays_end = (this->rank + 1) == this->world_size ? this->rays.size() : (this->rank + 1) * ray_portion; // tack on any odd rays to last proc
            }

            virtual void operator()() {

               boost::timer::auto_cpu_timer t("imageTracer time: %ws\n");
               long ray_counter = 0, domain_counter = 0;

                // this->generateRays();

                // buffer for color accumulation
                GVT::Data::RayVector moved_rays;
                int domTarget = -1, domTargetCount = 0;
                // process domains until all rays are terminated
                do {
                    // process domain with most rays queued
                    domTarget = -1;
                    domTargetCount = 0;

                    GVT_DEBUG(DBG_ALWAYS, "Selecting new domain");
                    {
               boost::timer::auto_cpu_timer t("imageTracer selecting new domain time: %ws\n");
                    for (map<int, GVT::Data::RayVector>::iterator q = this->queue.begin(); q != this->queue.end(); ++q) {
                        if (q->second.size() > domTargetCount) {
                            domTargetCount = q->second.size();
                            domTarget = q->first;
                        }
                    }
                }
                    GVT_DEBUG(DBG_ALWAYS, "Selecting new domain");
                    if (domTarget != -1) std::cout << "Domain " << domTarget << " size " << this->queue[domTarget].size() << std::endl;
                    if (DEBUG_RANK) GVT_DEBUG(DBG_ALWAYS, this->rank << ": selected domain " << domTarget << " (" << domTargetCount << " rays)");
                    if (DEBUG_RANK) GVT_DEBUG(DBG_ALWAYS, this->rank << ": currently processed " << ray_counter << " rays across " << domain_counter << " domains");

                        printf("Carson: processing dom targets\n");
                    if (domTarget >= 0) {
                        printf("Carson: processing dom target\n");

                        GVT_DEBUG(DBG_ALWAYS, "Getting domain " << domTarget << endl);
                        GVT::Domain::Domain* dom = GVT::Env::RayTracerAttributes::rta->dataset->getDomain(domTarget);
                        // dom->load();
                        GVT_DEBUG(DBG_ALWAYS, "dom: " << domTarget << endl);

                        // track domain loads
                        ++domain_counter;
                        GVT_DEBUG(DBG_ALWAYS, "Calling process queue");
                        //GVT::Backend::ProcessQueue<DomainType>(new GVT::Backend::adapt_param<DomainType>(this->queue, moved_rays, domTarget, dom, this->colorBuf, ray_counter, domain_counter))();

               boost::timer::auto_cpu_timer t("imageTracer dom->trace time: %ws\n");
                        dom->trace(this->queue[domTarget], moved_rays);
                        // Carson TODO: create BVH
                        GVT::Backend::ProcessQueue<DomainType>(new GVT::Backend::adapt_param<DomainType>(this->queue, moved_rays, domTarget, dom, this->rta, this->colorBuf, ray_counter, domain_counter))();

                        while (!moved_rays.empty()) {
                            GVT::Data::ray* mr = moved_rays.back();

                            if(!mr->domains.empty()) {
                                dom->marchOut(mr);
                                int target = mr.domains.back();
                                this->queue[target].push_back(mr);
                                this->rta.dataset->getDomain(target)->marchIn(mr);
                                mr->domains.pop_back();
                            } else {
                                this->addRay(mr);
                            }

                            moved_rays.pop_back();
                        }
                        dom->free();
                        this->queue.erase(domTarget); // TODO: for secondary rays, rays may have been added to this domain queue
                    }

                        GVT_DEBUG(DBG_ALWAYS, "Marching rays");
                        //                        BOOST_FOREACH( GVT::Data::ray* mr,  moved_rays) {
                        //                            dom->marchOut(mr);
                        //                            GVT::Concurrency::asyncExec::instance()->run_task(processRay(this,mr));
                        //                        }

                        boost::atomic<int> current_ray(0);
                        size_t workload = std::max((size_t)1,(size_t)(moved_rays.size() / (GVT::Concurrency::asyncExec::instance()->numThreads * 2)));
                        for (int rc = 0; rc < GVT::Concurrency::asyncExec::instance()->numThreads; ++rc) {
                            GVT::Concurrency::asyncExec::instance()->run_task(processRayVector(this, moved_rays, current_ray, moved_rays.size(),workload, dom));
                        }
                        GVT::Concurrency::asyncExec::instance()->sync();

                        GVT_DEBUG(DBG_ALWAYS, "Finished queueing");
                        GVT::Concurrency::asyncExec::instance()->sync();
                        GVT_DEBUG(DBG_ALWAYS, "Finished marching");
                        //dom->free();
                        moved_rays.clear();
                        //this->queue.erase(domTarget); // TODO: for secondary rays, rays may have been added to this domain queue
                } while (domTarget != -1);
                GVT_DEBUG(DBG_ALWAYS, "Gathering buffers");
                {
               boost::timer::auto_cpu_timer t("imageTracer gatherframebuffers time: %ws\n");
                this->gatherFramebuffers(this->rays.size());
            }
            }
        };
    };
};
#endif /* IMAGETRACER_H_ */
