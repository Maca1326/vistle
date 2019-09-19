#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <core/object.h>
#include <core/vec.h>
#include <core/normals.h>
#include <core/coords.h>

#include "Calc.h"
#include <util/enum.h>

#include <SeExpr2/Expression.h>
#include <SeExpr2/ExprFunc.h>
#include <SeExpr2/Vec.h>

#define STACK_DEPTH 256

DEFINE_ENUM_WITH_STRING_CONVERSIONS(OutputType, (AsInput)(AsInputGrid)(Vec1)(Vec3))

//! Simple expression class to support our function calculator
class CalcExpression : public SeExpr2::Expression {
  public:
    //! Constructor that takes the expression to parse
    CalcExpression(const std::string& expr) : SeExpr2::Expression(expr) {
    }

    //! Empty constructor
    CalcExpression() : Expression() {
    }

    std::vector<double> m_result;


    const std::vector<double> &result() {
        return m_result;
    }

  private:
    //! Simple variable that just returns its internal value
    struct Var : public SeExpr2::ExprVarRef {
        Var(int dim)
        : ExprVarRef(SeExpr2::ExprType().FP(dim).Varying())
        , dim(dim)
        , val(0.0) {
            assert(dim == 1 || dim == 3);
        }

        void setValue(int d, double v) {
            assert(d < dim);
            val[d] = v;
        }

        void eval(double* result) {
            for (int k = 0; k < dim; k++)
                result[k] = val[k];
        }

        void eval(const char** result) {}

        int dim = 0;
        SeExpr2::Vec<double, 3, false> val;  // independent variable
    };

    mutable std::map<std::string, Var> m_variables;

    //! resolve function that only supports one external variable 'x'
    SeExpr2::ExprVarRef* resolveVar(const std::string& name0) const {
        std::string name = name0;
        int dim = 1;
        if (name=="i" || name=="idx" || name=="index") {
            name = "index";
        } else if (name=="in.r") {
            dim = 3;
        } else if (name=="in.x") {
        } else if (name=="in.y") {
        } else if (name=="in.z") {
        } else if (name=="in.n") {
            dim = 3;
        } else if (name=="in.n.x") {
        } else if (name=="in.n.y") {
        } else if (name=="in.n.z") {
        } else if (name=="in.d0" || name=="d0") {
            dim = 3;
        } else if (name=="in.d0.x" || name=="d0.x") {
        } else if (name=="in.d0.y" || name=="d0.y") {
        } else if (name=="in.d0.z" || name=="d0.z") {
        } else if (name=="rank") {
        } else if (name=="block") {
        } else if (name=="t" || name=="time") {
            name = "time";
        } else if (name=="ts" || name=="timestep") {
            name = "timestep";
        } else {
            addError("Use of undefined variable " + name, 0, 0);
            return nullptr;
        }

        auto it = m_variables.find(name);
        if (it == m_variables.end()) {
            it = m_variables.emplace(std::make_pair<std::string,Var>(std::move(name), Var(dim))).first;
        }

        return &it->second;
    }

    public:
    void setVar(const std::string &name, const double &value) {
        auto var = dynamic_cast<Var *>(resolveVar(name));
        assert(var);
        if (!var)
            return;
        var->setValue(0, value);
    }

    void setVar(const std::string &name, const double &v0, const double &v1, const double &v2) {
        auto var = dynamic_cast<Var *>(resolveVar(name));
        assert(var);
        if (!var)
            return;
        var->setValue(0, v0);
        var->setValue(1, v1);
        var->setValue(2, v2);
    }
};


using namespace vistle;

Calc::Calc(const std::string &name, int moduleID, mpi::communicator comm)
: Module("transform vector fields to scalar fields", name, moduleID, comm) {
    for (int i=0; i<NumPorts; ++i) {
        m_dataIn[i] = createInputPort("data_in"+ std::to_string(i));
    }
    m_dataOut = createOutputPort("data_out");

    m_dataTerm = addStringParameter("formula", "formula for computing data", "");
    m_gridTerm = addStringParameter("grid_formula", "formula computing grid", "");
    m_normTerm = addStringParameter("normal_formula", "formula computing normals", "");
    m_outputType = addIntParameter("output_type", "type of output", AsInput, Parameter::Choice);
}

Calc::~Calc() {
}

bool Calc::compute(std::shared_ptr<PortTask> task) const {

    Object::const_ptr oin[NumPorts];
    DataBase::const_ptr din[NumPorts];
    Object::const_ptr grid;
    Normals::const_ptr normals;

    int t = -1;
    std::string attr;

    for (int i=0; i<NumPorts; ++i) {
        if (!m_dataIn[i]->isConnected())
            continue;

        oin[i] = task->expect<Object>(m_dataIn[i]);
        din[i] = DataBase::as(oin[i]);
        Object::const_ptr g;
        if (t == -1 && oin[i]) {
            t = oin[i]->getTimestep();
        }
        if (din[i]) {
            g = din[i]->grid();
            if (!g) {
                din[i].reset();
            }
        }
        if (!g && oin[i]->getInterface<GeometryInterface>())
            g = oin[i];
        if (grid) {
            if (*g != *grid) {
                sendError("Grids on input ports do not match");
                return true;
            }
        } else {
            auto gi = g->getInterface<GeometryInterface>();
            if (!gi) {
                sendError("Input does not have a grid on port %s", m_dataIn[i]->getName().c_str());
                return true;
            }
            grid = g;
            if (t == -1 && grid) {
                t = grid->getTimestep();
            }
            normals = gi->normals();
            if (t == -1 && normals) {
                t = normals->getTimestep();
            }
        }
    }

    if (m_dataExpr) {

        Object::ptr out;
        int outdim = 3;
        switch(m_outputType->getValue()) {
        case AsInput:
            out = oin[0]->clone();
            break;
        case AsInputGrid:
            out = grid->clone();
            break;
        case Vec1:
            out.reset(new Vec<Scalar,1>(Index(0)));
            break;
        case Vec3:
            out.reset(new Vec<Scalar,3>(Index(0)));
            break;
        }

        int timestep = getTimestep(din[0]);
        m_dataExpr->setVar("timestep", timestep);
        double time = getRealTime(din[0]);
        m_dataExpr->setVar("time", time);
        m_dataExpr->setVar("rank", rank());

        //auto cout = Coords::as(outgrid);
        auto dout = DataBase::as(out);
        if (dout) {
            dout->resetArrays();
            int indim[NumPorts];
            for (int i=0; i<NumPorts; ++i) {
                indim[i] = din[i]->dimension();
            }
            outdim = dout->dimension();
            m_dataExpr->setVar("block", din[0]->getBlock());

            auto gi = grid->getInterface<GridInterface>();
            auto mapping = din[0]->guessMapping();
            if (mapping == DataBase::Vertex) {
                Index nvert = din[0]->getSize();
                for (Index i=0; i<nvert; ++i) {
                    m_dataExpr->setVar("index", i);
                    auto v = gi->getVertex(i);
                    m_dataExpr->setVar("in.x", v[0]);
                    m_dataExpr->setVar("in.y", v[1]);
                    m_dataExpr->setVar("in.z", v[2]);
                    m_dataExpr->setVar("in.r", v[0], v[1], v[2]);

                    std::vector<double> d0(indim[0]);
                    for (int c=0; c<indim[0]; ++c) {
                        d0[c] = din[0]->value(i, c);
                    }
                    for (int c=indim[0]; c<3; ++c)
                        d0[c] = 0.;
                    m_dataExpr->setVar("in.d0.x", d0[0]);
                    m_dataExpr->setVar("in.d0.y", d0[1]);
                    m_dataExpr->setVar("in.d0.z", d0[2]);
                    m_dataExpr->setVar("in.d0", d0[0], d0[1], d0[2]);

                    const double *result = m_dataExpr->evalFP();
                    for (int c=0; c<outdim; ++c)
                        dout->setValue(i, c, result[c]);
                }
            } else if (mapping == DataBase::Element) {
            }
        }


        out->copyAttributes(din[0]);
        if (dout)
            dout->setGrid(din[0]->grid());
        task->addObject("data_out", out);
    } else {
        task->passThroughObject("data_out", din[0]);
    }

    return true;
}

bool Calc::prepare() {

    std::string term = m_dataTerm->getValue();
    if (!term.empty()) {
        m_dataExpr = std::make_unique<CalcExpression>();
        m_dataExpr->setDesiredReturnType(SeExpr2::ExprType().FP(3));
        m_dataExpr->setExpr(term);
        if (!m_dataExpr->isValid() && rank()==0) {
            sendError("formula not valid: %s", m_dataExpr->parseError().c_str());
        }
    }

    term = m_gridTerm->getValue();
    if (!term.empty()) {
        m_gridExpr = std::make_unique<CalcExpression>();
        m_gridExpr->setDesiredReturnType(SeExpr2::ExprType().FP(3));
        m_gridExpr->setExpr(term);
        if (!m_gridExpr->isValid() && rank()==0) {
            sendError("formula not valid: %s", m_gridExpr->parseError().c_str());
        }
    }

    term = m_normTerm->getValue();
    if (!term.empty()) {
        m_normExpr = std::make_unique<CalcExpression>();
        m_normExpr->setDesiredReturnType(SeExpr2::ExprType().FP(3));
        m_normExpr->setExpr(term);
        if (!m_normExpr->isValid() && rank()==0) {
            sendError("formula not valid: %s", m_normExpr->parseError().c_str());
        }
    }

    return true;
}

bool Calc::reduce(int t) {

    m_dataExpr.reset();
    m_gridExpr.reset();
    m_normExpr.reset();

    SeExpr2::ExprFunc::cleanup();

    return true;
}

MODULE_MAIN(Calc)
