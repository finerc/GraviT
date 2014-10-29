/*
 * File:   gvt_mesh.h
 * Author: jbarbosa
 *
 * Created on April 20, 2014, 11:01 PM
 */

#ifndef GVT_MESH_H
#define GVT_MESH_H

#include <GVT/Data/primitives.h>
#include <GVT/Data/primitives/gvt_material.h>
#include <GVT/Data/primitives/gvt_material_list.h>
#include <boost/container/vector.hpp>
#include <boost/tuple/tuple.hpp>

namespace GVT {

namespace Data {

class AbstractMesh {
 public:
  AbstractMesh() {}
  AbstractMesh(const AbstractMesh&) {}
  ~AbstractMesh() {}

  virtual AbstractMesh* getMesh() { return this; }

  virtual GVT::Data::box3D* getBoundingBox() { return NULL; }
};

class Mesh : public AbstractMesh {
 public:
  typedef boost::tuple<int, int, int> face;
  typedef boost::tuple<int, int, int> face_to_normals;

  Mesh(GVT::Data::Material* mat = NULL);
  Mesh(const Mesh& orig);

  virtual ~Mesh();
  virtual void addVertex(GVT::Math::Point4f vertex,
                         GVT::Math::Vector4f normal = GVT::Math::Vector4f(),
                         GVT::Math::Point4f texUV = GVT::Math::Point4f());
  virtual void pushVertex(int which, GVT::Math::Point4f vertex,
                          GVT::Math::Vector4f normal = GVT::Math::Vector4f(),
                          GVT::Math::Point4f texUV = GVT::Math::Point4f());
  virtual void pushNormal(int which,
                          GVT::Math::Vector4f normal = GVT::Math::Point4f());
  virtual void pushTexUV(int which,
                         GVT::Math::Point4f texUV = GVT::Math::Point4f());
  virtual void setMaterial(GVT::Data::Material* mat);
  virtual void setMaterialList(GVT::Data::MaterialList* materials);
  virtual void addFace(int v0, int v1, int v2);
  virtual void setFaceMaterial(int face_id, const GVT::Data::Material* material);

  void generateNormals();

  virtual GVT::Data::Color shade(GVT::Data::ray& r, GVT::Math::Vector4f normal,
                                 GVT::Data::LightSource* lsource);
  virtual GVT::Data::Color shadeFace(int face_id, GVT::Data::ray& r,
                                 GVT::Math::Vector4f normal,
                                 GVT::Data::LightSource* lsource);

  GVT::Data::Material* mat;
  GVT::Data::MaterialList* material_list;

  boost::container::vector<GVT::Math::Vector4f> vertices;
  boost::container::vector<GVT::Math::Vector4f> mapuv;
  boost::container::vector<GVT::Math::Vector4f> normals;
  boost::container::vector<GVT::Data::Mesh::face> faces;
  boost::container::vector<GVT::Data::Mesh::face_to_normals> faces_to_normals;
  boost::container::vector<GVT::Math::Vector4f> face_normals;
  boost::container::vector<const Material*> faces_to_materials;

  GVT::Data::box3D boundingBox;
  bool haveNormals;
 private:
};

}  // namespace Data

}  // namespace GVT

#endif /* GVT_MESH_H */

