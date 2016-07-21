#ifndef TRACER_H
#define TRACER_H

// define for profiling
//#define TIMING

#include <future>

#include <core/unstr.h>
#include <module/module.h>
#include <core/lines.h>

#include "Integrator.h"


class Tracer: public vistle::Module {

public:
    Tracer(const std::string &shmname, const std::string &name, int moduleID);
    ~Tracer();


 private:
    virtual bool compute();
    virtual bool prepare();
    virtual bool reduce(int timestep);

    std::vector<std::vector<vistle::Object::const_ptr>> grid_in;
    std::vector<std::vector<std::future<vistle::Celltree3::const_ptr>>> celltree;
    std::vector<std::vector<vistle::Vec<vistle::Scalar,3>::const_ptr>> data_in0;
    std::vector<std::vector<vistle::Vec<vistle::Scalar>::const_ptr>> data_in1;
    vistle::Vector3 Integration(const vistle::Vector3 &p0, const vistle::Vector3 &v0, const vistle::Scalar &dt);
    vistle::Index findCell(const vistle::Vector3 &point,
                           vistle::Object::const_ptr grid,
                           std::array<vistle::Vector3, 2> boundingbox,
                           vistle::Index lastcell);

    vistle::IntParameter *m_useCelltree;
    bool m_havePressure;

};


class BlockData{

    friend class Particle;

private:
    vistle::Object::const_ptr m_grid;
    const vistle::GridInterface *m_gridInterface;
    vistle::Vec<vistle::Scalar, 3>::const_ptr m_vecfld;
    vistle::Vec<vistle::Scalar>::const_ptr m_scafld;
    vistle::DataBase::Mapping m_vecmap, m_scamap;
    vistle::Lines::ptr m_lines;
    std::vector<vistle::Vec<vistle::Scalar, 3>::ptr> m_ivec;
    std::vector<vistle::Vec<vistle::Scalar>::ptr> m_iscal;
    vistle::Vec<vistle::Index>::ptr m_ids;
    vistle::Vec<vistle::Index>::ptr m_steps;
    const vistle::Scalar *m_vx, *m_vy, *m_vz, *m_p;

public:
    BlockData(vistle::Index i,
              vistle::Object::const_ptr grid,
              vistle::Vec<vistle::Scalar, 3>::const_ptr vdata,
              vistle::Vec<vistle::Scalar>::const_ptr pdata = nullptr);
    ~BlockData();

    void setMeta(const vistle::Meta &meta);
    const vistle::GridInterface *getGrid();
    vistle::Vec<vistle::Index>::ptr ids() const;
    vistle::Vec<vistle::Index>::ptr steps() const;
    vistle::Vec<vistle::Scalar, 3>::const_ptr getVecFld();
    vistle::DataBase::Mapping getVecMapping() const;
    vistle::Vec<vistle::Scalar>::const_ptr getScalFld();
    vistle::DataBase::Mapping getScalMapping() const;
    vistle::Lines::ptr getLines();
    std::vector<vistle::Vec<vistle::Scalar, 3>::ptr> getIplVec();
    std::vector<vistle::Vec<vistle::Scalar>::ptr> getIplScal();
    void addLines(vistle::Index id, const std::vector<vistle::Vector3> &points,
                 const std::vector<vistle::Vector3> &velocities,
                 const std::vector<vistle::Scalar> &pressures,
                 const std::vector<vistle::Index> &steps);
};

class Particle {

    friend class Integrator;

public:
    DEFINE_ENUM_WITH_STRING_CONVERSIONS(StopReason,
                                        (StillActive)
                                        (InitiallyOutOfDomain)
                                        (OutOfDomain)
                                        (NotMoving)
                                        (StepLimitReached)
                                        (NumStopReasons)
    )
    Particle(vistle::Index i, const vistle::Vector3 &pos, vistle::Scalar h, vistle::Scalar hmin,
             vistle::Scalar hmax, vistle::Scalar errtol, IntegrationMethod int_mode, const std::vector<std::unique_ptr<BlockData>> &bl,
             vistle::Index stepsmax, bool forward);
    ~Particle();
    void PointsToLines();
    bool isActive();
    bool inGrid();
    bool isMoving(vistle::Index maxSteps, vistle::Scalar minSpeed);
    bool findCell(const std::vector<std::unique_ptr<BlockData>> &block);
    void Deactivate(StopReason reason);
    void EmitData(bool havePressure);
    bool Step();
    void Communicator(boost::mpi::communicator mpi_comm, int root, bool havePressure);
    bool leftNode();
    void UpdateBlock();
    StopReason stopReason() const;

private:
    vistle::Index m_id; //!< partcle id
    vistle::Vector3 m_x; //!< current position
    vistle::Vector3 m_xold; //!< previous position
    std::vector<vistle::Vector3> m_xhist; //!< trajectory
    vistle::Vector3 m_v; //!< current velocity
    vistle::Scalar m_p; //!< current pressure
    std::vector<vistle::Vector3> m_vhist; //!< previous velocities
    std::vector<vistle::Scalar> m_pressures; //!< previous pressures
    std::vector<vistle::Index> m_steps; //!< previous steps
    vistle::Index m_stp; //!< current integration step
    BlockData* m_block; //!< block for current particle position
    vistle::Index m_el; //!< index of cell for current particle position
    bool m_ingrid; //!< particle still within domain on some rank
    bool m_searchBlock; //!< particle is new - has to be initialized
    Integrator m_integrator;
    StopReason m_stopReason; //! reason why particle was deactivated
};
#endif
