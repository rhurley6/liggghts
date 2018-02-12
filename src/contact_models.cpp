/* ----------------------------------------------------------------------
    This is the

    ██╗     ██╗ ██████╗  ██████╗  ██████╗ ██╗  ██╗████████╗███████╗
    ██║     ██║██╔════╝ ██╔════╝ ██╔════╝ ██║  ██║╚══██╔══╝██╔════╝
    ██║     ██║██║  ███╗██║  ███╗██║  ███╗███████║   ██║   ███████╗
    ██║     ██║██║   ██║██║   ██║██║   ██║██╔══██║   ██║   ╚════██║
    ███████╗██║╚██████╔╝╚██████╔╝╚██████╔╝██║  ██║   ██║   ███████║
    ╚══════╝╚═╝ ╚═════╝  ╚═════╝  ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚══════╝®

    DEM simulation engine, released by
    DCS Computing Gmbh, Linz, Austria
    http://www.dcs-computing.com, office@dcs-computing.com

    LIGGGHTS® is part of CFDEM®project:
    http://www.liggghts.com | http://www.cfdem.com

    Core developer and main author:
    Christoph Kloss, christoph.kloss@dcs-computing.com

    LIGGGHTS® is open-source, distributed under the terms of the GNU Public
    License, version 2 or later. It is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You should have
    received a copy of the GNU General Public License along with LIGGGHTS®.
    If not, see http://www.gnu.org/licenses . See also top-level README
    and LICENSE files.

    LIGGGHTS® and CFDEM® are registered trade marks of DCS Computing GmbH,
    the producer of the LIGGGHTS® software and the CFDEM®coupling software
    See http://www.cfdem.com/terms-trademark-policy for details.

-------------------------------------------------------------------------
    Contributing author and copyright for this file:

    Christoph Kloss (DCS Computing GmbH, Linz)
    Christoph Kloss (JKU Linz)
    Richard Berger (JKU Linz)

    Copyright 2012-     DCS Computing GmbH, Linz
    Copyright 2009-2012 JKU Linz
------------------------------------------------------------------------- */

#include "mpi.h"
#include "ctype.h"
#include "float.h"
#include "limits.h"
#include "math.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pair.h"
#include "atom.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "domain.h"
#include "comm.h"
#include "force.h"
#include "kspace.h"
#include "update.h"
#include "accelerator_cuda.h"
#include "suffix.h"
#include "atom_masks.h"
#include "memory.h"
#include "error.h"

#include <string>
#include <iostream>
#include "contact_models.h"

namespace LIGGGHTS {
namespace ContactModels {

int64_t generate_gran_hashcode(int model, int tangential, int cohesion, int rolling, int surface)
{
   return (((int64_t)model)) |
       (((int64_t)tangential) << 4) |
       (((int64_t)cohesion) << 8) |
       (((int64_t)rolling) << 12) |
       (((int64_t)surface) << 16);
 }

Factory::Factory() {
  // register contact model string to contact mappings
  #define SURFACE_MODEL(identifier,str,constant) \
  addSurfaceModel(#str, identifier);
  #include "style_surface_model.h"
  #undef SURFACE_MODEL

  #define NORMAL_MODEL(identifier,str,constant) \
  addNormalModel(#str, identifier);
  #include "style_normal_model.h"
  #undef NORMAL_MODEL

  addTangentialModel("off", TANGENTIAL_OFF);
  #define TANGENTIAL_MODEL(identifier,str,constant) \
  addTangentialModel(#str, identifier);
  #include "style_tangential_model.h"
  #undef TANGENTIAL_MODEL

  addCohesionModel("off", COHESION_OFF);
  #define COHESION_MODEL(identifier,str,constant) \
  addCohesionModel(#str, identifier);
  #include "style_cohesion_model.h"
  #undef COHESION_MODEL

  addRollingModel("off", ROLLING_OFF);
  #define ROLLING_MODEL(identifier,str,constant) \
  addRollingModel(#str, identifier);
  #include "style_rolling_model.h"
  #undef ROLLING_MODEL
}

Factory & Factory::instance() {
    static Factory _instance;
    return _instance;
}

void Factory::addNormalModel(const std::string & name, int identifier) {
  normal_models[name] = identifier;
}

void Factory::addTangentialModel(const std::string & name, int identifier){
  tangential_models[name] = identifier;
}

void Factory::addCohesionModel(const std::string & name, int identifier) {
  cohesion_models[name] = identifier;
}

void Factory::addRollingModel(const std::string & name, int identifier) {
  rolling_models[name] = identifier;
}

void Factory::addSurfaceModel(const std::string & name, int identifier) {
  surface_models[name] = identifier;
}

int64_t Factory::select(int & narg, char ** & args) {
  return instance().select_model(narg, args);
}

int64_t Factory::select_model(int & narg, char ** & args)
{
  // this method will consume arguments to determine which granular contact model is active

  // default configuration
  int model = -1;
  int tangential = TANGENTIAL_OFF;
  int cohesion = COHESION_OFF;
  int rolling = ROLLING_OFF;
  int surface = SURFACE_DEFAULT;

  // select normal model
  if (narg > 1 && strcmp(args[0], "model") == 0) {
    if (normal_models.find(args[1]) != normal_models.end()) {
      model = normal_models[args[1]];
    }

    if(narg > 2) args = &args[2];
    narg -= 2;
  }

  // OPTIONAL: select tangential model
  if (narg > 1 && strcmp(args[0], "tangential") == 0) {
    if (tangential_models.find(args[1]) != tangential_models.end()) {
      tangential = tangential_models[args[1]];
    } else {
      tangential = -1;
    }

    if(narg > 2) args = &args[2];
    narg -= 2;
  }

  // OPTIONAL: select cohesion model
  if (narg > 1 && strcmp(args[0], "cohesion") == 0) {
    if (cohesion_models.find(args[1]) != cohesion_models.end()) {
      cohesion = cohesion_models[args[1]];
    } else {
      cohesion = -1;
    }

    if(narg > 2) args = &args[2];
    narg -= 2;
  }

  // OPTIONAL: select rolling model
  if (narg > 1 && strcmp(args[0], "rolling_friction") == 0) {
    if (rolling_models.find(args[1]) != rolling_models.end()) {
      rolling = rolling_models[args[1]];
    } else {
      rolling = -1;
    }

    if(narg > 2) args = &args[2];
    narg -= 2;
  }

  // OPTIONAL: select surface model
  if (narg > 1 && strcmp(args[0], "surface") == 0) {
    if (surface_models.find(args[1]) != surface_models.end()) {
      surface = surface_models[args[1]];
    } else {
      surface = -1;
    }

    if(narg > 2) args = &args[2];
    narg -= 2;
  }

  if(model != -1 && tangential != -1 && cohesion != -1 && rolling != -1 && surface != -1) {
    return generate_gran_hashcode(model, tangential, cohesion, rolling, surface);
  }
  return -1;
}

} // ContactModels
} // LIGGGHTS
