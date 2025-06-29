#pragma once

#include "BVH.hpp"
#include "Intersection.hpp"
#include "Material.hpp"
#include "OBJ_Loader.hpp"
#include "Object.hpp"
#include "Triangle.hpp"
#include <cassert>
#include <array>

bool rayTriangleIntersect(const Vector3f &v0, const Vector3f &v1, const Vector3f &v2, const Vector3f &orig,
                          const Vector3f &dir, float &tnear, float &u, float &v)
{
    // tNear contains the distance to the cloesest intersected object.   即交点的t
    // uv stores the u and v barycentric coordinates of the intersected point     即 交点的重心坐标? (b1,b2)
    // TODO: Implement this function that tests whether the triangle
    // that's specified bt v0, v1 and v2 intersects with the ray (whose
    // origin is *orig* and direction is *dir*)
    // Also don't forget to update tnear, u and v.

    Vector3f S = orig - v0;
    Vector3f E1 = v1 - v0;
    Vector3f E2 = v2 - v0;
    Vector3f S1 = crossProduct(dir, E2);
    Vector3f S2 = crossProduct(S, E1);

    float S1E1 = dotProduct(S1, E1);
    float t = dotProduct(S2, E2) / S1E1;
    float b1 = dotProduct(S1, S) / S1E1;
    float b2 = dotProduct(S2, dir) / S1E1;
    if (t > 0 && b1 >= 0 && b2 >= 0 && b1 <= 1 && b2 <= 1 && b1 + b2 <= 1)
    {
        tnear = t;
        u = b1;
        v = b2;
        return true;
    }
    return false;
}

class Triangle : public Object
{
public:
    Vector3f v0, v1, v2; // vertices A, B ,C , counter-clockwise order
    Vector3f e1, e2;     // 2 edges v1-v0, v2-v0;
    Vector3f t0, t1, t2; // texture coords
    Vector3f normal;
    Material *m;

    Triangle(Vector3f _v0, Vector3f _v1, Vector3f _v2, Material *_m = nullptr)
        : v0(_v0), v1(_v1), v2(_v2), m(_m)
    {
        e1 = v1 - v0;
        e2 = v2 - v0;
        normal = normalize(crossProduct(e1, e2));
    }

    bool intersect(const Ray &ray) override;
    bool intersect(const Ray &ray, float &tnear,
                   uint32_t &index) const override;
    Intersection getIntersection(Ray ray) override;
    void getSurfaceProperties(const Vector3f &P, const Vector3f &I,
                              const uint32_t &index, const Vector2f &uv,
                              Vector3f &N, Vector2f &st) const override
    {
        N = normal;
        //        throw std::runtime_error("triangle::getSurfaceProperties not
        //        implemented.");
    }
    Vector3f evalDiffuseColor(const Vector2f &) const override;
    Bounds3 getBounds() override;
};

class MeshTriangle : public Object
{
public:
    MeshTriangle(const std::string &filename)
    {
        objl::Loader loader;
        loader.LoadFile(filename);

        assert(loader.LoadedMeshes.size() == 1);
        auto mesh = loader.LoadedMeshes[0];

        Vector3f min_vert = Vector3f{std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::infinity()};
        Vector3f max_vert = Vector3f{-std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity()};
        for (int i = 0; i < mesh.Vertices.size(); i += 3)
        {
            std::array<Vector3f, 3> face_vertices;
            for (int j = 0; j < 3; j++)
            {
                auto vert = Vector3f(mesh.Vertices[i + j].Position.X,
                                     mesh.Vertices[i + j].Position.Y,
                                     mesh.Vertices[i + j].Position.Z) *
                            60.f;
                face_vertices[j] = vert;

                min_vert = Vector3f(std::min(min_vert.x, vert.x),
                                    std::min(min_vert.y, vert.y),
                                    std::min(min_vert.z, vert.z));
                max_vert = Vector3f(std::max(max_vert.x, vert.x),
                                    std::max(max_vert.y, vert.y),
                                    std::max(max_vert.z, vert.z));
            }

            auto new_mat =
                new Material(MaterialType::DIFFUSE_AND_GLOSSY,
                             Vector3f(0.5, 0.5, 0.5), Vector3f(0, 0, 0));
            new_mat->Kd = 0.6;
            new_mat->Ks = 0.0;
            new_mat->specularExponent = 0;

            triangles.emplace_back(face_vertices[0], face_vertices[1],
                                   face_vertices[2], new_mat);
        }

        bounding_box = Bounds3(min_vert, max_vert);

        std::vector<Object *> ptrs;
        for (auto &tri : triangles)
            ptrs.push_back(&tri);

        bvh = new BVHAccel(ptrs);
    }

    bool intersect(const Ray &ray) { return true; }

    bool intersect(const Ray &ray, float &tnear, uint32_t &index) const
    {
        bool intersect = false;
        for (uint32_t k = 0; k < numTriangles; ++k)
        {
            const Vector3f &v0 = vertices[vertexIndex[k * 3]];
            const Vector3f &v1 = vertices[vertexIndex[k * 3 + 1]];
            const Vector3f &v2 = vertices[vertexIndex[k * 3 + 2]];
            float t, u, v;
            if (rayTriangleIntersect(v0, v1, v2, ray.origin, ray.direction, t,
                                     u, v) &&
                t < tnear)
            {
                tnear = t;
                index = k;
                intersect |= true;
            }
        }

        return intersect;
    }

    Bounds3 getBounds() { return bounding_box; }

    void getSurfaceProperties(const Vector3f &P, const Vector3f &I,
                              const uint32_t &index, const Vector2f &uv,
                              Vector3f &N, Vector2f &st) const
    {
        const Vector3f &v0 = vertices[vertexIndex[index * 3]];
        const Vector3f &v1 = vertices[vertexIndex[index * 3 + 1]];
        const Vector3f &v2 = vertices[vertexIndex[index * 3 + 2]];
        Vector3f e0 = normalize(v1 - v0);
        Vector3f e1 = normalize(v2 - v1);
        N = normalize(crossProduct(e0, e1));
        const Vector2f &st0 = stCoordinates[vertexIndex[index * 3]];
        const Vector2f &st1 = stCoordinates[vertexIndex[index * 3 + 1]];
        const Vector2f &st2 = stCoordinates[vertexIndex[index * 3 + 2]];
        st = st0 * (1 - uv.x - uv.y) + st1 * uv.x + st2 * uv.y;
    }

    Vector3f evalDiffuseColor(const Vector2f &st) const
    {
        float scale = 5;
        float pattern =
            (fmodf(st.x * scale, 1) > 0.5) ^ (fmodf(st.y * scale, 1) > 0.5);
        return lerp(Vector3f(0.815, 0.235, 0.031),
                    Vector3f(0.937, 0.937, 0.231), pattern);
    }

    Intersection getIntersection(Ray ray)
    {
        Intersection intersec;

        if (bvh)
        {
            intersec = bvh->Intersect(ray);
        }

        return intersec;
    }

    Bounds3 bounding_box;
    std::unique_ptr<Vector3f[]> vertices;
    uint32_t numTriangles;
    std::unique_ptr<uint32_t[]> vertexIndex;
    std::unique_ptr<Vector2f[]> stCoordinates;

    std::vector<Triangle> triangles;

    BVHAccel *bvh;

    Material *m;
};

inline bool Triangle::intersect(const Ray &ray) { return true; }
inline bool Triangle::intersect(const Ray &ray, float &tnear,
                                uint32_t &index) const
{
    return false;
}

inline Bounds3 Triangle::getBounds() { return Union(Bounds3(v0, v1), v2); }

bool rayTriangleIntersect(const Vector3f &v0, const Vector3f &v1, const Vector3f &v2, const Vector3f &orig,
                          const Vector3f &dir, double &tnear, double &u, double &v)
{
    // tNear contains the distance to the cloesest intersected object.   即交点的t
    // uv stores the u and v barycentric coordinates of the intersected point     即 交点的重心坐标? (b1,b2)
    // TODO: Implement this function that tests whether the triangle
    // that's specified bt v0, v1 and v2 intersects with the ray (whose
    // origin is *orig* and direction is *dir*)
    // Also don't forget to update tnear, u and v.

    Vector3f S = orig - v0;
    Vector3f E1 = v1 - v0;
    Vector3f E2 = v2 - v0;
    Vector3f S1 = crossProduct(dir, E2);
    Vector3f S2 = crossProduct(S, E1);

    double S1E1 = dotProduct(S1, E1);
    double t = dotProduct(S2, E2) / S1E1;
    double b1 = dotProduct(S1, S) / S1E1;
    double b2 = dotProduct(S2, dir) / S1E1;
    if (t > 0 && b1 >= 0 && b2 >= 0 && b1 <= 1 && b2 <= 1 && b1 + b2 <= 1)
    {
        tnear = t;
        u = b1;
        v = b2;
        return true;
    }
    return false;
}

inline Intersection Triangle::getIntersection(Ray ray)
{
    Intersection inter;

    if (dotProduct(ray.direction, normal) > 0)
        return inter;
    double u, v, t_tmp = 0;
    Vector3f pvec = crossProduct(ray.direction, e2);
    double det = dotProduct(e1, pvec);
    if (fabs(det) < EPSILON)
        return inter;

    double det_inv = 1. / det;
    Vector3f tvec = ray.origin - v0;
    u = dotProduct(tvec, pvec) * det_inv;
    if (u < 0 || u > 1)
        return inter;
    Vector3f qvec = crossProduct(tvec, e1);
    v = dotProduct(ray.direction, qvec) * det_inv;
    if (v < 0 || u + v > 1)
        return inter;
    t_tmp = dotProduct(e2, qvec) * det_inv;

    inter.coords = ray(t_tmp);
    inter.distance = t_tmp;
    inter.m = this->m;
    inter.obj = this;
    inter.normal = this->normal;
    inter.happened = true;

    return inter;
}

inline Vector3f Triangle::evalDiffuseColor(const Vector2f &) const
{
    return Vector3f(0.5, 0.5, 0.5);
}
