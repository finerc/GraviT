/*
 * File:   cell.cpp
 * Author: jbarbosa
 *
 * Created on February 27, 2014, 12:06 PM
 */

#include <iostream>
#include <vector>
#include <gvt/render/data/domain/Cell.h>

#include <cstring> // memcpy
#include <cstdlib> //realloc

#ifdef __USE_TAU
#include <TAU.h>
#endif
using namespace std;
using namespace gvt::render::data::domain;

Cell::Cell(const Cell& other)
{
    memcpy(data, other.data, sizeof (float)*8);
    memcpy(min, other.min, sizeof (float)*3);
    memcpy(max, other.max, sizeof (float)*3);
}

Cell::~Cell() {}

bool Cell::MakeCell(int id, Cell& cell, vector<int>& dim, vector<float> min, vector<float> max, vector<float>& cell_dim, float* data)
{
#ifdef __USE_TAU
  TAU_START("Cell::MakeCell");
#endif
    if (id < 0 || id >= (dim[0] * dim[1] * dim[2]))
    {
        cerr << "ERROR: invalid cell id '" << id << "' passed to MakeCell" << endl;
#ifdef __USE_TAU
        TAU_STOP("Cell::MakeCell");
#endif

        return false;
    }

    cell.data[0] = data[id];
    cell.data[1] = data[id + 1];
    cell.data[2] = data[id + dim[0]];
    cell.data[3] = data[id + dim[0] + 1];
    cell.data[4] = data[id + dim[0] * dim[1]];
    cell.data[5] = data[id + dim[0] * dim[1] + 1];
    cell.data[6] = data[id + dim[0] * dim[1] + dim[0]];
    cell.data[7] = data[id + dim[0] * dim[1] + dim[0] + 1];

    int idx[3] = {id % dim[0], id / dim[0] % dim[1], id / (dim[0] * dim[1])};
    for (int i = 0; i < 3; ++i)
    {
        cell.min[i] = min[i] + idx[i] * cell_dim[i];
        cell.max[i] = min[i] + idx[i] * cell_dim[i] + cell_dim[i];
    }
#ifdef __USE_TAU
      TAU_START("Cell::MakeCell");
#endif

    return true;
}

bool Cell::FindFaceIntersectionsWithRay(gvt::render::actor::Ray& r, vector<Cell::Face>& faces)
{
#ifdef __USE_TAU
  TAU_START("Cell::FindFaceIntersectionsWithRay");
#endif

    float t[2] = {-FLT_MAX, FLT_MAX};
    int axis[2] = {-1, -1};
    bool tswap, swap[2];

    for (int i = 0; i < 3; ++i)
    {
        tswap = false;
        if (r.direction[i] == 0)
        {
            if ((r.origin[i] < min[i]) | (r.origin[i] > max[i])) return false;
        }
        else
        {
            float inv_d = 1.f / r.direction[i];
            float t1, t2;
            t1 = (min[i] - r.origin[i]) * inv_d;
            t2 = (max[i] - r.origin[i]) * inv_d;
            if (t1 > t2)
            {
                float temp = t1;
                t1 = t2;
                t2 = temp;
                tswap = true;
            }

            if (t1 > t[0])
            {
                t[0] = t1;
                axis[0] = i;
                swap[0] = tswap;
            }
            if (t2 < t[1])
            {
                t[1] = t2;
                axis[1] = i;
                swap[1] = tswap;
            }
            if (t[0] > t[1]) return false;
            if (t[1] < 0) return false;
        }
    }

    float inv_size[3] = {1.f / (max[0] - min[0]), 1.f / (max[1] - min[1]), 1.f / (max[2] - min[2])};
    for (int i = 0; i < 2; ++i)
    {
        switch (axis[i])
        {
            case 0:
            {
                float pt[2] = {((r.origin[1] + r.direction[1] * t[i]) - min[1]) * inv_size[1],
                    ((r.origin[2] + r.direction[2] * t[i]) - min[2]) * inv_size[2]};
                if (i == 0)
                    if (swap[i])
                        faces.push_back(Face(t[i], pt[0], pt[1], data[1], data[3], data[5], data[7]));
                    else
                        faces.push_back(Face(t[i], pt[0], pt[1], data[0], data[2], data[4], data[6]));
                else
                    if (swap[i])
                    faces.push_back(Face(t[i], pt[0], pt[1], data[0], data[2], data[4], data[6]));
                else
                    faces.push_back(Face(t[i], pt[0], pt[1], data[1], data[3], data[5], data[7]));
            }
                break;
            case 1:
            {
                float pt[2] = {((r.origin[0] + r.direction[0] * t[i]) - min[0]) * inv_size[0],
                    ((r.origin[2] + r.direction[2] * t[i]) - min[2]) * inv_size[2]};
                if (i == 0)
                    if (swap[i])
                        faces.push_back(Face(t[i], pt[0], pt[1], data[2], data[3], data[6], data[7]));
                    else
                        faces.push_back(Face(t[i], pt[0], pt[1], data[0], data[1], data[4], data[5]));
                else
                    if (swap[i])
                    faces.push_back(Face(t[i], pt[0], pt[1], data[0], data[1], data[4], data[5]));
                else
                    faces.push_back(Face(t[i], pt[0], pt[1], data[2], data[3], data[6], data[7]));
            }
                break;
            case 2:
            {
                float pt[2] = {((r.origin[0] + r.direction[0] * t[i]) - min[0]) * inv_size[0],
                    ((r.origin[1] + r.direction[1] * t[i]) - min[1]) * inv_size[1]};
                if (i == 0)
                    if (swap[i])
                        faces.push_back(Face(t[i], pt[0], pt[1], data[4], data[5], data[6], data[7]));
                    else
                        faces.push_back(Face(t[i], pt[0], pt[1], data[0], data[1], data[2], data[3]));
                else
                    if (swap[i])
                    faces.push_back(Face(t[i], pt[0], pt[1], data[0], data[1], data[2], data[3]));
                else
                    faces.push_back(Face(t[i], pt[0], pt[1], data[4], data[5], data[6], data[7]));
            }
                break;
            default:
                cerr << "ERROR: unexpected axis value '" << axis[i] << "' in FindFaceIntersectionsWithRay" << endl;
                return false;
        }
    }
#ifdef __USE_TAU
  TAU_STOP("Cell::FindFaceIntersectionsWithRay");
#endif

    return true;
}

namespace gvt{ namespace render{ namespace data{ namespace domain {
ostream&
operator<<(ostream& os, Cell::Face const& f)
{
    os << "t: << " << f.t << "  pt:[" << f.pt[0] << " " << f.pt[1] << "]  ";
    os << "data:[" << f.data[0] << " " << f.data[1] << " " << f.data[2] << " " << f.data[3] << "]";

    return os;
}
}}}} // namespace domain} namespace data} namespace render} namespace gvt}
