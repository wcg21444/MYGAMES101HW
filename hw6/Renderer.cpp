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

            framebuffer[m++] = scene.castRay(Ray(eye_pos, dir), 0);
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
    Vector3f eye_pos(-1, 5, 10);

    // MultiThread Render
    std::vector<Vector3f> final_framebuffer;
    const size_t n_thrd = 15;
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
