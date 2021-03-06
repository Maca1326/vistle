// Copyright (c) Lawrence Livermore National Security, LLC and other VisIt
// Project developers.  See the top-level LICENSE file for dates and other
// details.  No copyright assignment is required to contribute to VisIt.

#include "VectorTypes.h"
#include <algorithm>

#include "VisItDataInterfaceRuntime.h"
#include "VisItDataInterfaceRuntimeP.h"

#include "SimulationMetaData.h"
#include "MeshMetaData.h"
#include "VariableMetaData.h"
#include "MaterialMetaData.h"
#include "CurveMetaData.h"
#include "ExpressionMetaData.h"
#include "SpeciesMetaData.h"
#include "CommandMetaData.h"
#include "MessageMetaData.h"

#include <vector>
struct VisIt_SimulationMetaData : public VisIt_ObjectBase
{
    VisIt_SimulationMetaData();
    virtual ~VisIt_SimulationMetaData();

    int                       simulationMode;
    int                       cycle;
    double                    time;

    std::vector<visit_handle> meshes;
    std::vector<visit_handle> variables;
    std::vector<visit_handle> materials;
    std::vector<visit_handle> curves;
    std::vector<visit_handle> expressions;
    std::vector<visit_handle> species;

    std::vector<visit_handle> genericCommands;
    std::vector<visit_handle> customCommands;

    std::vector<visit_handle> messages;
};

VisIt_SimulationMetaData::VisIt_SimulationMetaData() : 
    VisIt_ObjectBase(VISIT_SIMULATION_METADATA),
    simulationMode(0), 
    cycle(0), time(0.),
    meshes(), variables(), materials(), curves(), expressions(), species(),
    genericCommands(), customCommands(), messages()
{
}

VisIt_SimulationMetaData::~VisIt_SimulationMetaData()
{
    size_t i;
    for(i = 0; i < meshes.size(); ++i)
        simv2_FreeObject(meshes[i]);
    for(i = 0; i < variables.size(); ++i)
        simv2_FreeObject(variables[i]);
    for(i = 0; i < materials.size(); ++i)
        simv2_FreeObject(materials[i]);
    for(i = 0; i < curves.size(); ++i)
        simv2_FreeObject(curves[i]);
    for(i = 0; i < expressions.size(); ++i)
        simv2_FreeObject(expressions[i]);
    for(i = 0; i < species.size(); ++i)
        simv2_FreeObject(species[i]);

    for(i = 0; i < genericCommands.size(); ++i)
        simv2_FreeObject(genericCommands[i]);
    for(i = 0; i < customCommands.size(); ++i)
        simv2_FreeObject(customCommands[i]);

    for(i = 0; i < messages.size(); ++i)
        simv2_FreeObject(messages[i]);
}

static VisIt_SimulationMetaData *
GetObject(visit_handle h, const char *fname)
{
    VisIt_SimulationMetaData *obj = (VisIt_SimulationMetaData *)VisItGetPointer(h);
    char tmp[100];
    if(obj != NULL)
    {
        if(obj->objectType() != VISIT_SIMULATION_METADATA)
        {
            snprintf(tmp, 100, "%s: The provided handle does not point to a SimulationMetaData object.", fname);
            VisItError(tmp);
            obj = NULL;
        }
    }
    else
    {
        snprintf(tmp, 100, "%s: An invalid handle was provided.", fname);
        VisItError(tmp);
    }

    return obj;
}

/*******************************************************************************
 * Public functions, available to C 
 ******************************************************************************/
int
simv2_SimulationMetaData_alloc(visit_handle *h)
{
    *h = VisItStorePointer(new VisIt_SimulationMetaData);
    return (*h != VISIT_INVALID_HANDLE) ? VISIT_OKAY : VISIT_ERROR;
}

int
simv2_SimulationMetaData_free(visit_handle h)
{
    VisIt_SimulationMetaData *obj = GetObject(h, "simv2_SimulationMetaData_free");
    int retval = VISIT_ERROR;
    if(obj != NULL)
    {
        delete obj;
        VisItFreePointer(h);
        retval = VISIT_OKAY;
    }
    return retval;
}

int
simv2_SimulationMetaData_setMode(visit_handle h, int mode)
{
    if(mode != VISIT_SIMMODE_UNKNOWN &&
       mode != VISIT_SIMMODE_RUNNING &&
       mode != VISIT_SIMMODE_STOPPED)
    {
        VisItError("Simulation mode must be one of:VISIT_SIMMODE_UNKNOWN, "
            "VISIT_SIMMODE_RUNNING, VISIT_SIMMODE_STOPPED");
        return VISIT_ERROR;
    }
    int retval = VISIT_ERROR;
    VisIt_SimulationMetaData *obj = GetObject(h, "simv2_SimulationMetaData_setMode");
    if(obj != NULL)
    {
        obj->simulationMode = mode;
        retval = VISIT_OKAY;
    }
    return retval;
}

int
simv2_SimulationMetaData_setCycleTime(visit_handle h, int cycle, double time)
{
    int retval = VISIT_ERROR;
    VisIt_SimulationMetaData *obj = GetObject(h, "simv2_SimulationMetaData_setCycleTime");
    if(obj != NULL)
    {
        obj->cycle = cycle;
        obj->time = time; 
        retval = VISIT_OKAY;
    }
    return retval;
}

#define ADD_METADATA(FUNC, EXPECTED_TYPE, OBJNAME, VEC) \
int \
FUNC(visit_handle h, visit_handle obj)\
{\
    int retval = VISIT_ERROR; \
    VisIt_SimulationMetaData *md = GetObject(h, #FUNC); \
    if(md != NULL) \
    { \
        if(simv2_ObjectType(obj) != EXPECTED_TYPE)\
        {\
            VisItError("An attempt was made to add a handle of a type other than " OBJNAME " to SimulationMetaData"); \
            return VISIT_ERROR;\
        }\
        if(std::find(md->VEC.begin(), md->VEC.end(), obj) == md->VEC.end()) \
            md->VEC.push_back(obj);\
        retval = VISIT_OKAY;\
    }\
    return retval; \
}

ADD_METADATA(simv2_SimulationMetaData_addMesh, VISIT_MESHMETADATA, "MeshMetaData", meshes)
ADD_METADATA(simv2_SimulationMetaData_addVariable, VISIT_VARIABLEMETADATA, "VariableMetaData", variables)
ADD_METADATA(simv2_SimulationMetaData_addMaterial, VISIT_MATERIALMETADATA, "MaterialMetaData", materials)
ADD_METADATA(simv2_SimulationMetaData_addCurve, VISIT_CURVEMETADATA, "CurveMetaData", curves)
ADD_METADATA(simv2_SimulationMetaData_addExpression, VISIT_EXPRESSIONMETADATA, "ExpressionMetaData", expressions)
ADD_METADATA(simv2_SimulationMetaData_addSpecies, VISIT_SPECIESMETADATA, "SpeciesMetaData", species)
ADD_METADATA(simv2_SimulationMetaData_addGenericCommand, VISIT_COMMANDMETADATA, "CommandMetaData", genericCommands)
ADD_METADATA(simv2_SimulationMetaData_addCustomCommand, VISIT_COMMANDMETADATA, "CommandMetaData", customCommands)
ADD_METADATA(simv2_SimulationMetaData_addMessage, VISIT_MESSAGEMETADATA, "MessageMetaData", messages)


// C++ code that exists in the runtime that we can use in the SimV2 reader
int
simv2_SimulationMetaData_getData(visit_handle h, int &simulationMode, int &cycle, double &time)
{
    int retval = VISIT_ERROR;
    VisIt_SimulationMetaData *obj = GetObject(h, "simv2_SimulationMetaData_getData");
    if(obj != NULL)
    {
        simulationMode = obj->simulationMode;
        cycle = obj->cycle;
        time = obj->time;
        retval = VISIT_OKAY;
    }
    return retval;
}

#define GET_METADATA_ITEM(FUNC1, FUNC2, VEC) \
int \
FUNC1(visit_handle h, int &n) \
{ \
    int retval = VISIT_ERROR; \
    VisIt_SimulationMetaData *obj = GetObject(h, #FUNC1); \
    if(obj != NULL) \
    { \
        n = obj->VEC.size(); \
        retval = VISIT_OKAY; \
    } \
    return retval; \
} \
int \
FUNC2(visit_handle h, int i, visit_handle &val) \
{ \
    int retval = VISIT_ERROR; \
    VisIt_SimulationMetaData *obj = GetObject(h, #FUNC2); \
    if(obj != NULL) \
    { \
        if(i < 0 || i >= static_cast<int>(obj->VEC.size())) \
        { \
            VisItError("An invalid index was provided"); \
            return VISIT_ERROR; \
        } \
        val = obj->VEC[i]; \
        retval = VISIT_OKAY; \
    } \
    return retval; \
}

GET_METADATA_ITEM(simv2_SimulationMetaData_getNumMeshes, simv2_SimulationMetaData_getMesh, meshes)
GET_METADATA_ITEM(simv2_SimulationMetaData_getNumVariables, simv2_SimulationMetaData_getVariable, variables)
GET_METADATA_ITEM(simv2_SimulationMetaData_getNumMaterials, simv2_SimulationMetaData_getMaterial, materials)
GET_METADATA_ITEM(simv2_SimulationMetaData_getNumCurves, simv2_SimulationMetaData_getCurve, curves)
GET_METADATA_ITEM(simv2_SimulationMetaData_getNumExpressions, simv2_SimulationMetaData_getExpression, expressions)
GET_METADATA_ITEM(simv2_SimulationMetaData_getNumSpecies, simv2_SimulationMetaData_getSpecies, species)
GET_METADATA_ITEM(simv2_SimulationMetaData_getNumGenericCommands, simv2_SimulationMetaData_getGenericCommand, genericCommands)
GET_METADATA_ITEM(simv2_SimulationMetaData_getNumCustomCommands, simv2_SimulationMetaData_getCustomCommand, customCommands)
GET_METADATA_ITEM(simv2_SimulationMetaData_getNumMessages, simv2_SimulationMetaData_getMessage, messages)

int
simv2_SimulationMetaData_check(visit_handle h)
{
    char tmp[128];
    int retval = VISIT_ERROR;
    VisIt_SimulationMetaData *obj = GetObject(h, "simv2_SimulationMetaData_check"); 
    if(obj != NULL) 
    { 
        size_t i;
        retval = VISIT_OKAY;

        // Mesh check
        stringVector meshNames;
        for(i = 0; i < obj->meshes.size(); ++i)
        {
            if(simv2_MeshMetaData_check(obj->meshes[i]) == VISIT_ERROR)
            {
                retval = VISIT_ERROR;
            }
            else
            {
                char *name = NULL;
                if(simv2_MeshMetaData_getName(obj->meshes[i], &name) == VISIT_OKAY)
                {
                    meshNames.push_back(name);
                    free(name);
                }
            }
        }
        // Variable check
        for(i = 0; i < obj->variables.size(); ++i)
        {
            if(simv2_VariableMetaData_check(obj->variables[i]) == VISIT_ERROR)
            {
                retval = VISIT_ERROR;
            }
            else
            {
                // Make sure the variable has a valid mesh name
                char *name = NULL;
                if(simv2_VariableMetaData_getMeshName(obj->variables[i], &name) == VISIT_OKAY)
                {
                    if(std::find(meshNames.begin(), meshNames.end(), name) == meshNames.end())
                    {
                        char *varName;
                        simv2_VariableMetaData_getName(obj->variables[i],
                                                       &varName);
                      
                        snprintf(tmp, 128,
                                 "VariableMetaData for variable: %s: "
                                 " has an unknown mesh: %s",
                                 varName, name);
                        VisItError(tmp);

                        free(varName);
                        
                        retval = VISIT_ERROR;
                    }
                    
                    free(name);
                }
            }
        }
        // Material check
        stringVector matNames;
        for(i = 0; i < obj->materials.size(); ++i)
        {
            if(simv2_MaterialMetaData_check(obj->materials[i]) == VISIT_ERROR)
            {
                retval = VISIT_ERROR;
            }
            else
            {
                char *name = NULL;
                if(simv2_MaterialMetaData_getName(obj->materials[i], &name) == VISIT_OKAY)
                {
                    matNames.push_back(name);
                    free(name);
                }
                // Make sure the material has a valid mesh name
                if(simv2_MaterialMetaData_getMeshName(obj->materials[i], &name) == VISIT_OKAY)
                {
                    matNames.push_back(name);
                    if(std::find(meshNames.begin(), meshNames.end(), name) == meshNames.end())
                    {
                        char *varName;
                        simv2_MaterialMetaData_getName(obj->variables[i],
                                                       &varName);
                      
                        snprintf(tmp, 128,
                                 "MaterialMetaData for material: %s: "
                                 " has an unknown mesh: %s",
                                 varName, name);
                        VisItError(tmp);

                        free(varName);

                        retval = VISIT_ERROR;
                    }
                    
                    free(name);
                }
            }
        }
        // Curve check
        for(i = 0; i < obj->curves.size(); ++i)
        {
            if(simv2_CurveMetaData_check(obj->curves[i]) == VISIT_ERROR)
            {
                retval = VISIT_ERROR;
            }
        }
        // Expression check
        for(i = 0; i < obj->expressions.size(); ++i)
        {
            if(simv2_ExpressionMetaData_check(obj->expressions[i]) == VISIT_ERROR)
            {
                retval = VISIT_ERROR;
            }
        }
        // Species check
        for(i = 0; i < obj->species.size(); ++i)
        {
            if(simv2_SpeciesMetaData_check(obj->species[i]) == VISIT_ERROR)
            {
                retval = VISIT_ERROR;
            }
            else
            {
                // Make sure the species has a valid mesh name
                char *name = NULL;
                if(simv2_SpeciesMetaData_getMeshName(obj->species[i], &name) == VISIT_OKAY)
                {
                    if(std::find(meshNames.begin(), meshNames.end(), name) == meshNames.end())
                    {
                        char *varName;
                        simv2_SpeciesMetaData_getName(obj->species[i],
                                                      &varName);
                      
                        snprintf(tmp, 128,
                                 "SpeciesMetaData for species: %s: "
                                 " has an unknown mesh: %s",
                                 varName, name);
                        VisItError(tmp);

                        free(varName);

                        retval = VISIT_ERROR;
                    }
                    
                    free(name);
                }

                // Make sure the species has a valid material name.
                if(simv2_SpeciesMetaData_getMaterialName(obj->species[i], &name) == VISIT_OKAY)
                {
                    if(std::find(matNames.begin(), matNames.end(), name) == matNames.end())
                    {
                        char *varName;
                        simv2_SpeciesMetaData_getName(obj->species[i],
                                                      &varName);
                      
                        snprintf(tmp, 128,
                                 "SpeciesMetaData for species: %s: "
                                 " has an unknown material: %s",
                                 varName, name);
                        VisItError(tmp);

                        free(varName);

                        retval = VISIT_ERROR;
                    }
                    
                    free(name);
                }
            }
        }
        // Generic command check
        for(i = 0; i < obj->genericCommands.size(); ++i)
        {
            if(simv2_CommandMetaData_check(obj->genericCommands[i]) == VISIT_ERROR)
            {
                retval = VISIT_ERROR;
            }
        }
        // Custom command check
        for(i = 0; i < obj->customCommands.size(); ++i)
        {
            if(simv2_CommandMetaData_check(obj->customCommands[i]) == VISIT_ERROR)
            {
                retval = VISIT_ERROR;
            }
        }
        // Message check
        for(i = 0; i < obj->messages.size(); ++i)
        {
            if(simv2_MessageMetaData_check(obj->messages[i]) == VISIT_ERROR)
            {
                retval = VISIT_ERROR;
            }
        }
    }
    return retval;
}
