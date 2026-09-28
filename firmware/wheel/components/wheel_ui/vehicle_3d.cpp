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
constexpr unsigned W = VEHICLE_3D_WIDTH, H = VEHICLE_3D_HEIGHT;
constexpr auto SH = tgx::SHADER_ORTHO | tgx::SHADER_ZBUFFER | tgx::SHADER_FLAT |
                    tgx::SHADER_NOTEXTURE;
tgx::Renderer3D<tgx::RGB565, SH | tgx::SHADER_UNLIT, uint16_t, 1> renderer;
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
tgx::fVec3 world_point(tgx::fVec3 p, float sn, float cs, float tx, float ty) {
  return tgx::fVec3(p.x * cs - p.y * sn + tx, p.x * sn + p.y * cs + ty, p.z);
}
uint16_t dim_color(uint16_t c, float factor) {
  factor = fmaxf(0.f, fminf(1.f, factor));
  return (uint16_t)((unsigned)(((c >> 11) & 31) * factor) << 11 |
                    (unsigned)(((c >> 5) & 63) * factor) << 5 |
                    (unsigned)((c & 31) * factor));
}
void unlit(uint16_t c) {
  renderer.setShaders(tgx::SHADER_ORTHO | tgx::SHADER_ZBUFFER |
                      tgx::SHADER_UNLIT | tgx::SHADER_NOTEXTURE);
  renderer.setMaterial(tgx::RGBf(tgx::RGB565(c)), 1, 0, 0, 0);
}
// Bounded layered ground illumination; no off-screen pass or framebuffer copy.
void ground_light(float x, float y, float rx, float ry, uint16_t c,
                  float strength, float sn, float cs, float tx, float ty) {
  for (unsigned layer = 0; layer < 5; ++layer) {
    float scale = 1.f - layer * .14f;
    unlit(dim_color(c, strength * (.05f + .035f * layer)));
    float z = -.168f + layer * .0008f;
    auto center = world_point(tgx::fVec3(x, y, z), sn, cs, tx, ty);
    for (unsigned k = 0; k < 12; ++k) {
      float a = 2 * PI * k / 12, b = 2 * PI * (k + 1) / 12;
      auto p = world_point(
          tgx::fVec3(x + rx * scale * cosf(a), y + ry * scale * sinf(a), z), sn,
          cs, tx, ty);
      auto q = world_point(
          tgx::fVec3(x + rx * scale * cosf(b), y + ry * scale * sinf(b), z), sn,
          cs, tx, ty);
      renderer.drawTriangle(center, p, q);
    }
  }
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
                    tgx::RGBf(.7f, .7f, .7f), tgx::RGBf(2.f, 2.f, 2.f));
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
  if (lamp_view_until && now >= lamp_view_until) {
    lamp_view_until = 0;
    moving = true;
  }
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
  float move = 0, root_yaw = 0, root_x = 0, root_y = 0;
  float reveal = 1.f, angel_strength = 1.f;
  float half_width;
  if (boot) {
    float drift = smooth((progress - .58f) / .38f);
    yaw = 0;
    elevation = 6 + 8 * drift;
    angel_strength = smooth((progress - .10f) / .16f);
    reveal = smooth((progress - .30f) / .24f);
    half_width =
        2.55f - .70f * smooth((progress - .24f) / .30f) + 1.55f * drift;
    root_yaw = rad(-72.f * smooth((progress - .58f) / .20f));
    root_x = 8.f * drift * drift;
    root_y = .7f * sinf(drift * PI);
    move = 6.f * drift;
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
    half_width = 2.7f + .55f * fabsf(sinf(rad(yaw))) +
                 1.45f * smooth((elevation - 25.f) / 33.f);
    if (known != 63 || valid != 63)
      reveal = .45f;
  }
  float root_sn = sinf(root_yaw), root_cs = cosf(root_yaw);
  renderer.setOrtho(-half_width, half_width, -half_width * H / W,
                    half_width * H / W, 1, 30);
  float target_z = boot ? .30f : .48f;
  float az = rad(yaw), el = rad(elevation);
  renderer.setLookAt(tgx::fVec3(10 * sinf(az) * cosf(el),
                                -10 * cosf(az) * cosf(el),
                                target_z + 10 * sinf(el)),
                     tgx::fVec3(0, 0, target_z), tgx::fVec3(0, 0, 1));
  image.fillScreen(tgx::RGB565((uint16_t)0));
  renderer.clearZbuffer();
  uint8_t floor_lamps = boot ? 4 : active_lamps;
  float floor_strength = boot ? angel_strength : 1.f;
  if (floor_lamps & 7) {
    float energy = (floor_lamps & 3) ? 1.f : .45f;
    for (int side = -1; side <= 1; side += 2)
      ground_light(side * .62f, -2.45f, .50f, .8f, 0xbfff,
                   energy * floor_strength, root_sn, root_cs, root_x, root_y);
  }
  if (floor_lamps & 8)
    ground_light(.85f, -1.90f, .65f, .55f, 0xfd20, 1, root_sn, root_cs, root_x,
                 root_y);
  if (floor_lamps & 16)
    ground_light(-.85f, -1.90f, .65f, .55f, 0xfd20, 1, root_sn, root_cs, root_x,
                 root_y);
  if (floor_lamps & 32)
    for (int side = -1; side <= 1; side += 2)
      ground_light(side * .6f, 2.55f, .55f, .65f, 0xf800, 1, root_sn, root_cs,
                   root_x, root_y);
  // Body-mounted fender repeaters: independent of door hinge transforms.
  // Slightly enlarged for the native 320x172 display; no local blink timer.
  for (int side = -1; side <= 1; side += 2) {
    bool lit = (active_lamps & (side > 0 ? 8 : 16)) != 0;
    uint16_t lens = lit ? 0xfd20 : dim_color(0x4208, reveal);
    unlit(lens);
    tgx::fVec3 p[4];
    const float yy[4] = {-.99f, -.80f, -.80f, -.99f};
    const float zz[4] = {.68f, .68f, .735f, .735f};
    for (unsigned k = 0; k < 4; ++k)
      p[k] = world_point(tgx::fVec3(side * 1.006f, yy[k], zz[k]), root_sn,
                         root_cs, root_x, root_y);
    renderer.drawTriangle(p[0], p[1], p[2]);
    renderer.drawTriangle(p[0], p[2], p[3]);
    if (lit) {
      unlit(dim_color(lens, .16f));
      for (auto &point : p)
        point.z = -.336f - point.z;
      renderer.drawTriangle(p[0], p[1], p[2]);
      renderer.drawTriangle(p[0], p[2], p[3]);
    }
  }
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
      v[j] = world_point(tgx::fVec3(x, y, z), root_sn, root_cs, root_x, root_y);
    }
    uint16_t color = t.color;
    bool emissive = false;
    uint8_t lampbit = g == 7 ? 4 : g == 8 ? 2 : 0;
    if (lampbit) {
      bool lit = (valid & on & lampbit) != 0;
      if (boot)
        lit = g == 7 && angel_strength > 0;
      color = lit ? dim_color(0xefff, boot ? angel_strength : 1.f) : 0x2125;
      emissive = lit;
    }
    if (g >= 13 && g <= 16) {
      bool front = g <= 14, left_lamp = g == 13 || g == 15;
      float cx = fabsf((t.v[0] + t.v[3] + t.v[6]) / 3.f);
      bool indicator =
          (active_lamps & (left_lamp ? 8 : 16)) && cx > (front ? .68f : .57f);
      bool white = front && (active_lamps & 3);
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
    if (t.material == 1 && !emissive) {
      // Clearcoat is additive: TGX's base-color multiplication otherwise
      // suppresses white highlights on black paint. Camera-relative studio
      // strips keep the silhouette readable as doors and the camera move.
      auto a = v[1] - v[0], b = v[2] - v[0];
      float nx = a.y * b.z - a.z * b.y;
      float ny = a.z * b.x - a.x * b.z;
      float nz = a.x * b.y - a.y * b.x;
      float length = sqrtf(nx * nx + ny * ny + nz * nz);
      if (length > .000001f) {
        nx /= length;
        ny /= length;
        nz /= length;
      }
      float side = nx * cosf(az) + ny * sinf(az);
      float facing =
          nx * sinf(az) * cosf(el) - ny * cosf(az) * cosf(el) + nz * sinf(el);
      float strip = fabsf(.50f * side + .70f * facing + .50f * nz);
      strip = fminf(1.f, strip);
      float gloss = strip * strip;
      gloss *= gloss;
      gloss *= gloss;
      float value =
          .065f + .10f * fabsf(nz) + .055f * fabsf(facing) + .32f * gloss;
      color = (uint16_t)((unsigned)(31 * value) << 11 |
                         (unsigned)(63 * value) << 5 |
                         (unsigned)(31 * fminf(1.f, value * 1.04f)));
    }
    if (!boot && t.material == 1 && g >= 1 && g <= 4 &&
        (known & open & (1u << (g - 1)))) {
      // Subtle red paint tint, preserving the metallic highlights. Unknown
      // or merely animating-to-closed doors must not claim an active opening.
      unsigned r = (color >> 11) & 31, green = (color >> 5) & 63,
               b = color & 31;
      r += (31 - r) / 5;
      green = green * 4 / 5;
      b = b * 4 / 5;
      color = (uint16_t)((r << 11) | (green << 5) | b);
    }
    if (!emissive)
      color = dim_color(color, reveal);
    if (emissive) {
      // A dim mirrored light image below the virtual floor complements the
      // pool. Only luminous surfaces are mirrored, avoiding a second full-car
      // render.
      unlit(dim_color(color, .16f));
      tgx::fVec3 reflected[3];
      for (unsigned k = 0; k < 3; k++)
        reflected[k] = tgx::fVec3(v[k].x, v[k].y, -.336f - v[k].z);
      renderer.drawTriangle(reflected[0], reflected[1], reflected[2]);
      material_key = 0xffffffff;
    }
    uint32_t key =
        color | (emissive ? 0x10000u : 0) | ((uint32_t)t.material << 17);
    if (key != material_key) {
      if ((key ^ material_key) & 0x70000u) {
        renderer.setShaders((emissive || t.material == 1)
                                ? (tgx::SHADER_ORTHO | tgx::SHADER_ZBUFFER |
                                   tgx::SHADER_UNLIT | tgx::SHADER_NOTEXTURE)
                                : SH);
        renderer.setMaterial(
            tgx::RGBf(tgx::RGB565(color)),
            (emissive || t.material == 1) ? 1.f : (t.material ? .45f : .65f),
            emissive ? 0.f : (t.material ? .45f : .75f), 0.f, 0);
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
