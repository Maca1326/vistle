#ifndef VISTLEGEOMETRYGENERATOR_H
#define VISTLEGEOMETRYGENERATOR_H

#include <vistle/core/object.h>
#include <vistle/renderer/renderobject.h>
#include <mutex>
#include <memory>
#include <osg/StateSet>
#include <osg/KdTree>
#include <osg/Texture1D>
#include <osg/Image>
#include <osg/Uniform>
#include <osg/ref_ptr>

#ifdef COVER_PLUGIN
#include <cover/coVRShader.h>
#endif

namespace osg {
   class MatrixTransform;
}

struct OsgColorMap {
    OsgColorMap();
    void setName(const std::string &species);
    void setRange(float min, float max);
    void setBlendWithMaterial(bool enable);
#ifdef COVER_PLUGIN
    std::shared_ptr<opencover::coVRShader> shader;
    std::shared_ptr<opencover::coVRShader> shaderUnlit;
#endif
    bool blendWithMaterial = false;
    float rangeMin = 0.f, rangeMax = 1.f;
    osg::ref_ptr<osg::Texture1D> texture;
    osg::ref_ptr<osg::Image> image;
};

typedef std::map<std::string, OsgColorMap> OsgColorMapMap;

class VistleGeometryGenerator {
   public:
      VistleGeometryGenerator(std::shared_ptr<vistle::RenderObject> ro,
            vistle::Object::const_ptr geo,
            vistle::Object::const_ptr normal,
            vistle::Object::const_ptr mapped);

      const std::string &species() const;
      void setColorMaps(const OsgColorMapMap *colormaps);

      osg::MatrixTransform *operator()(osg::ref_ptr<osg::StateSet> state = NULL);

      static bool isSupported(vistle::Object::Type t);

      static void lock();
      static void unlock();

   private:
      std::shared_ptr<vistle::RenderObject> m_ro;
      vistle::Object::const_ptr m_geo;
      vistle::Object::const_ptr m_normal;
      vistle::Object::const_ptr m_mapped;

      std::string m_species;

      const OsgColorMapMap *m_colormaps = nullptr;

      static std::mutex s_coverMutex;
};

#endif
