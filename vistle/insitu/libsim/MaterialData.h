// Copyright (c) Lawrence Livermore National Security, LLC and other VisIt
// Project developers.  See the top-level LICENSE file for dates and other
// details.  No copyright assignment is required to contribute to VisIt.

#ifndef SIMV2_MaterialData_H
#define SIMV2_MaterialData_H
#include "export.h"
#include "VisItDataTypes.h"

// C-callable implementation of front end functions
#ifdef __cplusplus
extern "C"
{
#endif

V_VISITXPORT int simv2_MaterialData_alloc(visit_handle*);
V_VISITXPORT int simv2_MaterialData_free(visit_handle);

V_VISITXPORT int simv2_MaterialData_addMaterial(visit_handle obj,
                                             const char *matName, int *matno);

// Add materials 1 cell at a time...
V_VISITXPORT int simv2_MaterialData_appendCells(visit_handle h, int ncells);

V_VISITXPORT int simv2_MaterialData_addCleanCell(visit_handle h, int cell, int matno);

V_VISITXPORT int simv2_MaterialData_addMixedCell(visit_handle h, int cell, 
                  const int *matnos, const float *mixvf, int nmats);

// Or add materials all at once
V_VISITXPORT int simv2_MaterialData_setMaterials(visit_handle obj, 
                  visit_handle matlist);

V_VISITXPORT int simv2_MaterialData_setMixedMaterials(visit_handle obj,
                  visit_handle mix_mat, visit_handle mix_zone, 
                  visit_handle mix_next, visit_handle mix_vf);

#ifdef __cplusplus
}
#endif

// Callable from within the runtime and SimV2
V_VISITXPORT int simv2_MaterialData_getNumMaterials(visit_handle h, int &nmat);
V_VISITXPORT int simv2_MaterialData_getMaterial(visit_handle h, int i,
                  int &matno, char *matname, int maxlen);

V_VISITXPORT int simv2_MaterialData_getMaterials(visit_handle h, 
                  visit_handle &matlist);

V_VISITXPORT int simv2_MaterialData_getMixedMaterials(visit_handle h,
                  visit_handle &mix_mat, visit_handle &mix_zone, 
                  visit_handle &mix_next, visit_handle &mix_vf);

V_VISITXPORT int simv2_MaterialData_check(visit_handle h);

#endif
