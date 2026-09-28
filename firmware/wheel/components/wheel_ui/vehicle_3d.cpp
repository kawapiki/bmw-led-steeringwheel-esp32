#include "vehicle_3d.h"
#include "e90_mesh.h"
#include "tgx.h"
#include <cmath>
#include <cstdlib>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#endif
namespace {
constexpr unsigned W = 192, H = 104;
constexpr auto SH = tgx::SHADER_ORTHO | tgx::SHADER_ZBUFFER | tgx::SHADER_FLAT |
                    tgx::SHADER_NOTEXTURE;
tgx::Renderer3D<tgx::RGB565, SH | tgx::SHADER_UNLIT, uint16_t> renderer;
tgx::Image<tgx::RGB565> image;
lv_image_dsc_t descriptor;
uint16_t *pixels, *depth;
uint32_t generation;
uint64_t last_ms;
bool active, initialized;
float angles[7], yaw, elevation = 22;
uint64_t lamp_view_until;
float lamp_view_yaw;
uint32_t last_input = 0xffffffff;
bool moving = true;
constexpr float PI = 3.14159265358979323846f;
float rad(float d) { return d * PI / 180; }
float smooth(float x) {
  x = fmaxf(0, fminf(1, x));
  return x * x * (3 - 2 * x);
}
void *allocate(size_t n) {
#ifdef ESP_PLATFORM
  return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  return malloc(n);
#endif
}

} // namespace
extern "C" bool vehicle_3d_init(void) {
  if (initialized)
    return pixels && depth;
  initialized = true;
  pixels = (uint16_t *)allocate(W * H * 2);
  depth = (uint16_t *)allocate(W * H * 2);
  if (!pixels || !depth) {
    free(pixels);
    free(depth);
    pixels = depth = nullptr;
#ifdef ESP_PLATFORM
    ESP_LOGE("vehicle3d", "PSRAM buffers unavailable; vector fallback");
#endif
    return false;
  }
  image = tgx::Image<tgx::RGB565>((tgx::RGB565 *)pixels, W, H);
  renderer.setImage(&image);
  renderer.setZbuffer(depth);
  renderer.setViewportSize(W, H);
  renderer.setShaders(SH);
  renderer.setCulling(0);
  renderer.setOrtho(-3.25f, 3.25f, -1.7604f, 1.7604f, 1, 30);
  renderer.setLight(tgx::fVec3(-1, -2, -4), tgx::RGBf(.55f, .55f, .65f),
                    tgx::RGBf(.7f, .7f, .7f), tgx::RGBf(0, 0, 0));
  descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
  descriptor.header.cf = LV_COLOR_FORMAT_RGB565;
  descriptor.header.w = W;
  descriptor.header.h = H;
  descriptor.header.stride = W * 2;
  descriptor.data_size = W * H * 2;
  descriptor.data = (const uint8_t *)pixels;
#ifdef ESP_PLATFORM
  ESP_LOGI("vehicle3d", "TGX 1.1.4 triangles=%u buffers=%u PSRAM",
           (unsigned)E90_TRIANGLE_COUNT, W * H * 4);
#endif
  return true;
}
extern "C" void vehicle_3d_pause(void) { active = false; }
extern "C" uint32_t vehicle_3d_generation(void) { return generation; }
extern "C" const lv_image_dsc_t *vehicle_3d_frame(uint64_t now, bool boot,
                                                  float progress, uint8_t open,
                                                  uint8_t known, uint8_t valid,
                                                  uint8_t on) {
  if (!pixels)
    return nullptr;
  bool fresh = !active;
  if (lamp_view_until && now >= lamp_view_until) { lamp_view_until = 0; moving = true; }
  uint32_t input = (uint32_t)open | ((uint32_t)known << 8) |
                   ((uint32_t)valid << 16) | ((uint32_t)on << 24);
  if (active && !boot && !moving && input == last_input)
    return &descriptor;
  if (active && now >= last_ms && now - last_ms < 50)
    return &descriptor;
  last_input = input;
  float dt =
      active && now >= last_ms ? fminf((now - last_ms) / 1000.f, .1f) : .05f;
  active = true;
  last_ms = now;
#ifdef ESP_PLATFORM
  int64_t started = esp_timer_get_time();
#endif
  moving = boot;
  // Unknown panels preserve their last known angle; their caption is owned by
  // UI.
  for (unsigned i = 1; i <= 6; i++)
    if (known & (1u << (i - 1))) {
      float target = (open & (1u << (i - 1))) ? 1.f : 0.f;
      angles[i] += (target - angles[i]) * (1 - expf(-dt * 8));
      if (fabsf(target - angles[i]) > .001f)
        moving = true;
      else
        angles[i] = target;
    }
  float target_yaw = 20, target_elev = 18;
  uint8_t active_lamps = boot ? 0 : (valid & on);
  if (active_lamps & (8 | 16 | 32)) {
    lamp_view_yaw =
        (active_lamps & 32)
            ? 155
            : ((active_lamps & 24) == 24 ? 0 : (active_lamps & 8 ? 58 : -58));
    lamp_view_until = now + 1100;
  }
  if (!open && now < lamp_view_until)
    target_yaw = lamp_view_yaw;
  bool left = open & 5, right = open & 10;
  if (left && right)
    target_elev = 58;
  else if (left)
    target_yaw = 58;
  else if (right)
    target_yaw = -58;
  else if ((open & 16) && !(open & 32))
    target_yaw = 155;
  float move = 0;
  if (boot) {
    float orbit = smooth((progress - .22f) / .62f);
    yaw = orbit * 360;
    elevation = 20 + 8 * sinf(orbit * PI);
    move = 7 * (1 - smooth(progress / .22f));
  } else {
    if (fresh) {
      yaw = 0;
      elevation = 22;
    }
    float delta = fmodf(target_yaw - yaw + 540, 360) - 180;
    yaw += delta * (1 - expf(-dt * 5));
    elevation += (target_elev - elevation) * (1 - expf(-dt * 5));
    if (fabsf(delta) > .05f || fabsf(target_elev - elevation) > .05f)
      moving = true;
  }
  float half_width = 2.7f + .55f * fabsf(sinf(rad(yaw))) +
                     1.45f * smooth((elevation - 25.f) / 33.f);
  renderer.setOrtho(-half_width, half_width, -half_width * H / W,
                    half_width * H / W, 1, 30);
  float az = rad(yaw), el = rad(elevation);
  renderer.setLookAt(tgx::fVec3(10 * sinf(az) * cosf(el),
                                -10 * cosf(az) * cosf(el),
                                .65f + 10 * sinf(el)),
                     tgx::fVec3(0, 0, .65f), tgx::fVec3(0, 0, 1));
  image.fillScreen(tgx::RGB565((uint16_t)0x0863));
  renderer.clearZbuffer();
  float sn[7] = {}, cs[7] = {1, 1, 1, 1, 1, 1, 1};
  for (unsigned i = 1; i <= 6; i++) {
    float a = angles[i] * rad(i <= 4 ? ((i == 1 || i == 3) ? -52 : 52)
                                     : (i == 5 ? 38 : -34));
    if (boot)
      a = 0;
    sn[i] = sinf(a);
    cs[i] = cosf(a);
  }
  uint32_t material_key = 0xffffffff;
  float wheel_sin = sinf(move / .32f), wheel_cos = cosf(move / .32f);
  for (const auto &t : e90_triangles) {
    tgx::fVec3 v[3];
    unsigned g = t.group;
    for (unsigned j = 0; j < 3; j++) {
      float x = t.v[j * 3], y = t.v[j * 3 + 1], z = t.v[j * 3 + 2];
      if (g >= 1 && g <= 4) {
        float px = (g == 1 || g == 3) ? .82f : -.82f,
              py = g <= 2 ? -.73f : .34f;
        float xx = x - px, yy = y - py;
        x = px + xx * cs[g] - yy * sn[g];
        y = py + xx * sn[g] + yy * cs[g];
      } else if (g == 5 || g == 6) {
        float py = g == 5 ? 1.76f : -.72f, pz = g == 5 ? .92f : .83f;
        float yy = y - py, zz = z - pz;
        y = py + yy * cs[g] - zz * sn[g];
        z = pz + yy * sn[g] + zz * cs[g];
      }
      if (boot && g >= 9 && g <= 12) {
        float py = e90_wheel_pivots[g - 9][1], pz = e90_wheel_pivots[g - 9][2];
        float yy = y - py, zz = z - pz;
        y = py + yy * wheel_cos - zz * wheel_sin;
        z = pz + yy * wheel_sin + zz * wheel_cos;
      }
      v[j] = tgx::fVec3(x, y + move, z);
    }
    uint16_t color = t.color;
    bool emissive = false;
    uint8_t lampbit = g == 7 ? 4 : g == 8 ? 2 : 0;
    if (lampbit) {
      bool lit = (valid & on & lampbit) != 0;
      if (boot)
        lit = g == 7 || (progress > .86f && ((int)(progress * 28) % 2 == 0));
      color = lit ? 0xefbf : 0x2125;
      emissive = lit;
    }
    if (g >= 13 && g <= 16) {
      bool front = g <= 14, left_lamp = g == 13 || g == 15;
      float cx = fabsf((t.v[0] + t.v[3] + t.v[6]) / 3.f);
      bool indicator =
          (active_lamps & (left_lamp ? 8 : 16)) && cx > (front ? .68f : .57f);
      bool white =
          front && ((active_lamps & 3) || (boot && progress > .86f &&
                                           ((int)(progress * 28) % 2 == 0)));
      bool brake = !front && (active_lamps & 32);
      if (indicator && !boot) {
        color = 0xfd20;
        emissive = true;
      } else if (white) {
        color = (active_lamps & 2) || boot ? 0xffff : 0xffb8;
        emissive = true;
      } else if (brake && !boot) {
        color = 0xf800;
        emissive = true;
      }
    }
    uint32_t key = color | (emissive ? 0x10000u : 0);
    if (key != material_key) {
      if ((key ^ material_key) & 0x10000u) {
        renderer.setShaders(emissive
                                ? (tgx::SHADER_ORTHO | tgx::SHADER_ZBUFFER |
                                   tgx::SHADER_UNLIT | tgx::SHADER_NOTEXTURE)
                                : SH);
        renderer.setMaterial(tgx::RGBf(tgx::RGB565(color)),
                             emissive ? 1.f : .65f, emissive ? 0.f : .75f, 0,
                             0);
      } else
        renderer.setMaterialColor(tgx::RGBf(tgx::RGB565(color)));
      material_key = key;
    }
    renderer.drawTriangle(v[0], v[1], v[2]);
  }
  generation++;
#ifdef ESP_PLATFORM
  static uint32_t count;
  static uint64_t total;
  static uint32_t worst;
  uint32_t us = (uint32_t)(esp_timer_get_time() - started);
  total += us;
  worst = us > worst ? us : worst;
  if (++count % 40 == 0) {
    ESP_LOGI("vehicle3d", "frames=%lu render_avg_us=%lu render_max_us=%lu",
             (unsigned long)count, (unsigned long)(total / count),
             (unsigned long)worst);
  }
#endif
  return &descriptor;
}
