#include "render.isph"

#include <embree3/rtcore.isph>
#include <embree3/rtcore_ray.isph>

#include "linalg/math.isph"
#include "linalg/vec.isph"

#include "common.h"

inline uniform float dot(const uniform Vec4f a, const uniform Vec4f b) { return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w; }
inline varying float dot(const varying Vec4f a, const varying Vec4f b) { return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w; }

inline void setUpRay(uniform const TileData *uniform tile,
                     varying RTCRayHit &rayhit, const varying int x, const varying int y) {

    const Vec3f origin = tile->origin;
    RTCRay &ray = rayhit.ray;
    RTCHit &hit = rayhit.hit;

    ray.org_x = origin.x;
    ray.org_y = origin.y;
    ray.org_z = origin.z;

    const Vec3f rdir = tile->corner + x*tile->dx + y*tile->dy;
    ray.dir_x = rdir.x;
    ray.dir_y = rdir.y;
    ray.dir_z = rdir.z;

    ray.tnear = tile->tNear;
    ray.tfar = tile->tFar;

    hit.geomID = RTC_INVALID_GEOMETRY_ID;
    hit.primID = RTC_INVALID_GEOMETRY_ID;
    hit.instID[0] = RTC_INVALID_GEOMETRY_ID;

    ray.mask = RayEnabled;
    //ray.time = 0.0f;
}


inline unsigned texIndex(const ColorMapData *cmap, float val) {

    const float tc = clamp((val - cmap->min)/(cmap->max-cmap->min), 0.f, 1.f);
    const unsigned idx = tc * cmap->texWidth;
    return min(idx, cmap->texWidth-1);
}

inline Vec4f texColor(const ColorMapData *cmap, float val) {
    const unsigned idx = texIndex(cmap, val);
    const unsigned int8 *c = &cmap->texData[idx*4];
    return make_Vec4f(c[0], c[1], c[2], c[3]);
}


inline void shadeRay(uniform const SceneData *uniform scene, uniform const TileData *uniform tile,
                     const varying RTCRayHit &rayhit, const varying int x, const varying int y) {

    varying const RTCRay &ray = rayhit.ray;
    varying const RTCHit &hit = rayhit.hit;

    uniform const float ambientFactor = 0.2f;
    uniform const Vec4f specColor = make_Vec4f(0.4f, 0.4f, 0.4f, 1.0f);
    uniform const float specExp = 16.f;
    uniform const Vec4f ambient = make_Vec4f(0.2f, 0.2f, 0.2f, 1.0f);
    uniform const bool twoSided = true;

    const Vec3f origin = tile->origin;
    const Vec4f depthTransform2 = tile->depthTransform2;
    const Vec4f depthTransform3 = tile->depthTransform3;

    Vec4f shaded = make_Vec4f(0);
    float zValue = 1.f;
    if (hit.geomID != RTC_INVALID_GEOMETRY_ID) {

        const Vec3f rdir = make_Vec3f(ray.dir_x,ray.dir_y,ray.dir_z);
        const Vec3f pos = origin + ray.tfar * rdir;
        Vec3f dir = normalize(rdir);
        const Vec4f pos4 = make_Vec4f(pos.x, pos.y, pos.z, 1);
        const float win2 = dot(depthTransform2, pos4);
        const float win3 = dot(depthTransform3, pos4);
        zValue = (win2/win3+1.f)*0.5f;

        cif (scene->doShade) {

            RenderObjectData *ro = scene->ro[hit.instID[0]];
            assert(ro->geomID == hit.geomID);

            Vec4f color = scene->defaultColor;
            if (ro->hasSolidColor) {
                color = make_Vec4f(ro->solidColor[0]*255.f, ro->solidColor[1]*255.f, ro->solidColor[2]*255.f, ro->solidColor[3]*255.f);
            }
            if (scene->uvVis) {
                const float &u = hit.u;
                const float &v = hit.v;
                const float w = 1.f - u - v;
                color.x = u*255.f;
                color.y = v*255.f;
                color.z = w*255.f;
                color.w = 255.f;
            }
            else if (ro->texCoords && ro->cmap && ro->cmap->texData && ro->cmap->texWidth>0) {

                const float &u = hit.u;
                const float &v = hit.v;
                const float w = 1.f - u - v;
                float c = 0.f;
                if (ro->perPrimitiveMapping) {
                    if (ro->triangles && ro->indexBuffer) {
                        c = ro->texCoords[ro->indexBuffer[hit.primID].v3];
                    } else {
                        c = ro->texCoords[hit.primID];
                    }
                } else if (ro->triangles && ro->indexBuffer) {
                    unsigned int v0 = ro->indexBuffer[hit.primID].v0;
                    unsigned int v1 = ro->indexBuffer[hit.primID].v1;
                    unsigned int v2 = ro->indexBuffer[hit.primID].v2;

                    const float tc0 = ro->texCoords[v0];
                    const float tc1 = ro->texCoords[v1];
                    const float tc2 = ro->texCoords[v2];
                    c = w*tc0 + u*tc1 + v*tc2;
                } else if (ro->indexBuffer) {
                    unsigned int v0 = ro->indexBuffer[hit.primID].v0;
                    unsigned int v1 = ro->indexBuffer[hit.primID].v1;
                    unsigned int v2 = ro->indexBuffer[hit.primID].v2;
                    unsigned int v3 = ro->indexBuffer[hit.primID].v3;

                    const float tc0 = ro->texCoords[v0];
                    const float tc1 = ro->texCoords[v1];
                    const float tc2 = ro->texCoords[v2];
                    const float tc3 = ro->texCoords[v3];
                    c = (1-v)*((1-u)*tc0 + u*tc1) + v*((1-u)*tc3 + u*tc2);
                } else {
                    const unsigned int v0 = hit.primID;
                    const unsigned int v1 = hit.primID + 1;
                    const float tc0 = ro->texCoords[v0];
                    const float tc1 = ro->texCoords[v1];
                    c = w*tc0 + u*tc1;
                }

                Vec4f tc = texColor(ro->cmap, c);
                if (ro->cmap->blendWithMaterial) {
                    color = (tc*tc.w+color*(255.f-tc.w))/255.f;
                    color.w = 255.f;
                } else {
                    color = tc;
                }
            }

            if (ro->lighted) {
                Vec4f ambientColor = color;
                ambientColor.x *= ambientFactor;
                ambientColor.y *= ambientFactor;
                ambientColor.z *= ambientFactor;
                Vec3f normal;
                if (ro->normals[0]) {
                    const float &u = hit.u;
                    const float &v = hit.v;
                    const float w = 1.f - u - v;
                    if (ro->normalsPerPrimitiveMapping) {
                        if (ro->indexBuffer) {
                            const unsigned int idx = ro->indexBuffer[hit.primID].v3;
                            normal = make_Vec3f(ro->normals[0][idx], ro->normals[1][idx], ro->normals[2][idx]);
                        } else {
                            const unsigned int idx = hit.primID;
                            normal = make_Vec3f(ro->normals[0][idx], ro->normals[1][idx], ro->normals[2][idx]);
                        }
                    } else if (ro->triangles && ro->indexBuffer) {
                        unsigned int v0 = ro->indexBuffer[hit.primID].v0;
                        unsigned int v1 = ro->indexBuffer[hit.primID].v1;
                        unsigned int v2 = ro->indexBuffer[hit.primID].v2;
                        const Vec3f n0 = make_Vec3f(ro->normals[0][v0], ro->normals[1][v0], ro->normals[2][v0]);
                        const Vec3f n1 = make_Vec3f(ro->normals[0][v1], ro->normals[1][v1], ro->normals[2][v1]);
                        const Vec3f n2 = make_Vec3f(ro->normals[0][v2], ro->normals[1][v2], ro->normals[2][v2]);
                        normal = w*n0 + u*n1 + v*n2;
                    } else if (ro->indexBuffer) {
                        unsigned int v0 = ro->indexBuffer[hit.primID].v0;
                        unsigned int v1 = ro->indexBuffer[hit.primID].v1;
                        unsigned int v2 = ro->indexBuffer[hit.primID].v2;
                        unsigned int v3 = ro->indexBuffer[hit.primID].v3;
                        const Vec3f n0 = make_Vec3f(ro->normals[0][v0], ro->normals[1][v0], ro->normals[2][v0]);
                        const Vec3f n1 = make_Vec3f(ro->normals[0][v1], ro->normals[1][v1], ro->normals[2][v1]);
                        const Vec3f n2 = make_Vec3f(ro->normals[0][v2], ro->normals[1][v2], ro->normals[2][v2]);
                        const Vec3f n3 = make_Vec3f(ro->normals[0][v3], ro->normals[1][v3], ro->normals[2][v3]);
                        normal = (1-v)*((1-u)*n0 + u*n1) + v*((1-u)*n3 + u*n2);
                    } else {
                        const unsigned int v0 = hit.primID;
                        const unsigned int v1 = hit.primID + 1;
                        const Vec3f n0 = make_Vec3f(ro->normals[0][v0], ro->normals[1][v0], ro->normals[2][v0]);
                        const Vec3f n1 = make_Vec3f(ro->normals[0][v1], ro->normals[1][v1], ro->normals[2][v1]);
                        normal = w*n0 + u*n1;
                    }
                } else {
                    normal = make_Vec3f(hit.Ng_x,hit.Ng_y,hit.Ng_z);
                }
                normal = normalize(make_Vec3f(dot(normal, ro->normalTransform[0]), dot(normal, ro->normalTransform[1]), dot(normal, ro->normalTransform[2])));
                if (twoSided && dot(normal, dir) > 0.f)
                    normal = neg(normal);

                shaded = shaded + ambientColor * ambient;
                cfor (uniform int l=0; l<scene->numLights; ++l) {
                    const uniform Light &light = scene->lights[l];
                    cif (light.enabled) {
                        const Vec3f lv = normalize(light.isDirectional ? light.transformedPosition : light.transformedPosition-pos);
                        float atten = 1.f;
                        cif (light.isDirectional) {
                            atten = light.attenuation[0];
                            cif (light.attenuation[1]>0.f || light.attenuation[2]>0.f) {
                                Vec4f ec = make_Vec4f(light.transformedPosition-pos);
                                float d = 0.;
                                for (int i=0; i<3; ++i) {
                                    float dd = dot(scene->modelView[i], ec);
                                    d += dd*dd;
                                }
                                atten += (light.attenuation[1] + light.attenuation[2]*d)*d;
                            }
                            atten = rcp(atten);
                        }
                        shaded = shaded + ambientColor * atten * light.ambient;
                        const float ldot = max(0.f, dot(normal, lv));
                        shaded = shaded + color * atten *ldot * light.diffuse;
                        if (ldot > 0.f) {
                            const Vec3f halfway = normalize(lv-dir);
                            const float hdot = max(0.f, dot(normal, halfway));
                            if (hdot > 0.f) {
                                shaded = shaded + specColor * atten * powf(hdot, specExp) * light.specular;
                            }
                        }
                    }
                }
            } else {
                shaded = color;
            }

            shaded.x = clamp(shaded.x, 0.f, 255.f);
            shaded.y = clamp(shaded.y, 0.f, 255.f);
            shaded.z = clamp(shaded.z, 0.f, 255.f);
            shaded.w = clamp(shaded.w, 0.f, 255.f);

        } else {

            shaded = scene->defaultColor;
        }
    }

    tile->depth[y*tile->imgWidth+x] = zValue;
    unsigned int8 *rgba = tile->rgba+(y*tile->imgWidth+x)*4;
    rgba[0] = shaded.x;
    rgba[1] = shaded.y;
    rgba[2] = shaded.z;
    rgba[3] = shaded.w;
}


export void ispcRenderTilePacket(uniform const SceneData *uniform scene, uniform const TileData *uniform tile) {

   uniform const int x0=tile->x0, x1=tile->x1;
   uniform const int y0=tile->y0, y1=tile->y1;

   int x,y;
   foreach_tiled(y=y0...y1, x=x0...x1) {

      varying RTCRayHit ray;
      ray.ray.flags = 0;
      setUpRay(tile, ray, x, y);
      {
        uniform RTCIntersectContext context;
        rtcInitIntersectContext(&context);
        rtcIntersectV(scene->scene,&context,&ray);
      }
      shadeRay(scene, tile, ray, x, y);
   }
}

export void ispcRenderTileStream(uniform const SceneData *uniform scene, uniform const TileData *uniform tile) {

#define TSX 8
#define TSY 8

   uniform const int tx0=tile->x0, tx1=tile->x1;
   uniform const int ty0=tile->y0, ty1=tile->y1;

   for (uniform int y0=ty0; y0<ty1; y0 += TSY) {
       uniform int y1 = min(y0+TSY, ty1);
       for (uniform int x0=tx0; x0<tx1; x0 += TSX) {
           uniform int x1 = min(x0+TSX, tx1);

           bool valid_stream[TSX*TSY];
           RTCRayHit primary_stream[TSX*TSY];

           int x, y;
           /* set up rays */
           uniform int i=0;
           foreach_tiled(y=y0...y1, x=x0...x1) {
               /* ISPC workaround for mask == 0 */
               if (all(__mask == 0)) continue;

               bool mask = __mask; unmasked { valid_stream[i] = mask; }
               RTCRayHit &rayhit = primary_stream[i];
               setUpRay(tile, rayhit, x, y);
               mask = __mask; unmasked { // invalidates inactive rays
                   rayhit.ray.tnear = mask ? 0.0f         : (float)(pos_inf);
                   rayhit.ray.tfar  = mask ? (float)(inf) : (float)(neg_inf);
               }

               ++i;
           }

           /* shoot rays */
           uniform RTCIntersectContext primary_context;
           rtcInitIntersectContext(&primary_context);
           primary_context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;
           //primary_context.userRayExt = &primary_stream;
           rtcIntersectVM(scene->scene,&primary_context,&primary_stream[0],i,sizeof(RTCRayHit));

           /* shade rays */
           i=-1;
           foreach_tiled(y=y0...y1, x=x0...x1) {
               /* ISPC workaround for mask == 0 */
               if (all(__mask == 0)) continue;

               ++i;
               /* ignore invalid rays */
               if (valid_stream[i] == false) continue;

               RTCRayHit &rayhit = primary_stream[i];
               shadeRay(scene, tile, rayhit, x, y);
           }
       }
   }

#undef TSX
#undef TSY
}
