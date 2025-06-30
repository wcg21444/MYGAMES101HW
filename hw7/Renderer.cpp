//
// Created by goksu on 2/25/20.
//

#include <fstream>
#include "Scene.hpp"
#include "Renderer.hpp"

#include <future>
#include <thread>

inline float deg2rad(const float &deg) { return deg * M_PI / 180.0; }

const float EPSILON = 0.00001;

std::vector<Vector3f> RenderKernel(const Scene &scene, Vector3f &eye_pos, const size_t n_row, const size_t begin_row, int spp, size_t t)
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
            float x = (2 * (i + 0.5) / (float)scene.width - 1) *
                      imageAspectRatio * scale;
            float y = (1 - 2 * (j + 0.5) / (float)scene.height) * scale;
            Vector3f dir = normalize(Vector3f(-x, y, 1));
            dir = normalize(dir);
            for (int k = 0; k < spp; k++)
            {
                framebuffer[m] += scene.castRay(Ray(eye_pos, dir), 0) / (float)spp;
            }
            m++;
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

// The main render function. This where we iterate over all pixels in the image,
// generate primary rays and cast these rays into the scene. The content of the
// framebuffer is saved to a file.
void Renderer::Render(const Scene &scene)
{
    Vector3f eye_pos(278, 273, -800);

    // MultiThread Render
    std::vector<Vector3f> final_framebuffer;
    const size_t n_thrd = 16;
    const size_t n_row = scene.height / n_thrd;
    const int spp = 8;

    std::vector<std::future<std::vector<Vector3f>>> futures_framebuffer;
    futures_framebuffer.reserve(n_thrd);
    for (size_t t = 0; t < n_thrd; t++)
    {
        futures_framebuffer.emplace_back(std::async(std::launch::async, [&scene, &eye_pos, n_row, spp, t]() -> std::vector<Vector3f>
                                                    { return RenderKernel(scene, eye_pos, n_row, n_row * t, spp, t); }));
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

    // save framebuffer to file
    FILE *fp = fopen("binary.ppm", "wb");
    (void)fprintf(fp, "P6\n%d %d\n255\n", scene.width, scene.height);
    for (auto i = 0; i < scene.height * scene.width; ++i)
    {
        static unsigned char color[3];
        color[0] = (unsigned char)(255 * std::pow(clamp(0, 1, final_framebuffer[i].x), 0.6f));
        color[1] = (unsigned char)(255 * std::pow(clamp(0, 1, final_framebuffer[i].y), 0.6f));
        color[2] = (unsigned char)(255 * std::pow(clamp(0, 1, final_framebuffer[i].z), 0.6f));
        fwrite(color, 1, 3, fp);
    }
    fclose(fp);
}