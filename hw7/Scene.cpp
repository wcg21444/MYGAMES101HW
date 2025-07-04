//
// Created by Göksu Güvendiren on 2019-05-14.
//

#include "Scene.hpp"

void Scene::buildBVH()
{
    printf(" - Generating BVH...\n\n");
    this->bvh = new BVHAccel(objects, 1, BVHAccel::SplitMethod::NAIVE);
}

//[in]: ray 光线
//[out]: ray 与scene的交点信息

Intersection Scene::intersect(const Ray &ray) const
{
    return this->bvh->Intersect(ray);
}

// [in]: scene
// [out]: pos, pdf
// 在场景的所有光源上按面积uniform 地 sample 一个点，并计算该 sample 的概率密度 也就是说 会平等地采样每一个点,点一定在某个光源上
void Scene::sampleLight(Intersection &pos, float &pdf) const
{
    float emit_area_sum = 0;
    for (uint32_t k = 0; k < objects.size(); ++k)
    {
        if (objects[k]->hasEmit())
        {
            emit_area_sum += objects[k]->getArea();
        }
    }
    float p = get_random_float() * emit_area_sum;
    emit_area_sum = 0;

    for (uint32_t k = 0; k < objects.size(); ++k)
    {
        if (objects[k]->hasEmit())
        {
            emit_area_sum += objects[k]->getArea();
            if (p <= emit_area_sum)
            {
                objects[k]->Sample(pos, pdf);
                break;
            }
        }
    }
}

bool Scene::trace(
    const Ray &ray,
    const std::vector<Object *> &objects,
    float &tNear, uint32_t &index, Object **hitObject)
{
    *hitObject = nullptr;
    for (uint32_t k = 0; k < objects.size(); ++k)
    {
        float tNearK = kInfinity;
        uint32_t indexK;
        Vector2f uvK;
        if (objects[k]->intersect(ray, tNearK, indexK) && tNearK < tNear)
        {
            *hitObject = objects[k];
            tNear = tNearK;
            index = indexK;
        }
    }

    return (*hitObject != nullptr);
}

// Implementation of Path Tracing
// [in]: ray发射光线,可以是多次反射的光线, depth
// [out]: shade color/本次反射光照
Vector3f Scene::castRay(const Ray &ray, int depth) const
{

    // ISSUE : 亮噪点很多
    // 如果次级光castRay直接打到光源,然后返回0 亮噪点少了, 但是间接光依然较暗  因为直接光没有作背面剔除导致出现负值直接光
    auto p = intersect(ray);
    if (!p.m)
        // return {backgroundColor};
        return {};

    if (p.m->hasEmission()) // 命中光源
    {
        if (depth > 0)
        {
            return {};
        }
        return p.m->getEmission();
    }

    auto wo = -ray.direction; // 着色点出射方向
    auto N = p.normal;        // 光线命中点的法向量

    Vector3f L_dir;
    Vector3f L_indir;

    // 计算直接光照

    auto inter_light = Intersection();
    float pdf_light = 0.f;
    sampleLight(inter_light, pdf_light);

    auto x = inter_light.coords;
    auto NN = inter_light.normal;
    auto emit = inter_light.emit;
    auto ws = normalize(x - p.coords);

    float cos_surface = dotProduct(ws, N);
    float cos_light = dotProduct(-ws, NN);
    auto geo_term = cos_surface * cos_light;
    if (geo_term - EPSILON > 0)
    {
        // 判断光源---采样点之间有无阻挡
        auto distance_squre = dotProduct(x - p.coords, x - p.coords);
        auto inter_test = intersect(Ray(p.coords, ws));
        auto delta = inter_test.coords - inter_light.coords;
        if (dotProduct(delta, delta) < EPSILON)
        {
            L_dir = emit * p.m->eval(ws, wo, N) * geo_term / distance_squre / pdf_light; // pdf_light, emit brdf >0
        }
    }

#define INDIRECT_LIGHT
#ifdef INDIRECT_LIGHT
    // 测试俄罗斯轮盘
    if (get_random_float() > RussianRoulette)
        return L_dir;
    auto wi = p.m->sample(wo, N); // p点随机接受光线的方向
    Ray r(p.coords, wi);
    // 如何得知r是否击中光源?
    float pdf_p = p.m->pdf(wo, wi, N);
    if (pdf_p == 0.f)
    {
        return L_dir;
    }
    L_indir = castRay(r, depth + 1) * p.m->eval(ws, wo, N) * dotProduct(wi, N) / pdf_p / RussianRoulette;
#endif
    return L_indir + L_dir;
    // TO DO Implement Path Tracing Algorithm here
    // 解决发射的光线能追踪到的光照着色
}