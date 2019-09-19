#ifndef CALC_H
#define CALC_H

#include <module/module.h>
#include <core/vector.h>
#include <core/vec.h>

class CalcExpression;

class Calc: public vistle::Module {

 public:
   Calc(const std::string &name, int moduleID, mpi::communicator comm);
   ~Calc();

 private:
   static const int NumPorts = 3;

   bool compute(std::shared_ptr<vistle::PortTask> task) const override;
   bool prepare() override;
   bool reduce(int t) override;

   vistle::Port *m_dataIn[NumPorts] = {nullptr, nullptr, nullptr};
   vistle::Port *m_dataOut = nullptr;
   vistle::IntParameter *m_outputType = nullptr;
   vistle::StringParameter *m_dataTerm=nullptr, *m_gridTerm=nullptr, *m_normTerm=nullptr;

   std::unique_ptr<CalcExpression> m_dataExpr, m_gridExpr, m_normExpr;
};

#endif
