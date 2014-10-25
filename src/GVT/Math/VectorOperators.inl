/*
 * File:   VectorOperators.inl
 * Author: jbarbosa
 *
 * Created on May 3, 2014, 1:01 PM
 */

template <class T>
T operator*(const GVT::Math::Vector<T>& a, const GVT::Math::Vector<T>& b) {
#ifdef _DEBUG
  if (a.numElements != b.numElements) throw VectortorSizeMismatch();
#endif

  double result = 0.0;

  for (int i = 0; i < a.numElements; i++) result += a[i] * b[i];

  return result;
}

template <class T>
GVT::Math::Vector<T> operator-(const GVT::Math::Vector<T>& v) {
  GVT::Math::Vector<T> result(v.numElements, false);
  for (int i = 0; i < v.numElements; i++) result[i] = -v[i];
  return result;
}

template <class T>
GVT::Math::Vector<T> operator*(const GVT::Math::Vector<T>& a, const double d) {
  GVT::Math::Vector<T> result(a.numElements, false);

  for (int i = 0; i < a.numElements; i++) result[i] = a[i] * d;

  return result;
}

template <class T>
GVT::Math::Vector<T> operator*(const double d, const GVT::Math::Vector<T>& a) {
  GVT::Math::Vector<T> result(a.numElements, false);

  for (int i = 0; i < a.numElements; i++) result[i] = a[i] * d;

  return result;
}

template <class T>
GVT::Math::Vector<T> operator/(const GVT::Math::Vector<T>& a, const double d) {
  GVT::Math::Vector<T> result(a.numElements, false);

  for (int i = 0; i < a.numElements; i++) result[i] = a[i] / d;

  return result;
}

template <class T>
GVT::Math::Vector<T> operator^(const GVT::Math::Vector<T>& a,
                               const GVT::Math::Vector<T>& b) {
#ifdef _DEBUG
  if (a.numElements != b.numElements) throw VectortorSizeMismatch();
#endif

  return a;
}

template <class T>
bool operator==(const GVT::Math::Vector<T>& a, const GVT::Math::Vector<T>& b) {
#ifdef _DEBUG
  if (a.numElements != b.numElements) throw VectortorSizeMismatch();
#endif

  for (int i = 0; i < a.numElements; i++)
    if (a[i] != b[i]) return false;

  return true;
}

template <class T>
bool operator!=(const GVT::Math::Vector<T>& a, const GVT::Math::Vector<T>& b) {
#ifdef _DEBUG
  if (a.numElements != b.numElements) throw VectortorSizeMismatch();
#endif

  for (int i = 0; i < a.numElements; i++)
    if (a[i] == b[i]) return false;

  return true;
}

template <class T>
std::ostream& operator<<(std::ostream& os, const GVT::Math::Vector<T>& a) {
  os << a.numElements;

  for (int i = 0; i < a.numElements; i++) os << " " << a[i];

  return os;
}

template <class T>
std::istream& operator>>(std::istream& is, const GVT::Math::Vector<T>& a) {
  return is;
}

//==========[ Inline Method Definitions (Vectortors) ]========

template <class T>
inline T operator*(const GVT::Math::Vector3<T>& a,
                   const GVT::Math::Vector4<T>& b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + b[3];
}

template <class T>
inline T operator*(const GVT::Math::Vector4<T>& b,
                   const GVT::Math::Vector3<T>& a) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + b[3];
}

template <class T>
inline GVT::Math::Vector3<T> operator-(const GVT::Math::Vector3<T>& v) {
  return GVT::Math::Vector3<T>(-v[0], -v[1], -v[2]);
}

template <class T>
inline GVT::Math::Vector3<T> operator*(const GVT::Math::Vector3<T>& a,
                                       const double d) {
  return GVT::Math::Vector3<T>(a[0] * d, a[1] * d, a[2] * d);
}

template <class T>
inline GVT::Math::Vector3<T> operator*(const double d,
                                       const GVT::Math::Vector3<T>& a) {
  return a * d;
}

template <class T>
inline GVT::Math::Vector3<T> operator*(
    const GVT::Math::AffineTransformMatrix<T>& a,
    const GVT::Math::Vector3<T>& v) {
  return GVT::Math::Vector3<T>(
      a.n[0] * v[0] + a.n[1] * v[1] + a.n[2] * v[2] + a.n[3],
      a.n[4] * v[0] + a.n[5] * v[1] + a.n[6] * v[2] + a.n[7],
      a.n[8] * v[0] + a.n[9] * v[1] + a.n[10] * v[2] + a.n[11]);
}

template <class T>
inline GVT::Math::Vector3<T> operator*(const GVT::Math::Vector3<T>& v,
                                       GVT::Math::AffineTransformMatrix<T>& a) {
  return a.transpose() * v;
}

template <class T>
inline T operator*(const GVT::Math::Vector3<T>& a,
                   const GVT::Math::Vector3<T>& b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

template <class T>
inline GVT::Math::Vector3<T> operator*(const GVT::Math::Matrix3<T>& a,
                                       const GVT::Math::Vector3<T>& v) {
  return GVT::Math::Vector3<T>(a.n[0] * v[0] + a.n[1] * v[1] + a.n[2] * v[2],
                               a.n[3] * v[0] + a.n[4] * v[1] + a.n[5] * v[2],
                               a.n[6] * v[0] + a.n[7] * v[1] + a.n[8] * v[2]);
}

template <class T>
inline GVT::Math::Vector3<T> operator*(const GVT::Math::Vector3<T>& v,
                                       const GVT::Math::Matrix3<T>& a) {
  return a.transpose() * v;
}

template <class T>
inline GVT::Math::Vector3<T> operator/(const GVT::Math::Vector3<T>& a,
                                       const double d) {
  return GVT::Math::Vector3<T>(a[0] / d, a[1] / d, a[2] / d);
}

template <class T>
inline GVT::Math::Vector3<T> operator^(const GVT::Math::Vector3<T>& a,
                                       const GVT::Math::Vector3<T>& b) {
  return GVT::Math::Vector3<T>(a[1] * b[2] - a[2] * b[1],
                               a[2] * b[0] - a[0] * b[2],
                               a[0] * b[1] - a[1] * b[0]);
}

template <class T>
inline bool operator==(const GVT::Math::Vector3<T>& a,
                       const GVT::Math::Vector3<T>& b) {
  return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

template <class T>
inline bool operator!=(const GVT::Math::Vector3<T>& a,
                       const GVT::Math::Vector3<T>& b) {
  return !(a == b);
}

template <class T>
inline std::ostream& operator<<(std::ostream& os,
                                const GVT::Math::Vector3<T>& v) {
  return os << v[0] << " " << v[1] << " " << v[2];
}

template <class T>
inline std::istream& operator>>(std::istream& is, GVT::Math::Vector3<T>& v) {
  return is >> v[0] >> v[1] >> v[2];
}

template <class T>
inline GVT::Math::Vector4<T> operator-(const GVT::Math::Vector4<T>& v) {
  return GVT::Math::Vector4<T>(-v[0], -v[1], -v[2], -v[3]);
}

template <class T>
inline GVT::Math::Vector4<T> operator*(const GVT::Math::Vector4<T>& a,
                                       const double d) {
  return GVT::Math::Vector4<T>(a[0] * d, a[1] * d, a[2] * d, a[3] * d);
}

template <class T>
inline GVT::Math::Vector4<T> operator*(const double d,
                                       const GVT::Math::Vector4<T>& a) {
  return a * d;
}

template <class T>
inline T operator*(const GVT::Math::Vector4<T>& a,
                   const GVT::Math::Vector4<T>& b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

template <class T>
inline GVT::Math::Vector4<T> operator*(
    const GVT::Math::AffineTransformMatrix<T>& a,
    const GVT::Math::Vector4<T>& v) {
  return GVT::Math::Vector4<T>(
      a.n[0] * v[0] + a.n[1] * v[1] + a.n[2] * v[2] + a.n[3] * v[3],
      a.n[4] * v[0] + a.n[5] * v[1] + a.n[6] * v[2] + a.n[7] * v[3],
      a.n[8] * v[0] + a.n[9] * v[1] + a.n[10] * v[2] + a.n[11] * v[3],
      a.n[12] * v[0] + a.n[13] * v[1] + a.n[14] * v[2] + a.n[15] * v[3]);
}

template <class T>
inline GVT::Math::Vector4<T> operator*(const GVT::Math::Vector4<T>& v,
                                       GVT::Math::AffineTransformMatrix<T>& a) {
  return a.transpose() * v;
}

template <class T>
inline GVT::Math::Vector4<T> operator/(const GVT::Math::Vector4<T>& a,
                                       const double d) {
  return GVT::Math::Vector4<T>(a[0] / d, a[1] / d, a[2] / d, a[3] / d);
}

template <class T>
inline bool operator==(const GVT::Math::Vector4<T>& a,
                       const GVT::Math::Vector4<T>& b) {
  return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

template <class T>
inline bool operator!=(const GVT::Math::Vector4<T>& a,
                       const GVT::Math::Vector4<T>& b) {
  return !(a == b);
}

template <class T>
inline std::ostream& operator<<(std::ostream& os,
                                const GVT::Math::Vector4<T>& v) {
  return os << v[0] << " " << v[1] << " " << v[2] << " " << v[3];
}

template <class T>
inline std::istream& operator>>(std::istream& is, GVT::Math::Vector4<T>& v) {
  return is >> v[0] >> v[1] >> v[2] >> v[3];
}

// template <class T>
// inline std::ostream& operator <<(std::ostream& os, const
// GVT::Math::Vector3<T>& v) {
//    os << v.n[0] << " " << v.n[1] << " " << v.n[2];
//}
//
// template <class T>
// inline std::istream& operator >>(std::istream& is, GVT::Math::Vector3<T>& v)
// {
//    is >> v.n[0] >> v.n[1] >> v.n[2];
//}
template <class T>
inline std::ostream& operator<<(std::ostream& os,
                                const GVT::Math::Point4<T>& v) {
  return os << v.n[0] << " " << v.n[1] << " " << v.n[2] << " " << v.n[3];
}

template <class T>
inline std::istream& operator>>(std::istream& is, GVT::Math::Point4<T>& v) {
  return is >> v.n[0] >> v.n[1] >> v.n[2] >> v.n[3];
}

