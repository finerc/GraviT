/* =======================================================================================
 This file is released as part of GraviT - scalable, platform independent ray
 tracing
 tacc.github.io/GraviT

 Copyright 2013-2015 Texas Advanced Computing Center, The University of Texas at
 Austin
 All rights reserved.

 Licensed under the BSD 3-Clause License, (the "License"); you may not use this
 file
 except in compliance with the License.
 A copy of the License is included with this software in the file LICENSE.
 If your copy does not contain the License, you may obtain a copy of the License
 at:

 http://opensource.org/licenses/BSD-3-Clause

 Unless required by applicable law or agreed to in writing, software distributed
 under
 the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY
 KIND, either express or implied.
 See the License for the specific language governing permissions and limitations
 under
 limitations under the License.

 GraviT is funded in part by the US National Science Foundation under awards
 ACI-1339863,
 ACI-1339881 and ACI-1339840
 =======================================================================================
 */
//
// OptixMeshAdapter.cpp
//
#include "gvt/render/adapter/optix/data/OptixMeshAdapter.h"
#include "gvt/core/CoreContext.h"

#include <gvt/core/Debug.h>
#include <gvt/core/Math.h>

#include <gvt/render/actor/Ray.h>
#include <gvt/render/data/DerivedTypes.h>
#include <gvt/render/data/primitives/Mesh.h>
#include <gvt/render/data/scene/ColorAccumulator.h>
#include <gvt/render/data/scene/Light.h>

#include <atomic>

#include <tbb/blocked_range.h>
#include <tbb/mutex.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_for_each.h>
#include <tbb/partitioner.h>
#include <tbb/tick_count.h>
#include <tbb/task_group.h>
#include <tbb/parallel_for.h>

#include <cuda.h>
#include <cuda_runtime.h>
#include <optix_prime/optix_primepp.h>
#include <optix.h>
#include <optix_cuda_interop.h>
#include "optix_prime/optix_prime.h"

#include "OptixMeshAdapter.cuh"


#include <float.h>

#include <gvt/render/data/primitives/Material.h>


//__inline__ void cudaRayToGvtRay(
//		const gvt::render::data::cuda_primitives::Ray& cudaRay,
//		gvt::render::actor::Ray& gvtRay) {
//
//	memcpy(&(gvtRay.origin[0]), &(cudaRay.origin.x), sizeof(glm::vec3));
//	memcpy(&(gvtRay.direction[0]), &(cudaRay.direction.x), sizeof(glm::vec3));
////	memcpy(&(gvtRay.inverseDirection[0]), &(cudaRay.inverseDirection.x),
////			sizeof(float) * 4);
//	memcpy(glm::value_ptr(gvtRay.color), &(cudaRay.color.x),
//			sizeof(glm::vec3));
//	gvtRay.id = cudaRay.id;
//	gvtRay.depth = cudaRay.depth;
//	gvtRay.w = cudaRay.w;
//	gvtRay.t = cudaRay.t;
//	gvtRay.t_min = cudaRay.t_min;
//	gvtRay.t_max = cudaRay.t_max;
//	gvtRay.type = cudaRay.type;
//
//}
//
//__inline__ void gvtRayToCudaRay(const gvt::render::actor::Ray& gvtRay,
//		gvt::render::data::cuda_primitives::Ray& cudaRay) {
//
////	memcpy(&(cudaRay.origin.x), &(gvtRay.origin[0]), sizeof(glm::vec3));
////	//cudaRay.origin.w=1.0f;
////	memcpy(&(cudaRay.direction.x), &(gvtRay.direction[0]),sizeof(glm::vec3));
//////	memcpy(&(cudaRay.inverseDirection.x), &(gvtRay.inverseDirection[0]),
//////			sizeof(float) * 4);
////	memcpy(&(cudaRay.color.x), glm::value_ptr(gvtRay.color),
////			sizeof(cuda_vec));
////	cudaRay.id = gvtRay.id;
////	cudaRay.depth = gvtRay.depth;
////	cudaRay.w = gvtRay.w;
////	cudaRay.t = gvtRay.t;
////	cudaRay.t_min = gvtRay.t_min;
////	cudaRay.t_max = gvtRay.t_max;
////	cudaRay.type = gvtRay.type;
//
//
////	memcpy(&cudaRay, gvtRay.data, 64);
//
//}

int3*
cudaCreateFacesToNormals(
		std::vector<gvt::render::data::primitives::Mesh::FaceToNormals>& gvt_face_to_normals) {

	int3* faces_to_normalsBuff;

	gpuErrchk(
			cudaMalloc((void ** ) &faces_to_normalsBuff,
					sizeof(int3) * gvt_face_to_normals.size()));

	std::vector<int3> faces_to_normals;
	for (int i = 0; i < gvt_face_to_normals.size(); i++) {

		const boost::tuple<int, int, int> &f = gvt_face_to_normals[i];

		int3 v = make_int3(f.get<0>(), f.get<1>(), f.get<2>());

		faces_to_normals.push_back(v);
	}

	gpuErrchk(
			cudaMemcpy(faces_to_normalsBuff, &faces_to_normals[0],
					sizeof(int3) * faces_to_normals.size(),
					cudaMemcpyHostToDevice));

	return faces_to_normalsBuff;

}

cuda_vec*
cudaCreateNormals(std::vector<glm::vec3>& gvt_normals) {

	cuda_vec* normalsBuff;

	gpuErrchk(
			cudaMalloc((void ** ) &normalsBuff,
					sizeof(cuda_vec) * gvt_normals.size()));

	std::vector<cuda_vec> normals;
	for (int i = 0; i < gvt_normals.size(); i++) {

		cuda_vec v = make_cuda_vec(gvt_normals[i].x, gvt_normals[i].y,
				gvt_normals[i].z);
		normals.push_back(v);
	}

	gpuErrchk(
			cudaMemcpy(normalsBuff, &normals[0],
					sizeof(cuda_vec) * gvt_normals.size(),
					cudaMemcpyHostToDevice));

	return normalsBuff;

}

int3*
cudaCreateFaces(
		std::vector<gvt::render::data::primitives::Mesh::Face>& gvt_faces) {

	int3* facesBuff;

	gpuErrchk(
			cudaMalloc((void ** ) &facesBuff, sizeof(int3) * gvt_faces.size()));

	std::vector<int3> faces;
	for (int i = 0; i < gvt_faces.size(); i++) {

		const boost::tuple<int, int, int> &f = gvt_faces[i];

		int3 v = make_int3(f.get<0>(), f.get<1>(), f.get<2>());
		faces.push_back(v);
	}

	gpuErrchk(
			cudaMemcpy(facesBuff, &faces[0], sizeof(int3) * gvt_faces.size(),
					cudaMemcpyHostToDevice));

	return facesBuff;

}

cuda_vec*
cudaCreateVertices(std::vector<glm::vec3>& gvt_verts) {

	cuda_vec* buff;

	gpuErrchk(
			cudaMalloc((void ** ) &buff,
					sizeof(cuda_vec) * gvt_verts.size()));

	std::vector<cuda_vec> verts;
	for (int i = 0; i < gvt_verts.size(); i++) {

		cuda_vec v = make_cuda_vec(gvt_verts[i].x, gvt_verts[i].y,
				gvt_verts[i].z);
		verts.push_back(v);
	}

	gpuErrchk(
			cudaMemcpy(buff, &verts[0],
					sizeof(cuda_vec) * gvt_verts.size(),
					cudaMemcpyHostToDevice));

	return buff;

}
gvt::render::data::primitives::Material *
cudaCreateMaterial(gvt::render::data::primitives::Material *gvtMat) {

	gvt::render::data::primitives::Material *cudaMat_ptr;

	gpuErrchk(
			cudaMalloc((void ** ) &cudaMat_ptr,
					sizeof(gvt::render::data::primitives::Material)));

	gpuErrchk(
			cudaMemcpy(cudaMat_ptr, gvtMat,
					sizeof(gvt::render::data::primitives::Material),
					cudaMemcpyHostToDevice));

	return cudaMat_ptr;
}

void cudaSetRays(gvt::render::actor::RayVector::iterator gvtRayVector,
		int localRayCount,
		gvt::render::data::cuda_primitives::Ray *cudaRays_devPtr, int localIdx,
		cudaStream_t& stream,
		gvt::render::data::cuda_primitives::Ray* cudaRays) {

//	const int offset_rays =
//			localRayCount > std::thread::hardware_concurrency() ?
//					localRayCount / std::thread::hardware_concurrency() : 100;


//	static tbb::auto_partitioner ap;
//	tbb::parallel_for(tbb::blocked_range<int>(0, localRayCount, 128),
//			[&](tbb::blocked_range<int> chunk) {
//				for (int jj = chunk.begin(); jj < chunk.end(); jj++) {
//						gvtRayToCudaRay(gvtRayVector[jj], cudaRays[jj]);
//
//				}}, ap);

	//Copying to pinned
	memcpy(cudaRays, &gvtRayVector[0], sizeof(gvt::render::data::cuda_primitives::Ray)*localRayCount);

	gpuErrchk(
			cudaMemcpyAsync(cudaRays_devPtr, cudaRays,
					sizeof(gvt::render::data::cuda_primitives::Ray)
							* localRayCount, cudaMemcpyHostToDevice, stream));


}

void cudaGetRays(size_t& localDispatchSize,
		gvt::render::data::cuda_primitives::CudaGvtContext& cudaGvtCtx,
		gvt::render::data::cuda_primitives::Ray* disp_tmp,
		gvt::render::actor::RayVector& localDispatch,
		gvt::render::actor::RayVector::iterator &rayList) {


	gpuErrchk(
			cudaMemcpyAsync(disp_tmp, cudaGvtCtx.dispatch,
					sizeof(gvt::render::data::cuda_primitives::Ray)
							* cudaGvtCtx.dispatchCount, cudaMemcpyDeviceToHost,
					cudaGvtCtx.stream));

	gpuErrchk(cudaStreamSynchronize(cudaGvtCtx.stream));

	memcpy(&localDispatch[localDispatchSize],disp_tmp,sizeof(gvt::render::data::cuda_primitives::Ray)
			* cudaGvtCtx.dispatchCount);


//	static tbb::auto_partitioner ap;
//	tbb::parallel_for(tbb::blocked_range<int>(0, cudaGvtCtx.dispatchCount, 128),
//			[&](tbb::blocked_range<int> chunk) {
//				for (int jj = chunk.begin(); jj < chunk.end(); jj++) {
//					if (jj < cudaGvtCtx.dispatchCount) {
//						gvt::render::actor::Ray& gvtRay =
//						localDispatch[localDispatchSize + jj];
//						const gvt::render::data::cuda_primitives::Ray& cudaRay =
//						disp_tmp[jj];
//
//						cudaRayToGvtRay(cudaRay, gvtRay);
//
//						//gvtRay.setDirection(gvtRay.direction);
//					}
//				}
//			}, ap);


	localDispatchSize += cudaGvtCtx.dispatchCount;
	cudaGvtCtx.dispatchCount = 0;


}

void cudaGetLights(std::vector<gvt::render::data::scene::Light *> gvtLights,
		gvt::render::data::cuda_primitives::Light * cudaLights_devPtr) {

	gvt::render::data::cuda_primitives::Light *cudaLights =
			new gvt::render::data::cuda_primitives::Light[gvtLights.size()];

	for (int i = 0; i < gvtLights.size(); i++) {

		if (dynamic_cast<gvt::render::data::scene::AmbientLight *>(gvtLights[i])
				!=
				NULL) {

			gvt::render::data::scene::AmbientLight *l =
					dynamic_cast<gvt::render::data::scene::AmbientLight *>(gvtLights[i]);

			memcpy(&(cudaLights[i].ambient.position.x), &(l->position.x),
					sizeof(cuda_vec));
			memcpy(&(cudaLights[i].ambient.color.x), &(l->color.x),
					sizeof(cuda_vec));

			cudaLights[i].type =
					gvt::render::data::cuda_primitives::LIGH_TYPE::AMBIENT;

		} else if (dynamic_cast<gvt::render::data::scene::AreaLight *>(gvtLights[i])
				!= NULL) {

			gvt::render::data::scene::AreaLight *l =
					dynamic_cast<gvt::render::data::scene::AreaLight *>(gvtLights[i]);

			memcpy(&(cudaLights[i].area.position.x), &(l->position.x),
					sizeof(float4));

			memcpy(&(cudaLights[i].area.color.x), &(l->color.x),
					sizeof(float4));

			memcpy(&(cudaLights[i].area.u.x), &(l->u.x),
					sizeof(float4));

			memcpy(&(cudaLights[i].area.v.x), &(l->v.x),
					sizeof(float4));

			memcpy(&(cudaLights[i].area.w.x), &(l->w.x),
					sizeof(float4));

			memcpy(&(cudaLights[i].area.LightNormal.x), &(l->LightNormal.x),
					sizeof(float4));

			cudaLights[i].area.LightHeight = l->LightHeight;

			cudaLights[i].area.LightWidth = l->LightWidth;


			cudaLights[i].type =
					gvt::render::data::cuda_primitives::LIGH_TYPE::AREA;

		  } else if (dynamic_cast<gvt::render::data::scene::PointLight *>(gvtLights[i])
				  != NULL) {

                          gvt::render::data::scene::PointLight *l =
                                          dynamic_cast<gvt::render::data::scene::PointLight *>(gvtLights[i]);

                          memcpy(&(cudaLights[i].point.position.x), &(l->position.x),
                                          sizeof(float4));
                          memcpy(&(cudaLights[i].point.color.x), &(l->color.x),
                                          sizeof(float4));

                          cudaLights[i].type =
                                          gvt::render::data::cuda_primitives::LIGH_TYPE::POINT;


		} else {
			std::cout << "Unknown light" << std::endl;
			return;
		}
	}

	gpuErrchk(
			cudaMemcpy(cudaLights_devPtr, cudaLights,
					sizeof(gvt::render::data::cuda_primitives::Light)
							* gvtLights.size(), cudaMemcpyHostToDevice));

	delete[] cudaLights;

}

gvt::render::data::cuda_primitives::Mesh cudaInstanceMesh(
		gvt::render::data::primitives::Mesh* mesh) {

	gvt::render::data::cuda_primitives::Mesh cudaMesh;


	cudaMesh.faces_to_normals = cudaCreateFacesToNormals(
			mesh->faces_to_normals);
	cudaMesh.normals = cudaCreateNormals(mesh->normals);
	cudaMesh.faces = cudaCreateFaces(mesh->faces);
	cudaMesh.mat = cudaCreateMaterial(mesh->mat);
	cudaMesh.vertices = cudaCreateVertices(mesh->vertices);


//	gvt::render::data::cuda_primitives::Mesh* cudaMesh_dev;
//	gpuErrchk(
//				cudaMalloc((void ** ) &cudaMesh_dev,
//						sizeof(gvt::render::data::cuda_primitives::Mesh)));
//
//	gpuErrchk(cudaMemcpy(cudaMesh_dev, &cudaMesh,
//			sizeof(gvt::render::data::cuda_primitives::Mesh),
//				cudaMemcpyHostToDevice));



	return cudaMesh;
}

void gvt::render::data::cuda_primitives::CudaGvtContext::initCudaBuffers(
		int packetSize) {

	gvt::core::DBNodeH root = gvt::core::CoreContext::instance()->getRootNode();

	auto lightNodes = root["Lights"].getChildren();
	std::vector<gvt::render::data::scene::Light *> lights;
	lights.reserve(2);
	for (auto lightNode : lightNodes) {
		auto color = lightNode["color"].value().tovec3();

		if (lightNode.name() == std::string("PointLight")) {
			auto pos = lightNode["position"].value().tovec3();
			lights.push_back(
					new gvt::render::data::scene::PointLight(pos, color));
		} else if (lightNode.name() == std::string("AmbientLight")) {
			lights.push_back(new gvt::render::data::scene::AmbientLight(color));
		}
		 else if (lightNode.name() == std::string("AreaLight")) {
			 auto pos = lightNode["position"].value().tovec3();
			         auto normal = lightNode["normal"].value().tovec3();
			         auto width = lightNode["width"].value().toFloat();
			         auto height = lightNode["height"].value().toFloat();
			         lights.push_back(new gvt::render::data::scene::AreaLight(pos, color, normal, width, height));
		 }
	}

	gvt::render::data::cuda_primitives::Light *cudaLights_devPtr;
	gpuErrchk(
			cudaMalloc((void ** )&cudaLights_devPtr,
					sizeof(gvt::render::data::cuda_primitives::Light)
							* lights.size()));

	cudaGetLights(lights, cudaLights_devPtr);

//	gvt::render::data::cuda_primitives::Matrix3f *normiBuff;
//	gpuErrchk(
//			cudaMalloc((void ** ) &normiBuff,
//					sizeof(gvt::render::data::cuda_primitives::Matrix3f)));
//
//	gvt::render::data::cuda_primitives::Matrix4f *minvBuff;
//	gpuErrchk(
//			cudaMalloc((void ** ) &minvBuff,
//					sizeof(gvt::render::data::cuda_primitives::Matrix4f)));

	gvt::render::data::cuda_primitives::OptixRay *cudaOptixRayBuff;
	gpuErrchk(
			cudaMalloc((void ** ) &cudaOptixRayBuff,
					sizeof(gvt::render::data::cuda_primitives::OptixRay)
							* packetSize * lights.size()));

	gvt::render::data::cuda_primitives::OptixHit *cudaHitsBuff;
	gpuErrchk(
			cudaMalloc((void ** ) &cudaHitsBuff,
					sizeof(gvt::render::data::cuda_primitives::OptixHit)
							* packetSize * lights.size()));

	gvt::render::data::cuda_primitives::Ray *shadowRaysBuff;
	gpuErrchk(
			cudaMalloc((void ** ) &shadowRaysBuff,
					sizeof(gvt::render::data::cuda_primitives::Ray) * packetSize
							* lights.size()));

	bool *validBuff;
	gpuErrchk(cudaMalloc((void **) &validBuff, sizeof(bool) * packetSize));

	gvt::render::data::cuda_primitives::Ray *cudaRays_devPtr;
	gpuErrchk(
			cudaMalloc((void ** ) &cudaRays_devPtr,
					sizeof(gvt::render::data::cuda_primitives::Ray)
							* packetSize));

	// Size is rayCount but depends on instance, so majoring to double packetsize
	gvt::render::data::cuda_primitives::Ray * dispatchBuff;
	gpuErrchk(
			cudaMalloc((void ** ) &dispatchBuff,
					sizeof(gvt::render::data::cuda_primitives::Ray) *
					/*(end-begin)*/packetSize * 2));

	set_random_states(packetSize);

	gpuErrchk(
			cudaMalloc(&devicePtr,
					sizeof(gvt::render::data::cuda_primitives::CudaGvtContext)));

	gpuErrchk(cudaStreamCreate(&stream));

	rays = cudaRays_devPtr;
	traceRays = cudaOptixRayBuff;
	traceHits = cudaHitsBuff;
	this->lights = cudaLights_devPtr;
	shadowRays = shadowRaysBuff;
	nLights = lights.size();
	valid = validBuff;
	dispatch = dispatchBuff;
	//normi = normiBuff;
	//minv = minvBuff;

	dirty = true;

}

// TODO: add logic for other packet sizes
#define GVT_OPTIX_PACKET_SIZE 4096

using namespace gvt::render::actor;
using namespace gvt::render::adapter::optix::data;
using namespace gvt::render::data::primitives;

static std::atomic<size_t> counter(0);


OptixContext *OptixContext::_singleton;

OptixMeshAdapter::OptixMeshAdapter(std::map<int, gvt::render::data::primitives::Mesh *> &meshRef,
				                                     std::map<int, glm::mat4 *> &instM, std::map<int, glm::mat4 *> &instMinv,
				                                     std::map<int, glm::mat3 *> &instMinvN,
				                                     std::vector<gvt::render::data::scene::Light *> &lights,
				                                     std::vector<size_t> instances, bool unique)
				    : Adapter(meshRef, instM, instMinv, instMinvN, lights, instances, unique),
						packetSize(GVT_OPTIX_PACKET_SIZE) {

	cudaSetDevice(0);

	optix_context_= OptixContext::singleton()->context();

	GVT_ASSERT(optix_context_.isValid(), "Optix Context is not valid");

	// Create Optix Prime Context

	// Use all CUDA devices, if multiple are present ignore the GPU driving the
	// display
	{
		std::vector<unsigned> activeDevices;
		int devCount = 0;
		cudaDeviceProp prop;
		gpuErrchk(cudaGetDeviceCount(&devCount));
		GVT_ASSERT(devCount,
				"You choose optix render, but no cuda capable devices are present");

		for (int i = 0; i < devCount; i++) {
			gpuErrchk(cudaGetDeviceProperties(&prop, i));
//			if (prop.kernelExecTimeoutEnabled == 0)
//				activeDevices.push_back(i);
			// Oversubcribe the GPU
			packetSize = prop.multiProcessorCount
					* prop.maxThreadsPerMultiProcessor * 8 /* hand tunned value*/;

		}
		if (!activeDevices.size()) {
			activeDevices.push_back(0);
		}
		optix_context_->setCudaDeviceNumbers(activeDevices);
		gpuErrchk(cudaGetLastError());
	}

	OptixContext::singleton()->initCuda(packetSize);


	cudaMallocHost(&(disp_Buff[0]),
			sizeof(gvt::render::data::cuda_primitives::Ray) * packetSize * 2);
	cudaMallocHost(&(cudaRaysBuff[0]),
			sizeof(gvt::render::data::cuda_primitives::Ray) * packetSize);
	cudaMallocHost(&(disp_Buff[1]),
			sizeof(gvt::render::data::cuda_primitives::Ray) * packetSize * 2);
	cudaMallocHost(&(cudaRaysBuff[1]),
			sizeof(gvt::render::data::cuda_primitives::Ray) * packetSize);


	gpuErrchk(
			cudaMalloc((void ** ) &m_dev,
					sizeof(gvt::render::data::cuda_primitives::Matrix4f)
							* instM.size()));

	gpuErrchk(
			cudaMalloc((void ** ) &normi_dev,
					sizeof(gvt::render::data::cuda_primitives::Matrix3f)
							* instMinvN.size()));

	std::set<gvt::render::data::primitives::Mesh *> _mesh;
	std::map<gvt::render::data::primitives::Mesh *,
			gvt::render::data::cuda_primitives::Mesh > mesh2cudaMesh;

	std::vector<RTPmodel> _instList;
	std::vector<gvt::render::data::cuda_primitives::Mesh > _inst2mesh;
	std::vector<gvt::render::data::cuda_primitives::Matrix4f> _inst2mat;


	int instID =0;
	for (auto &inst : instances) {

		Mesh *mesh = meshRef[inst];

		mesh->generateNormals();
		GVT_ASSERT(mesh,
				"OptixMeshAdapter: mesh pointer in the database is null");

		if (std::find(_mesh.begin(), _mesh.end(), mesh) == _mesh.end()) {


			int numVerts = mesh->vertices.size();
			int numTris = mesh->faces.size();

			gvt::render::data::cuda_primitives::Mesh cudaMesh = cudaInstanceMesh(mesh);

			// Setup the buffer to hold our vertices.
			//
			std::vector<float> vertices;
			std::vector<int> faces;

			vertices.resize(numVerts * 3);
			faces.resize(numTris * 3);

			static tbb::auto_partitioner ap;
			tbb::parallel_for(tbb::blocked_range<int>(0, numVerts, 128),
					[&](tbb::blocked_range<int> chunk) {
						for (int jj = chunk.begin(); jj < chunk.end(); jj++) {
							vertices[jj * 3 + 0] = mesh->vertices[jj][0];
							vertices[jj * 3 + 1] = mesh->vertices[jj][1];
							vertices[jj * 3 + 2] = mesh->vertices[jj][2];
						}
					}, ap);

			tbb::parallel_for(tbb::blocked_range<int>(0, numTris, 128),
					[&](tbb::blocked_range<int> chunk) {
						for (int jj = chunk.begin(); jj < chunk.end(); jj++) {
							gvt::render::data::primitives::Mesh::Face f = mesh->faces[jj];
							faces[jj * 3 + 0] = f.get<0>();
							faces[jj * 3 + 1] = f.get<1>();
							faces[jj * 3 + 2] = f.get<2>();
						}
					}, ap);

			// Create and setup vertex buffer
			::optix::prime::BufferDesc vertices_desc;
			vertices_desc = optix_context_->createBufferDesc(
					RTP_BUFFER_FORMAT_VERTEX_FLOAT3, RTP_BUFFER_TYPE_HOST,
					&vertices[0]);

			GVT_ASSERT(vertices_desc.isValid(), "Vertices are not valid");
			vertices_desc->setRange(0, vertices.size() / 3);
			vertices_desc->setStride(sizeof(float) * 3);

			// Create and setup triangle buffer
			::optix::prime::BufferDesc indices_desc;
			indices_desc = optix_context_->createBufferDesc(
					RTP_BUFFER_FORMAT_INDICES_INT3, RTP_BUFFER_TYPE_HOST,
					&faces[0]);

			GVT_ASSERT(indices_desc.isValid(), "Indices are not valid");
			indices_desc->setRange(0, faces.size() / 3);
			indices_desc->setStride(sizeof(int) * 3);


			::optix::prime::Model optix_model_;
			optix_model_ = optix_context_->createModel();
			GVT_ASSERT(optix_model_.isValid(), "Model is not valid");
			optix_model_->setTriangles(indices_desc, vertices_desc);
			optix_model_->update(RTP_MODEL_HINT_NONE);

			_instList.push_back(optix_model_->getRTPmodel());
			_mesh.insert(mesh);
			_inst2mesh.push_back(cudaMesh);
			mesh2model[mesh] = optix_model_;
			mesh2cudaMesh[mesh] = cudaMesh;

		} else {

			::optix::prime::Model optix_model_ = mesh2model[mesh];
			GVT_ASSERT(optix_model_.isValid(), "Model is not valid");

			_inst2mesh.push_back(mesh2cudaMesh[mesh]);

			_instList.push_back(optix_model_->getRTPmodel());


		}

		const float* m_ = glm::value_ptr(glm::transpose(*(instM[inst])));
		_inst2mat.push_back(*(gvt::render::data::cuda_primitives::Matrix4f*)m_);

		const float* norm_ = glm::value_ptr(glm::transpose((*(instMinvN[inst]))));

					gpuErrchk(
							cudaMemcpy(normi_dev + instID,norm_,
									sizeof(gvt::render::data::cuda_primitives::Matrix3f), cudaMemcpyHostToDevice));

		instID++;
	}

	::optix::prime::BufferDesc instancesBuff;
		instancesBuff = optix_context_->createBufferDesc(
				RTP_BUFFER_FORMAT_INSTANCE_MODEL, RTP_BUFFER_TYPE_HOST,
				&_instList[0]);

		instancesBuff->setRange(0, _instList.size());

		::optix::prime::BufferDesc transforms;
		transforms = optix_context_->createBufferDesc(
				RTP_BUFFER_FORMAT_TRANSFORM_FLOAT4x4, RTP_BUFFER_TYPE_HOST,
				&_inst2mat[0]);

		transforms->setRange(0, _inst2mat.size());



		// create the "scene" model

		_scene = optix_context_->createModel();
		_scene->setInstances(instancesBuff, transforms );
		_scene->update(RTP_MODEL_HINT_NONE);


		gpuErrchk(
					cudaMalloc((void ** ) &_inst2mesh_dev,
							sizeof(gvt::render::data::cuda_primitives::Mesh)
									* _inst2mesh.size()));

		gpuErrchk(cudaMemcpy(_inst2mesh_dev, &_inst2mesh[0],
				sizeof(gvt::render::data::cuda_primitives::Mesh )
													* _inst2mesh.size(),
									cudaMemcpyHostToDevice));

}

OptixMeshAdapter::~OptixMeshAdapter() {

	cudaFreeHost(disp_Buff);
	cudaFreeHost(cudaRaysBuff);
	cudaFree(normi_dev);
	cudaFree(m_dev);


}

struct OptixParallelTrace {

	gvt::render::adapter::optix::data::OptixMeshAdapter *adapter;

	/**
	 * Shared ray list used in the current trace() call
	 */
	gvt::render::actor::RayVector::iterator &rayList;

	/**
	 * Shared outgoing ray list used in the current trace() call
	 */
	gvt::render::actor::RayVector &moved_rays;

	/**
	 * Thread local outgoing ray queue
	 */
	gvt::render::actor::RayVector localDispatch;

	gvt::render::data::cuda_primitives::Ray* disp_Buff;

	gvt::render::data::cuda_primitives::Ray* cudaRaysBuff;

	/**
	 * Size of Optix-CUDA packet
	 */
	size_t packetSize; // TODO: later make this configurable

	const size_t begin, end;

	/**
	 * Construct a OptixParallelTrace struct with information needed for the
	 * thread
	 * to do its tracing
	 */
	OptixParallelTrace(
			gvt::render::adapter::optix::data::OptixMeshAdapter *adapter,
			gvt::render::actor::RayVector::iterator &rayList,
			gvt::render::actor::RayVector &moved_rays,
//			 glm::mat4 *m, glm::mat4 *minv,
//			glm::mat3 *normi,
//			//std::vector<gvt::render::data::scene::Light *> &lights,
			std::atomic<size_t> &counter, const size_t begin, const size_t end,
			gvt::render::data::cuda_primitives::Ray* disp_Buff,
			gvt::render::data::cuda_primitives::Ray* cudaRaysBuff) :
			adapter(adapter), rayList(rayList), moved_rays(moved_rays), /* m(m), minv(minv), normi(normi),*/
			packetSize(adapter->getPacketSize()), begin(begin), end(end), disp_Buff(
					disp_Buff), cudaRaysBuff(cudaRaysBuff) {
	}

	/**
	 * Test occlusion for stored shadow rays.  Add missed rays
	 * to the dispatch queue.
	 */
	void traceShadowRays(
			gvt::render::data::cuda_primitives::CudaGvtContext& cudaGvtCtx) {

		::optix::prime::Model model = adapter->getScene();

		RTPquery query;
		CHK_PRIME(rtpQueryCreate(model->getRTPmodel(), RTP_QUERY_TYPE_CLOSEST, &query));

		cudaPrepOptixRays(cudaGvtCtx.traceRays, NULL, cudaGvtCtx.shadowRayCount,
				cudaGvtCtx.shadowRays, &cudaGvtCtx, true, cudaGvtCtx.stream);

		RTPbufferdesc desc;
		CHK_PRIME(rtpBufferDescCreate(
				OptixContext::singleton()->context()->getRTPcontext(),
				RTP_BUFFER_FORMAT_RAY_ORIGIN_TMIN_DIRECTION_TMAX,
				RTP_BUFFER_TYPE_CUDA_LINEAR, cudaGvtCtx.traceRays, &desc));

		CHK_PRIME(rtpBufferDescSetRange(desc, 0, cudaGvtCtx.shadowRayCount));
		CHK_PRIME(rtpQuerySetRays(query, desc));

		RTPbufferdesc desc2;
		CHK_PRIME(rtpBufferDescCreate(
				OptixContext::singleton()->context()->getRTPcontext(),
				RTP_BUFFER_FORMAT_HIT_T_TRIID_INSTID_U_V, RTP_BUFFER_TYPE_CUDA_LINEAR,
				cudaGvtCtx.traceHits, &desc2));

		CHK_PRIME(rtpBufferDescSetRange(desc2, 0, cudaGvtCtx.shadowRayCount));
		CHK_PRIME(rtpQuerySetHits(query, desc2));

		CHK_PRIME(rtpQuerySetCudaStream(query, cudaGvtCtx.stream));

		CHK_PRIME(rtpQueryExecute(query, RTP_QUERY_HINT_ASYNC));
		//rtpQueryFinish(query);

		cudaProcessShadows(&cudaGvtCtx);

	}

	void traceRays(
			gvt::render::data::cuda_primitives::CudaGvtContext& cudaGvtCtx) {

//		gpuErrchk(
//				cudaMemsetAsync(cudaGvtCtx.traceRays, 0,
//						sizeof(gvt::render::data::cuda_primitives::OptixRay)
//								* cudaGvtCtx.rayCount, cudaGvtCtx.stream));
//
//		gpuErrchk(
//				cudaMemsetAsync(cudaGvtCtx.traceHits, 0,
//						sizeof(gvt::render::data::cuda_primitives::OptixHit)
//								* cudaGvtCtx.rayCount, cudaGvtCtx.stream));

		cudaPrepOptixRays(cudaGvtCtx.traceRays, cudaGvtCtx.valid,
				cudaGvtCtx.rayCount, cudaGvtCtx.rays, &cudaGvtCtx, false,
				cudaGvtCtx.stream);



		::optix::prime::Model model = adapter->getScene();
		RTPquery query;

		CHK_PRIME(rtpQueryCreate(model->getRTPmodel(), RTP_QUERY_TYPE_CLOSEST, &query));

		RTPbufferdesc rays;
		CHK_PRIME(rtpBufferDescCreate(
				OptixContext::singleton()->context()->getRTPcontext(),
				RTP_BUFFER_FORMAT_RAY_ORIGIN_TMIN_DIRECTION_TMAX,
				RTP_BUFFER_TYPE_CUDA_LINEAR, cudaGvtCtx.traceRays, &rays));

		CHK_PRIME(rtpBufferDescSetRange(rays, 0, cudaGvtCtx.rayCount));

		CHK_PRIME(rtpQuerySetRays(query, rays));

		RTPbufferdesc hits;
		CHK_PRIME(rtpBufferDescCreate(
				OptixContext::singleton()->context()->getRTPcontext(),
				 RTP_BUFFER_FORMAT_HIT_T_TRIID_INSTID_U_V, RTP_BUFFER_TYPE_CUDA_LINEAR,
				cudaGvtCtx.traceHits, &hits));
		CHK_PRIME(rtpBufferDescSetRange(hits, 0, cudaGvtCtx.rayCount));

		CHK_PRIME(rtpQuerySetHits(query, hits));

		CHK_PRIME(rtpQuerySetCudaStream(query, cudaGvtCtx.stream));
		CHK_PRIME(rtpQueryExecute(query, RTP_QUERY_HINT_ASYNC));
		//rtpQueryFinish(query);

	}

	void operator()() {
#ifdef GVT_USE_DEBUG
		boost::timer::auto_cpu_timer t_functor(
				"OptixMeshAdapter: thread trace time: %w\n");
#endif


		int thread = begin > 0 ? 1 : 0;

		int localEnd = (end - begin);
		size_t localDispatchSize = 0;
		localDispatch.reserve((end - begin) * 2);

		gvt::render::data::cuda_primitives::CudaGvtContext& cudaGvtCtx =
				*(OptixContext::singleton()->_cudaGvtCtx[thread]);


		cudaGvtCtx.unique = adapter->unique;
		cudaGvtCtx.mesh = adapter->_inst2mesh_dev;
		cudaGvtCtx.normi = adapter->normi_dev;

		cudaGvtCtx.dispatchCount = 0;
		cudaGvtCtx.dirty=true;


		for (size_t localIdx = 0; localIdx < localEnd; localIdx += packetSize) {

			const size_t localPacketSize =
					(localIdx + packetSize > localEnd) ?
							(localEnd - localIdx) : packetSize;

			gpuErrchk(
					cudaMemsetAsync(cudaGvtCtx.valid, 1, sizeof(bool) * packetSize, cudaGvtCtx.stream));

			cudaGvtCtx.rayCount = localPacketSize;
			gvt::render::actor::RayVector::iterator localRayList = rayList
					+ begin + localIdx;


			cudaSetRays(localRayList, localPacketSize, cudaGvtCtx.rays,
					localIdx, cudaGvtCtx.stream, cudaRaysBuff);


			cudaGvtCtx.validRayLeft = true;
			cudaGvtCtx.dirty=true;

			while (cudaGvtCtx.validRayLeft) {

				cudaGvtCtx.validRayLeft = false;
				cudaGvtCtx.shadowRayCount = 0;

				cudaGvtCtx.dirty=true;

				traceRays(cudaGvtCtx);

				shade(&cudaGvtCtx);

				gpuErrchk(cudaStreamSynchronize(cudaGvtCtx.stream)); //wait for shadow ray count

				traceShadowRays(cudaGvtCtx);

				gpuErrchk(cudaStreamSynchronize(cudaGvtCtx.stream)); //wait for validRayLeft

//				if (cudaGvtCtx.validRayLeft)
//					printf("Valid Rays left..\n");

			}


			cudaGetRays(localDispatchSize, cudaGvtCtx, disp_Buff, localDispatch,
					rayList);

		}

		// copy localDispatch rays to outgoing rays queue
		std::unique_lock < std::mutex > moved(adapter->_outqueue);

		for (int i = 0; i < localDispatchSize; i++) {
			moved_rays.push_back(localDispatch[i]);
		}

		moved.unlock();


	}
};

void OptixMeshAdapter::trace(gvt::render::actor::RayVector &rayList,
		gvt::render::actor::RayVector &moved_rays, /*glm::mat4 *m,
        glm::mat4 *minv, glm::mat3 *normi, std::vector<gvt::render::data::scene::Light *> &lights,*/
        size_t _begin, size_t _end) {
#ifdef GVT_USE_DEBUG
	boost::timer::auto_cpu_timer t_functor("OptixMeshAdapter: trace time: %w\n");
#endif

	if (_end == 0)
		_end = rayList.size();

	this->begin = _begin;
	this->end = _end;

	size_t localWork = end-begin;

	// pull out instance transform data
	GVT_DEBUG(DBG_ALWAYS, "OptixMeshAdapter: getting instance transform data");

	gpuErrchk(cudaDeviceSynchronize());

	tbb::task_group _tasks;
	bool parallel = true;

	_tasks.run(
			[&]() {
				gvt::render::actor::RayVector::iterator localRayList = rayList.begin()+ _begin;
				size_t begin=0;
				size_t end=(parallel && localWork >= 2* packetSize) ? (localWork/2) : localWork;

				OptixParallelTrace(this, localRayList, moved_rays, /* m,
						minv, normi,*/ counter, begin, end, disp_Buff[0], cudaRaysBuff[0])();

			});

	if (parallel && localWork >= 2 * packetSize) {

		_tasks.run(
				[&]() {
					gvt::render::actor::RayVector::iterator localRayList = rayList.begin() + _begin ;
					size_t begin= localWork / 2;
					size_t end=localWork;

					OptixParallelTrace(this, localRayList, moved_rays, /*m,
							minv, normi, */counter, begin, end, disp_Buff[1], cudaRaysBuff[1])();

				});
	}

	_tasks.wait();

	GVT_DEBUG(DBG_ALWAYS,
			"OptixMeshAdapter: Forwarding rays: " << moved_rays.size());

}
