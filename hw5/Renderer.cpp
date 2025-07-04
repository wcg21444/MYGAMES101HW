#define _CRT_SECURE_NO_WARNINGS

#include <fstream>
#include "Vector.hpp"
#include "Renderer.hpp"
#include "Scene.hpp"
#include <optional>
#include <future>
#include <thread>

inline float deg2rad(const float &deg)
{
    return deg * M_PI / 180.0;
}

// Compute reflection direction
Vector3f reflect(const Vector3f &I, const Vector3f &N)
{
    return I - 2 * dotProduct(I, N) * N;
}

// [comment]
// Compute refraction direction using Snell's law
//
// We need to handle with care the two possible situations:
//
//    - When the ray is inside the object
//
//    - When the ray is outside.
//
// If the ray is outside, you need to make cosi positive cosi = -N.I
//
// If the ray is inside, you need to invert the refractive indices and negate the normal N
// [/comment]
Vector3f refract(const Vector3f &I, const Vector3f &N, const float &ior)
{
    float cosi = clamp(-1, 1, dotProduct(I, N));
    float etai = 1, etat = ior;
    Vector3f n = N;
    if (cosi < 0)
    {
        cosi = -cosi;
    }
    else
    {
        std::swap(etai, etat);
        n = -N;
    }
    float eta = etai / etat;
    float k = 1 - eta * eta * (1 - cosi * cosi);
    return k < 0 ? 0 : eta * I + (eta * cosi - sqrtf(k)) * n;
}

// [comment]
// Compute Fresnel equation
//
// \param I is the incident view direction
//
// \param N is the normal at the intersection point
//
// \param ior is the material refractive index
// [/comment]
float fresnel(const Vector3f &I, const Vector3f &N, const float &ior)
{
    float cosi = clamp(-1, 1, dotProduct(I, N));
    float etai = 1, etat = ior;
    if (cosi > 0)
    {
        std::swap(etai, etat);
    }
    // Compute sini using Snell's law
    float sint = etai / etat * sqrtf(std::max(0.f, 1 - cosi * cosi));
    // Total internal reflection
    if (sint >= 1)
    {
        return 1;
    }
    else
    {
        float cost = sqrtf(std::max(0.f, 1 - sint * sint));
        cosi = fabsf(cosi);
        float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
        float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
        return (Rs * Rs + Rp * Rp) / 2;
    }
    // As a consequence of the conservation of energy, transmittance is given by:
    // kt = 1 - kr;
}

// [comment]
// Returns true if the ray intersects an object, false otherwise.
//
// \param orig is the ray origin
// \param dir is the ray direction
// \param objects is the list of objects the scene contains
// \param[out] tNear contains the distance to the cloesest intersected object.
// \param[out] index stores the index of the intersect triangle if the interesected object is a mesh.
// \param[out] uv stores the u and v barycentric coordinates of the intersected point
// \param[out] *hitObject stores the pointer to the intersected object (used to retrieve material information, etc.)
// \param isShadowRay is it a shadow ray. We can return from the function sooner as soon as we have found a hit.
// [/comment]
std::optional<hit_payload> trace(
    const Vector3f &orig, const Vector3f &dir,
    const std::vector<std::unique_ptr<Object>> &objects)
{
    float tNear = kInfinity;
    std::optional<hit_payload> payload;
    for (const auto &object : objects)
    {
        float tNearK = kInfinity;
        uint32_t indexK;
        Vector2f uvK;
        if (object->intersect(orig, dir, tNearK, indexK, uvK) && tNearK < tNear)
        {
            payload.emplace();
            payload->hit_obj = object.get();
            payload->tNear = tNearK;
            payload->index = indexK;
            payload->uv = uvK;
            tNear = tNearK;
        }
    }

    return payload;
}

// [comment]
// Implementation of the Whitted-style light transport algorithm (E [S*] (D|G) L)
//
// This function is the function that compute the color at the intersection point
// of a ray defined by a position and a direction. Note that thus function is recursive (it calls itself).
//
// If the material of the intersected object is either reflective or reflective and refractive,
// then we compute the reflection/refraction direction and cast two new rays into the scene
// by calling the castRay() function recursively. When the surface is transparent, we mix
// the reflection and refraction color using the result of the fresnel equations (it computes
// the amount of reflection and refraction depending on the surface normal, incident view direction
// and surface refractive index).
//
// If the surface is diffuse/glossy we use the Phong illumation model to compute the color
// at the intersection point.
// [/comment]
Vector3f castRay(
    const Vector3f &orig, const Vector3f &dir, const Scene &scene,
    int depth)
{
    if (depth > scene.maxDepth)
    {
        return Vector3f(0.0, 0.0, 0.0);
    }

    Vector3f hitColor = scene.backgroundColor;
    if (auto payload = trace(orig, dir, scene.get_objects()); payload)
    {
        Vector3f hitPoint = orig + dir * payload->tNear;
        Vector3f N;  // normal
        Vector2f st; // st coordinates
        payload->hit_obj->getSurfaceProperties(hitPoint, dir, payload->index, payload->uv, N, st);
        switch (payload->hit_obj->materialType)
        {
        case REFLECTION_AND_REFRACTION:
        {
            Vector3f reflectionDirection = normalize(reflect(dir, N));
            Vector3f refractionDirection = normalize(refract(dir, N, payload->hit_obj->ior));
            Vector3f reflectionRayOrig = (dotProduct(reflectionDirection, N) < 0) ? hitPoint - N * scene.epsilon : hitPoint + N * scene.epsilon;
            Vector3f refractionRayOrig = (dotProduct(refractionDirection, N) < 0) ? hitPoint - N * scene.epsilon : hitPoint + N * scene.epsilon;
            Vector3f reflectionColor = castRay(reflectionRayOrig, reflectionDirection, scene, depth + 1);
            Vector3f refractionColor = castRay(refractionRayOrig, refractionDirection, scene, depth + 1);
            float kr = fresnel(dir, N, payload->hit_obj->ior);
            hitColor = reflectionColor * kr + refractionColor * (1 - kr);
            break;
        }
        case REFLECTION:
        {
            float kr = fresnel(dir, N, payload->hit_obj->ior);
            Vector3f reflectionDirection = reflect(dir, N);
            Vector3f reflectionRayOrig = (dotProduct(reflectionDirection, N) < 0) ? hitPoint + N * scene.epsilon : hitPoint - N * scene.epsilon;
            hitColor = castRay(reflectionRayOrig, reflectionDirection, scene, depth + 1) * kr;
            break;
        }
        default:
        {
            // [comment]
            // We use the Phong illumation model int the default case. The phong model
            // is composed of a diffuse and a specular reflection component.
            // [/comment]
            Vector3f lightAmt = 0, specularColor = 0;
            Vector3f shadowPointOrig = (dotProduct(dir, N) < 0) ? hitPoint + N * scene.epsilon : hitPoint - N * scene.epsilon;
            // [comment]
            // Loop over all lights in the scene and sum their contribution up
            // We also apply the lambert cosine law
            // [/comment]
            for (auto &light : scene.get_lights())
            {
                Vector3f lightDir = light->position - hitPoint;
                // square of the distance between hitPoint and the light
                float lightDistance2 = dotProduct(lightDir, lightDir);
                lightDir = normalize(lightDir);
                float LdotN = std::max(0.f, dotProduct(lightDir, N));
                // is the point in shadow, and is the nearest occluding object closer to the object than the light itself?
                auto shadow_res = trace(shadowPointOrig, lightDir, scene.get_objects());
                bool inShadow = shadow_res && (shadow_res->tNear * shadow_res->tNear < lightDistance2);

                lightAmt += inShadow ? 0 : light->intensity * LdotN;
                Vector3f reflectionDirection = reflect(-lightDir, N);

                specularColor += powf(std::max(0.f, -dotProduct(reflectionDirection, dir)),
                                      payload->hit_obj->specularExponent) *
                                 light->intensity;
            }

            hitColor = lightAmt * payload->hit_obj->evalDiffuseColor(st) * payload->hit_obj->Kd + specularColor * payload->hit_obj->Ks;
            break;
        }
        }
    }

    return hitColor;
}

std::vector<Vector3f> RenderKernel(const Scene &scene, Vector3f &eye_pos, const size_t n_row, const size_t begin_row, size_t t)
{
    uint64_t id = std::hash<std::thread::id>{}(std::this_thread::get_id());

    std::vector<Vector3f> framebuffer(scene.width * n_row);

    static const float scale = std::tan(deg2rad(scene.fov * 0.5f));
    static const float imageAspectRatio = scene.width / (float)scene.height; // static const 能否保证线程安全?
    int m = 0;
    for (int j = begin_row; j < (begin_row + n_row); ++j)
    {
        for (int i = 0; i < scene.width; ++i)
        {

            float x_ndc = 2.f * i / scene.width - 1.f;

            float y_ndc = 1.f - 2.f * j / scene.height;

            float x = x_ndc * scale * imageAspectRatio;
            float y = y_ndc * scale;

            Vector3f dir = Vector3f(x, y, -1); // Don't forget to normalize this direction!
            dir = normalize(dir);
            framebuffer[m++] = castRay(eye_pos, dir, scene, 0);
        }
        if (t == 0)
        {
            UpdateProgress(j / (float)n_row);
        }
    }
    if (t == 0)
    {
        std::cout << "\nWait for other threds\n";
    }

    return framebuffer;
}

// [comment]
// The main render function. This where we iterate over all pixels in the image, generate
// primary rays and cast these rays into the scene. The content of the framebuffer is
// saved to a file.
// [/comment]
/*
void Renderer::Render(const Scene &scene)
{
    std::vector<Vector3f> framebuffer(scene.width * scene.height);

    float scale = std::tan(deg2rad(scene.fov * 0.5f));
    float imageAspectRatio = scene.width / (float)scene.height;

    // Use this variable as the eye position to start your rays.
    // 可不可以改成多线程的RayTracing?
    Vector3f eye_pos(0);
    int m = 0;
    for (int j = 0; j < scene.height; ++j)
    {
        for (int i = 0; i < scene.width; ++i)
        {

            float x_ndc = 2.f * i / scene.width - 1.f;

            float y_ndc = 1.f - 2.f * j / scene.height;
            // 这里得到的ndc坐标 不需要z方向,因此不需要深度缓冲区

            // generate primary ray direction
            float x = x_ndc * scale * imageAspectRatio;
            float y = y_ndc * scale;

            // TODO: Find the x and y positions of the current pixel (i,j) to get the direction
            // vector that passes through it.
            // Also, don't forget to multiply both of them with the variable *scale*, and
            // x (horizontal) variable with the *imageAspectRatio*

            Vector3f dir = Vector3f(x, y, -1); // Don't forget to normalize this direction!
            dir = normalize(dir);
            framebuffer[m++] = castRay(eye_pos, dir, scene, 0);
        }
        UpdateProgress(j / (float)scene.height);
    }

    // save framebuffer to file
    FILE *fp = nullptr;
    fopen_s(&fp, "binary.ppm", "wb");
    (void)fprintf(fp, "P6\n%d %d\n255\n", scene.width, scene.height);
    for (auto i = 0; i < scene.height * scene.width; ++i)
    {
        static unsigned char color[3];
        color[0] = (char)(255 * clamp(0, 1, framebuffer[i].x));
        color[1] = (char)(255 * clamp(0, 1, framebuffer[i].y));
        color[2] = (char)(255 * clamp(0, 1, framebuffer[i].z));
        fwrite(color, 1, 3, fp);
    }
    fclose(fp);
}
*/

void Renderer::Render(const Scene &scene)
{
    Vector3f eye_pos(0);

    // MultiThread Render
    std::vector<Vector3f> final_framebuffer;
    const size_t n_thrd = 1;
    const size_t n_row = scene.height / n_thrd;

    std::vector<std::future<std::vector<Vector3f>>> futures_framebuffer;
    futures_framebuffer.reserve(n_thrd);
    for (size_t t = 0; t < n_thrd; t++)
    {
        futures_framebuffer.emplace_back(std::async(std::launch::async, [&scene, &eye_pos, n_row, t]() -> std::vector<Vector3f>
                                                    { return RenderKernel(scene, eye_pos, n_row, n_row * t, t); }));
    }
    std::atomic<int> completed{0};
    for (size_t t = 0; t < n_thrd; t++)
    {
        auto partial = futures_framebuffer[t].get();
        final_framebuffer.insert(
            final_framebuffer.end(),
            std::make_move_iterator(partial.begin()),
            std::make_move_iterator(partial.end()));

        ++completed;
        UpdateProgress(float(completed) / n_thrd);
    }

    // save final_framebuffer to file
    FILE *fp = nullptr;
    fopen_s(&fp, "binary.ppm", "wb");
    (void)fprintf(fp, "P6\n%d %d\n255\n", scene.width, scene.height);
    for (auto i = 0; i < scene.height * scene.width; ++i)
    {
        static unsigned char color[3];
        color[0] = (char)(255 * clamp(0, 1, final_framebuffer[i].x));
        color[1] = (char)(255 * clamp(0, 1, final_framebuffer[i].y));
        color[2] = (char)(255 * clamp(0, 1, final_framebuffer[i].z));
        fwrite(color, 1, 3, fp);
    }
    fclose(fp);
}
