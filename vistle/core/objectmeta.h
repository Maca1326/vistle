#ifndef OBJECTMETA_H
#define OBJECTMETA_H

#include <array>
#include <vector>
#include <memory>

#include "archives_config.h"

#include "export.h"
#include "vector.h"
#include "shm.h"

namespace vistle {

class V_COREEXPORT Meta {
   public:
      Meta(int block=-1, int timestep=-1, int animationstep=-1, int iteration=-1, int execcount=-1, int creator=-1);
      int block() const { return m_block; }
      void setBlock(int block) { m_block = block; }
      int numBlocks() const { return m_numBlocks; }
      void setNumBlocks(int num) { m_numBlocks = num; }
      double realTime() const { return m_realtime; }
      void setRealTime(double time) { m_realtime = time; }
      int timeStep() const { return m_timestep; }
      void setTimeStep(int timestep) { m_timestep = timestep; }
      int numTimesteps() const { return m_numTimesteps; }
      void setNumTimesteps(int num) { m_numTimesteps = num; }
      int animationStep() const { return m_animationstep; }
      void setAnimationStep(int step) { m_animationstep = step; }
      void setNumAnimationSteps(int step) { m_numAnimationsteps = step; }
      int numAnimationSteps() const { return m_numAnimationsteps; }
      int iteration() const { return m_iteration; }
      void setIteration(int iteration) { m_iteration = iteration; }
      int executionCounter() const { return m_executionCount; }
      void setExecutionCounter(int count) { m_executionCount = count; }
      int creator() const { return m_creator; }
      void setCreator(int id) { m_creator = id; }
      Matrix4 transform() const;
      void setTransform(const Matrix4 &transform);

   private:
      int m_block, m_numBlocks, m_timestep, m_numTimesteps, m_animationstep, m_numAnimationsteps, m_iteration, m_executionCount, m_creator;
      double m_realtime;
      std::array<double, 16> m_transform;

      ARCHIVE_ACCESS
      template<class Archive>
      void serialize(Archive &ar);
};

V_COREEXPORT std::ostream &operator<<(std::ostream &out, const Meta &meta);

} // namespace vistle
#endif

#include "objectmeta_impl.h"
