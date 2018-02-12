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

#include "global_properties.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include "update.h"
#include "error.h"
#include "neighbor.h"
#include "force.h"

using namespace std;

namespace MODEL_PARAMS
{
  static const char * COHESION_DISTANCE_SETTINGS = "cohesionDistanceSettings";
  static const char * COHESION_ENERGY_DENSITY = "cohesionEnergyDensity";
  static const char * CHARACTERISTIC_VELOCITY = "characteristicVelocity";
  static const char * YOUNGS_MODULUS = "youngsModulus";
  static const char * POISSONS_RATIO = "poissonsRatio";
  static const char * COEFFICIENT_RESTITUTION = "coefficientRestitution";
  static const char * COEFFICIENT_RESTITUTION_LOG = "coefficientRestitutionLog";
  static const char * COEFFICIENT_FRICTION = "coefficientFriction";
  static const char * COEFFICIENT_ROLL_FRICTION = "coefficientRollingFriction";
  static const char * COEFFICIENT_ROLL_VISCOUS_DAMPING = "coefficientRollingViscousDamping";
  static const char * FLUID_VISCOSITY = "FluidViscosity";
  static const char * MAXIMUM_RESTITUTION = "MaximumRestitution";
  static const char * CRITITCAL_STOKES = "CriticalStokes";
  static const char * LIQUID_VOLUME = "liquidVolume";
  static const char * HISTORY_INDEX = "historyIndex";
  static const char * SURFACE_TENSION = "surfaceTension";
  static const char * SWITCH_MODEL = "switchModel";
  static const char * CONTACT_ANGLE = "contactAngle";
  static const char * COEFFICIENT_MAX_ELASTIC_STIFFNESS = "coefficientMaxElasticStiffness";
  static const char * COEFFICIENT_ADHESION_STIFFNESS = "coefficientAdhesionStiffness";
  static const char * COEFFICIENT_PLASTICITY_DEPTH = "coefficientPlasticityDepth";
  static const char * ROUGHNESS_ABSOLUTE = "roughnessAbsolute";
  static const char * ROUGHNESS_RELATIVE = "roughnessRelative";
  static const char * ROLLING_STIFFNESS = "rollingStiffness";

  /* -----------------------------------------------------------------------
   * Utility functions
   * ----------------------------------------------------------------------- */

  ScalarProperty* createScalarProperty(PropertyRegistry & registry, const char* name, const char * caller)
  {
    ScalarProperty * scalar = new ScalarProperty();
    FixPropertyGlobal * property = registry.getGlobalProperty(name,"property/global","scalar",0,0,caller);
    scalar->data = property->compute_scalar();
    return scalar;
  }

  VectorProperty* createPerTypeProperty(PropertyRegistry & registry, const char* name, const char * caller)
  {
    const int max_type = registry.max_type();

    VectorProperty * vector = new VectorProperty(max_type+1);
    FixPropertyGlobal * property = registry.getGlobalProperty(name,"property/global","vector",max_type,0,caller);
    for(int i = 1; i < max_type+1; i++)
        vector->data[i] = property->compute_vector(i-1);

    return vector;
  }

  MatrixProperty* createPerTypePairProperty(PropertyRegistry & registry, const char * name, const char * caller)
  {
    const int max_type = registry.max_type();

    MatrixProperty * matrix = new MatrixProperty(max_type+1, max_type+1);
    FixPropertyGlobal *   property = registry.getGlobalProperty(name,"property/global","peratomtypepair",max_type,max_type,caller);

    for(int i = 1; i < max_type+1; i++)
    {
      for(int j = 1; j < max_type+1; j++)
      {
         matrix->data[i][j] = property->compute_array(i-1,j-1);
      }
    }

    return matrix;
  }

  /* -----------------------------------------------------------------------
   * Property Creators
   * ----------------------------------------------------------------------- */

  ScalarProperty* createCharacteristicVelocity(PropertyRegistry & registry, const char * caller, bool sanity_checks)
  {
    LAMMPS * lmp = registry.getLAMMPS();
    ScalarProperty* charVelScalar = createScalarProperty(registry, CHARACTERISTIC_VELOCITY, caller);
    double charVel = charVelScalar->data;

    if(sanity_checks)
    {
      if(strcmp(lmp->update->unit_style,"si") == 0  && charVel < 1e-2)
        lmp->error->all(FLERR,"characteristicVelocity >= 1e-2 required for SI units");
    }

    return charVelScalar;
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty* createCohesionEnergyDensity(PropertyRegistry & registry, const char * caller, bool sanity_checks)
  {
    return createPerTypePairProperty(registry, COHESION_ENERGY_DENSITY, caller);
  }

  /* ---------------------------------------------------------------------- */

  VectorProperty * createCohesionDistanceSettings(PropertyRegistry & registry, const char * caller, bool sanity_checks)
  {
    LAMMPS * lmp = registry.getLAMMPS();
    int        numSettings = 4;        //use 4 settings, starting index = 0
    VectorProperty *   vec = new VectorProperty(numSettings); 
    FixPropertyGlobal * ca = registry.getGlobalProperty(COHESION_DISTANCE_SETTINGS,"property/global","vector",numSettings,0,caller);

    for(int i=0; i <numSettings; i++)
    {
      const double aSetting = ca->compute_vector(i);

      // error checks on v
      if(sanity_checks)
      {
        if(aSetting < 0.0)
          lmp->error->all(FLERR,"distance settings for cohesion model must be all positive");
      }

      vec->data[i] = aSetting;
    }

    return vec;
  }

  /* ---------------------------------------------------------------------- */

  VectorProperty * createYoungsModulus(PropertyRegistry & registry, const char * caller, bool sanity_checks)
  {
    LAMMPS * lmp = registry.getLAMMPS();
    const int max_type = registry.max_type();

    VectorProperty * vec = new VectorProperty(max_type+1);
    FixPropertyGlobal * Y = registry.getGlobalProperty(YOUNGS_MODULUS,"property/global","peratomtype",max_type,0,caller);

    for(int i=1; i < max_type+1; i++)
    {
      const double Yi = Y->compute_vector(i-1);

      // error checks on Y
      if(sanity_checks)
      {
        if(strcmp(lmp->update->unit_style,"si") == 0  && Yi < 5e6)
          lmp->error->all(FLERR,"youngsModulus >= 5e6 required for SI units");
        if(strcmp(lmp->update->unit_style,"cgs") == 0 && Yi < 5e5)
          lmp->error->all(FLERR,"youngsModulus >= 5e5 required for CGS units");
      }

      vec->data[i] = Yi;
    }

    return vec;
  }

  /* ---------------------------------------------------------------------- */

  VectorProperty * createPoissonsRatio(PropertyRegistry & registry, const char * caller, bool sanity_checks)
  {
    LAMMPS * lmp = registry.getLAMMPS();
    const int max_type = registry.max_type();

    VectorProperty * vec = new VectorProperty(max_type+1);
    FixPropertyGlobal * v = registry.getGlobalProperty(POISSONS_RATIO,"property/global","peratomtype",max_type,0,caller);

    for(int i=1; i < max_type+1; i++)
    {
      const double vi = v->compute_vector(i-1);

      // error checks on v
      if(sanity_checks)
      {
        if(vi < 0. || vi > 0.5)
          lmp->error->all(FLERR,"0 <= poissonsRatio <= 0.5 required");
      }

      vec->data[i] = vi;
    }

    return vec;
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty * createYeff(PropertyRegistry & registry, const char * caller, bool)
  {
    const int max_type = registry.max_type();

    registry.registerProperty(YOUNGS_MODULUS, &createYoungsModulus);
    registry.registerProperty(POISSONS_RATIO, &createPoissonsRatio);

    MatrixProperty * matrix = new MatrixProperty(max_type+1, max_type+1);
    VectorProperty * youngsModulus = registry.getVectorProperty(YOUNGS_MODULUS,caller);
    VectorProperty * poissonRatio = registry.getVectorProperty(POISSONS_RATIO,caller);
    double * Y = youngsModulus->data;
    double * v = poissonRatio->data;

    for(int i=1;i< max_type+1; i++)
    {
      for(int j=1;j<max_type+1;j++)
      {
        const double Yi=Y[i];
        const double Yj=Y[j];
        const double vi=v[i];
        const double vj=v[j];
        matrix->data[i][j] = 1./((1.-pow(vi,2.))/Yi+(1.-pow(vj,2.))/Yj);
      }
    }

    return matrix;
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty * createGeff(PropertyRegistry & registry, const char * caller, bool)
  {
    const int max_type = registry.max_type();

    registry.registerProperty(YOUNGS_MODULUS, &createYoungsModulus);
    registry.registerProperty(POISSONS_RATIO, &createPoissonsRatio);

    MatrixProperty * matrix = new MatrixProperty(max_type+1, max_type+1);
    VectorProperty * youngsModulus = registry.getVectorProperty(YOUNGS_MODULUS,caller);
    VectorProperty * poissonRatio = registry.getVectorProperty(POISSONS_RATIO,caller);
    double * Y = youngsModulus->data;
    double * v = poissonRatio->data;

    for(int i=1;i< max_type+1; i++)
    {
      for(int j=1;j<max_type+1;j++)
      {
        const double Yi=Y[i];
        const double Yj=Y[j];
        const double vi=v[i];
        const double vj=v[j];

        matrix->data[i][j] = 1./(2.*(2.-vi)*(1.+vi)/Yi+2.*(2.-vj)*(1.+vj)/Yj);
      }
    }

    return matrix;
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty * createCoeffRest(PropertyRegistry & registry, const char * caller, bool sanity_checks)
  {
     LAMMPS * lmp = registry.getLAMMPS();
     const int max_type = registry.max_type();

     MatrixProperty * matrix = new MatrixProperty(max_type+1, max_type+1);
     FixPropertyGlobal * coeffRest = registry.getGlobalProperty(COEFFICIENT_RESTITUTION,"property/global","peratomtypepair",max_type,max_type,caller);

     for(int i=1;i< max_type+1; i++)
     {
       for(int j=1;j<max_type+1;j++)
       {
         const double cor = coeffRest->compute_array(i-1,j-1);

         if(sanity_checks)
         {
           if(cor <= 0.05 || cor > 1) {
             lmp->error->all(FLERR,"0.05 < coefficientRestitution <= 1 required");
           }
         }

         matrix->data[i][j] = cor;
       }
     }

     return matrix;
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty * createCoeffRestLog(PropertyRegistry & registry, const char * caller, bool)
  {
    const int max_type = registry.max_type();

    registry.registerProperty(COEFFICIENT_RESTITUTION, &createCoeffRest);

    MatrixProperty * matrix = new MatrixProperty(max_type+1, max_type+1);
    MatrixProperty * coeffRestProp = registry.getMatrixProperty(COEFFICIENT_RESTITUTION,caller);
    double ** coeffRest = coeffRestProp->data;

    for(int i=1;i< max_type+1; i++)
    {
      for(int j=1;j<max_type+1;j++)
      {
        matrix->data[i][j] = log(coeffRest[i][j]);
      }
    }

    return matrix;
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty * createBetaEff(PropertyRegistry & registry, const char * caller, bool)
  {
    const int max_type = registry.max_type();

    registry.registerProperty(COEFFICIENT_RESTITUTION_LOG, &createCoeffRestLog);

    MatrixProperty * matrix = new MatrixProperty(max_type+1, max_type+1);
    MatrixProperty * coeffRestLogProp = registry.getMatrixProperty(COEFFICIENT_RESTITUTION_LOG,caller);
    double ** coeffRestLog = coeffRestLogProp->data;

    for(int i=1;i< max_type+1; i++)
    {
      for(int j=1;j<max_type+1;j++)
      {
        matrix->data[i][j] = coeffRestLog[i][j] / sqrt(pow(coeffRestLog[i][j],2.)+pow(M_PI,2.));
      }
    }

    return matrix;
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty* createCoeffFrict(PropertyRegistry & registry, const char * caller, bool)
  {
    return createPerTypePairProperty(registry, COEFFICIENT_FRICTION, caller);
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty* createCoeffRollFrict(PropertyRegistry & registry, const char * caller, bool)
  {
    return createPerTypePairProperty(registry, COEFFICIENT_ROLL_FRICTION, caller);
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty* createCoeffRollVisc(PropertyRegistry & registry, const char * caller, bool)
  {
    return createPerTypePairProperty(registry, COEFFICIENT_ROLL_VISCOUS_DAMPING, caller);
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty * createCoeffMu(PropertyRegistry & registry, const char * caller, bool sanity_checks)
  {
    LAMMPS * lmp = registry.getLAMMPS();
    const int max_type = registry.max_type();

    MatrixProperty * matrix = new MatrixProperty(max_type+1, max_type+1);
    FixPropertyGlobal * coeffMu = registry.getGlobalProperty(FLUID_VISCOSITY,"property/global","peratomtypepair",max_type,max_type,caller);

    for(int i=1;i< max_type+1; i++)
    {
     for(int j=1;j<max_type+1;j++)
     {
       const double mu = coeffMu->compute_array(i-1,j-1);

       if(sanity_checks)
       {
         if(mu <= 0.)
          lmp->error->all(FLERR,"coeffMu > 0 required");
       }

       matrix->data[i][j] = mu;
     }
    }

    return matrix;
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty * createCoeffRestMax(PropertyRegistry & registry, const char * caller, bool sanity_checks)
  {
   LAMMPS * lmp = registry.getLAMMPS();
   const int max_type = registry.max_type();

   MatrixProperty * matrix = new MatrixProperty(max_type+1, max_type+1);
   FixPropertyGlobal * coeffRestMax = registry.getGlobalProperty(MAXIMUM_RESTITUTION,"property/global","peratomtypepair",max_type,max_type,caller);

   for(int i=1;i< max_type+1; i++)
   {
     for(int j=1;j<max_type+1;j++)
     {
       const double restMax = coeffRestMax->compute_array(i-1,j-1);

       if(sanity_checks)
       {
         if(restMax <= 0. || restMax > 1.0)
           lmp->error->all(FLERR,"0 < MaximumRestitution <= 1 required");
       }

       matrix->data[i][j] = restMax;
     }
   }

   return matrix;
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty * createCoeffStc(PropertyRegistry & registry, const char * caller, bool sanity_checks)
  {
   LAMMPS * lmp = registry.getLAMMPS();
   const int max_type = registry.max_type();

   MatrixProperty * matrix = new MatrixProperty(max_type+1, max_type+1);
   FixPropertyGlobal * coeffStc = registry.getGlobalProperty(CRITITCAL_STOKES,"property/global","peratomtypepair",max_type,max_type,caller);

   for(int i=1;i< max_type+1; i++)
   {
     for(int j=1;j<max_type+1;j++)
     {
       const double stc= coeffStc->compute_array(i-1,j-1);

       if(sanity_checks)
       {
         if(stc <= 0.)
           lmp->error->all(FLERR,"CriticalStokes > 0 required");
       }

       matrix->data[i][j] = stc;
     }
   }

   return matrix;
  }

  /* ---------------------------------------------------------------------- */

  ScalarProperty* createRollingStiffness(PropertyRegistry & registry, const char * caller, bool sanity_checks)
  {
    LAMMPS * lmp = registry.getLAMMPS();
    ScalarProperty* rollingStiffness = createScalarProperty(registry, ROLLING_STIFFNESS, caller);
    double rollStiffness = rollingStiffness->data;

    if(sanity_checks)
    {
      if(strcmp(lmp->update->unit_style,"si") == 0  && rollStiffness < 1.0 )
        lmp->error->all(FLERR,"rollingStiffness >= 1 required for SI units");
    }

    return rollingStiffness;
  }

  /* ---------------------------------------------------------------------- */

  ScalarProperty* createLiquidVolume(PropertyRegistry & registry, const char * caller, bool)
  {
    return createScalarProperty(registry, LIQUID_VOLUME, caller);
  }

  /* ---------------------------------------------------------------------- */

  ScalarProperty* createSurfaceTension(PropertyRegistry & registry, const char * caller, bool)
  {
    return createScalarProperty(registry, SURFACE_TENSION, caller);
  }

  /* ---------------------------------------------------------------------- */

  ScalarProperty* createSwitchModel(PropertyRegistry & registry, const char * caller, bool)
  {
    return createScalarProperty(registry, SWITCH_MODEL, caller);
  }

  /* ---------------------------------------------------------------------- */

  ScalarProperty* createHistoryIndex(PropertyRegistry & registry, const char * caller, bool)
  {
    return createScalarProperty(registry, HISTORY_INDEX, caller);
  }

  /* ---------------------------------------------------------------------- */

  VectorProperty * createContactAngle(PropertyRegistry & registry, const char * caller, bool sanity_checks)
  {
    LAMMPS * lmp = registry.getLAMMPS();
    const int max_type = registry.max_type();

    VectorProperty * vec = new VectorProperty(max_type+1);
    FixPropertyGlobal * ca = registry.getGlobalProperty(CONTACT_ANGLE,"property/global","peratomtype",max_type,0,caller);

    for(int i=1; i < max_type+1; i++)
    {
      const double vi = ca->compute_vector(i-1);

      // error checks on v
      if(sanity_checks)
      {
        if(vi < 0. || vi > 180.)
          lmp->error->all(FLERR,"0 <= contactAngle <= 180° required");
      }

      vec->data[i] = vi * M_PI / 180.; // from grad to rad
    }

    return vec;
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty* createKn(PropertyRegistry & registry, const char * caller, bool)
  {
    LAMMPS * lmp = registry.getLAMMPS();
    const int max_type = registry.max_type();

    MatrixProperty * matrix = new MatrixProperty(max_type+1, max_type+1);
    FixPropertyGlobal * k_n1 = registry.getGlobalProperty("kn","property/global","peratomtypepair",max_type,max_type,caller);
    const double cg = lmp->force->cg();

    for(int i=1;i< max_type+1; i++)
    {
     for(int j=1;j<max_type+1;j++)
     {
       const double k_n = cg*k_n1->compute_array(i-1,j-1);
       matrix->data[i][j] = k_n;
     }
    }

    return matrix;
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty* createKt(PropertyRegistry & registry, const char * caller, bool)
  {
    return createPerTypePairProperty(registry, "kt", caller);
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty* createGamman(PropertyRegistry & registry, const char * caller, bool)
  {
    LAMMPS * lmp = registry.getLAMMPS();
    const int max_type = registry.max_type();

    MatrixProperty * matrix = new MatrixProperty(max_type+1, max_type+1);
    FixPropertyGlobal * gamma_n1 = registry.getGlobalProperty("gamman","property/global","peratomtypepair",max_type,max_type,caller);
    const double cg = lmp->force->cg();

    for(int i=1;i< max_type+1; i++)
    {
      for(int j=1;j<max_type+1;j++)
      {
        const double k_n = (1./cg)*gamma_n1->compute_array(i-1,j-1);
        matrix->data[i][j] = k_n;
      }
    }

    return matrix;
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty* createGammat(PropertyRegistry & registry, const char * caller, bool)
  {
    return createPerTypePairProperty(registry, "gammat", caller);
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty* createGammanAbs(PropertyRegistry & registry, const char * caller, bool)
  {
    LAMMPS * lmp = registry.getLAMMPS();
    const int max_type = registry.max_type();

    MatrixProperty * matrix = new MatrixProperty(max_type+1, max_type+1);
    FixPropertyGlobal * gamma_n1 = registry.getGlobalProperty("gamman_abs","property/global","peratomtypepair",max_type,max_type,caller);
    const double cg = lmp->force->cg();

    for(int i=1;i< max_type+1; i++)
    {
      for(int j=1;j<max_type+1;j++)
      {
        const double k_n = cg*cg*gamma_n1->compute_array(i-1,j-1);
        matrix->data[i][j] = k_n;
      }
    }

    return matrix;
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty* createGammatAbs(PropertyRegistry & registry, const char * caller, bool)
  {
    return createPerTypePairProperty(registry, "gammat_abs", caller);
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty* createCoeffMaxElasticStiffness(PropertyRegistry & registry, const char * caller, bool)
  {
    return createPerTypePairProperty(registry, COEFFICIENT_MAX_ELASTIC_STIFFNESS, caller);
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty* createCoeffAdhesionStiffness(PropertyRegistry & registry, const char * caller, bool)
  {
    return createPerTypePairProperty(registry, COEFFICIENT_ADHESION_STIFFNESS, caller);
  }

  /* ---------------------------------------------------------------------- */

  MatrixProperty* createCoeffPlasticityDepth(PropertyRegistry & registry, const char * caller, bool)
  {
    return createPerTypePairProperty(registry, COEFFICIENT_PLASTICITY_DEPTH, caller);
  }

  /* ---------------------------------------------------------------------- */

  ScalarProperty* createRoughnessAbsolute(PropertyRegistry & registry, const char * caller, bool)
  {
    return createScalarProperty(registry, ROUGHNESS_ABSOLUTE, caller);
  }

  /* ---------------------------------------------------------------------- */

  ScalarProperty* createRoughnessRelative(PropertyRegistry & registry, const char * caller, bool)
  {
    return createScalarProperty(registry, ROUGHNESS_RELATIVE, caller);
  }
  /* ---------------------------------------------------------------------- */
}
