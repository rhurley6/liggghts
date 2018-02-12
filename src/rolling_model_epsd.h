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

    Andreas Aigner (JKU Linz)
    Christoph Kloss (DCS Computing GmbH, Linz)
    Christoph Kloss (JKU Linz)
    Richard Berger (JKU Linz)

    Copyright 2012-     DCS Computing GmbH, Linz
    Copyright 2009-2012 JKU Linz
------------------------------------------------------------------------- */

#ifdef ROLLING_MODEL
ROLLING_MODEL(ROLLING_EPSD,epsd,2)
#else
#ifndef ROLLING_MODEL_EPSD_H_
#define ROLLING_MODEL_EPSD_H_
#include "contact_models.h"
#include <algorithm>
#include "math.h"
#include "domain.h"
#include "math_extra_liggghts.h"

namespace LIGGGHTS {
namespace ContactModels
{
  using namespace LAMMPS_NS;

  template<>
  class RollingModel<ROLLING_EPSD> : protected Pointers
  {
  public:
    static const int MASK = CM_CONNECT_TO_PROPERTIES | CM_SURFACES_INTERSECT | CM_SURFACES_CLOSE;

    RollingModel(class LAMMPS * lmp, IContactHistorySetup * hsetup,class ContactModelBase *) :
        Pointers(lmp), coeffRollFrict(NULL), coeffRollVisc(NULL)
    {
      history_offset = hsetup->add_history_value("r_torquex_old", "1");
      hsetup->add_history_value("r_torquey_old", "1");
      hsetup->add_history_value("r_torquez_old", "1");
      
    }

    void registerSettings(Settings&) {}

    void connectToProperties(PropertyRegistry & registry) {
      registry.registerProperty("coeffRollFrict", &MODEL_PARAMS::createCoeffRollFrict);
      registry.registerProperty("coeffRollVisc", &MODEL_PARAMS::createCoeffRollVisc);
      registry.connect("coeffRollFrict", coeffRollFrict,"rolling_model epsd");
      registry.connect("coeffRollVisc", coeffRollVisc,"rolling_model epsd");

      // error checks on coarsegraining
      if(force->cg_active())
        error->cg(FLERR,"rolling model epsd");
    }

    void surfacesIntersect(SurfacesIntersectData & sidata, ForceData & i_forces, ForceData & j_forces) 
    {
      double r_torque[3];
      vectorZeroize3D(r_torque);

      if(sidata.contact_flags) *sidata.contact_flags |= CONTACT_ROLLING_MODEL;

      double radi = sidata.radi;
      double radj = sidata.radj;
      double reff=sidata.is_wall ? sidata.radi : (radi*radj/(radi+radj));

#ifdef SUPERQUADRIC_ACTIVE_FLAG
      if(sidata.is_non_spherical)
        reff = MathExtraLiggghtsSuperquadric::get_effective_radius(sidata);
#endif
      if(sidata.is_wall) {
        const double wr1 = sidata.wr1;
        const double wr2 = sidata.wr2;
        const double wr3 = sidata.wr3;

        double r_inertia = 0.0; //pre-initialize to prevent compiler "warning"

#ifdef SUPERQUADRIC_ACTIVE_FLAG
        if(sidata.is_non_spherical) {
          const double rii = pointDistance(sidata.contact_point, sidata.pos_i);
          const double omega_mag = sqrt(wr1*wr1 + wr2*wr2 + wr3*wr3);
          if(omega_mag != 0.0) {
            double er[3];
            er[0] = wr1 / omega_mag;
            er[1] = wr2 / omega_mag;
            er[2] = wr3 / omega_mag;
            const double Ix = sidata.inertia_i[0];
            const double Iy = sidata.inertia_i[1];
            const double Iz = sidata.inertia_i[2];
            double inertia_tensor[3][3];
            double inertia_tensor_local[3][3] = { {Ix, 0.0, 0.0},
                                                  {0.0, Iy, 0.0},
                                                  {0.0, 0.0, Iz} };
            MathExtraLiggghtsSuperquadric::tensor_quat_rotate(inertia_tensor_local, sidata.quat_i, inertia_tensor);
            double temp[3];
            MathExtra::matvec(inertia_tensor, er, temp);
            double Ii = MathExtra::dot3(temp, er);
            r_inertia = Ii + sidata.mi*rii*rii;
          }
        } else {
          if (domain->dimension == 2) r_inertia = 1.5*sidata.mi*reff*reff;
          else  r_inertia = 1.4*sidata.mi*reff*reff;
        }
#else

        if (domain->dimension == 2) r_inertia = 1.5*sidata.mi*reff*reff;
        else  r_inertia = 1.4*sidata.mi*reff*reff;
#endif

        calcRollTorque(r_torque,sidata,reff,wr1,wr2,wr3,r_inertia);

      } else {

        double  wr_roll[3];

        const int i = sidata.i;
        const int j = sidata.j;

        const double * const * const omega = atom->omega;

        // relative rotational velocity
        vectorSubtract3D(omega[i],omega[j],wr_roll);
        double r_inertia = 0.0; //pre-initialize to prevent compiler "warning"
        double r_inertia_red_i, r_inertia_red_j;

#ifdef SUPERQUADRIC_ACTIVE_FLAG
        if(sidata.is_non_spherical) {
          const double rii = pointDistance(sidata.contact_point, sidata.pos_i);
          const double rjj = pointDistance(sidata.contact_point, sidata.pos_j);
          const double omega_mag = vectorMag3D(wr_roll);
          if(omega_mag != 0.0) {
            double er[3];
            er[0] = wr_roll[0] / omega_mag;
            er[1] = wr_roll[1] / omega_mag;
            er[2] = wr_roll[2] / omega_mag;
            const double Ix_i = sidata.inertia_i[0];
            const double Iy_i = sidata.inertia_i[1];
            const double Iz_i = sidata.inertia_i[2];

            const double Ix_j = sidata.inertia_j[0];
            const double Iy_j = sidata.inertia_j[1];
            const double Iz_j = sidata.inertia_j[2];

            double inertia_tensor_i[3][3];
            double inertia_tensor_local_i[3][3] = { {Ix_i, 0.0, 0.0},
                                                    {0.0, Iy_i, 0.0},
                                                    {0.0, 0.0, Iz_i} };
            double inertia_tensor_j[3][3];
            double inertia_tensor_local_j[3][3] = { {Ix_j, 0.0, 0.0},
                                                    {0.0, Iy_j, 0.0},
                                                    {0.0, 0.0, Iz_j} };
            MathExtraLiggghtsSuperquadric::tensor_quat_rotate(inertia_tensor_local_i, sidata.quat_i, inertia_tensor_i);
            MathExtraLiggghtsSuperquadric::tensor_quat_rotate(inertia_tensor_local_j, sidata.quat_j, inertia_tensor_j);
            double temp[3];
            MathExtra::matvec(inertia_tensor_i, er, temp);
            double Ii = MathExtra::dot3(temp, er);
            MathExtra::matvec(inertia_tensor_j, er, temp);
            double Ij = MathExtra::dot3(temp, er);
            r_inertia_red_i = Ii + sidata.mi*rii*rii; //
            r_inertia_red_j = Ij + sidata.mj*rjj*rjj;
            r_inertia = r_inertia_red_i*r_inertia_red_j / (r_inertia_red_i + r_inertia_red_j);
          }

        } else {
          r_inertia_red_i = sidata.mi*radi*radi;
          r_inertia_red_j= sidata.mj*radj*radj;
        if (domain->dimension == 2) r_inertia = 1.5 * r_inertia_red_i * r_inertia_red_j/(r_inertia_red_i + r_inertia_red_j);
        else  r_inertia = 1.4 * r_inertia_red_i * r_inertia_red_j/(r_inertia_red_i + r_inertia_red_j);
        }
#else
        r_inertia_red_i = sidata.mi*radi*radi;
        r_inertia_red_j= sidata.mj*radj*radj;
        if (domain->dimension == 2) r_inertia = 1.5 * r_inertia_red_i * r_inertia_red_j/(r_inertia_red_i + r_inertia_red_j);
        else  r_inertia = 1.4 * r_inertia_red_i * r_inertia_red_j/(r_inertia_red_i + r_inertia_red_j);
#endif

        calcRollTorque(r_torque,sidata,reff,wr_roll[0],wr_roll[1],wr_roll[2],r_inertia);
      }

      i_forces.delta_torque[0] -= r_torque[0];
      i_forces.delta_torque[1] -= r_torque[1];
      i_forces.delta_torque[2] -= r_torque[2];
      j_forces.delta_torque[0] += r_torque[0];
      j_forces.delta_torque[1] += r_torque[1];
      j_forces.delta_torque[2] += r_torque[2];
    }

    void surfacesClose(SurfacesCloseData & scdata, ForceData&, ForceData&)
    {
      if(scdata.contact_flags) *scdata.contact_flags &= ~CONTACT_ROLLING_MODEL;
      double * const c_history = &scdata.contact_history[history_offset];
      c_history[0] = 0.0; // this is the r_torque_old
      c_history[1] = 0.0; // this is the r_torque_old
      c_history[2] = 0.0; // this is the r_torque_old
    }

    void beginPass(SurfacesIntersectData&, ForceData&, ForceData&){}
    void endPass(SurfacesIntersectData&, ForceData&, ForceData&){}

  private:
    double ** coeffRollFrict;
    double ** coeffRollVisc;
    int history_offset;

    inline void calcRollTorque(double (&r_torque)[3],const SurfacesIntersectData & sidata,double reff,double wr1,double wr2,double wr3,double r_inertia) {

      double wr_n[3],wr_t[3],dr_torque[3];

      const int itype = sidata.itype;
      const int jtype = sidata.jtype;

      const double enx = sidata.en[0];
      const double eny = sidata.en[1];
      const double enz = sidata.en[2];

      const double dt = update->dt; 

      double * const c_history = &sidata.contact_history[history_offset]; // requires Style::TANGENTIAL == TANGENTIAL_HISTORY
      const double rmu= coeffRollFrict[itype][jtype];

      // remove normal (torsion) part of relative rotation
      // use only tangential parts for rolling torque
      const double wr_dot_delta = wr1*enx+ wr2*eny + wr3*enz;
      wr_n[0] = enx * wr_dot_delta;
      wr_n[1] = eny * wr_dot_delta;
      wr_n[2] = enz * wr_dot_delta;
      wr_t[0] = wr1 - wr_n[0];
      wr_t[1] = wr2 - wr_n[1];
      wr_t[2] = wr3 - wr_n[2];

      // spring
      const double kr = 2.25*sidata.kn*rmu*rmu*reff*reff; 

      vectorScalarMult3D(wr_t,dt*kr,dr_torque);

      r_torque[0] = c_history[0] + dr_torque[0];
      r_torque[1] = c_history[1] + dr_torque[1];
      r_torque[2] = c_history[2] + dr_torque[2];

      // limit max. torque
      const double r_torque_mag = vectorMag3D(r_torque);
      const double r_torque_max = fabs(sidata.Fn)*reff*rmu;
      if(r_torque_mag > r_torque_max)
      {
        //printf("[%d] %e > %e\n", update->ntimestep, r_torque_mag, r_torque_max);
        const double factor = r_torque_max / r_torque_mag;

        r_torque[0] *= factor;
        r_torque[1] *= factor;
        r_torque[2] *= factor;

        // save rolling torque due to spring
        c_history[0] = r_torque[0];
        c_history[1] = r_torque[1];
        c_history[2] = r_torque[2];

        // no damping / no dashpot in case of full mobilisation rolling angle

      } else {
        // save rolling torque due to spring before adding damping torque
        c_history[0] = r_torque[0];
        c_history[1] = r_torque[1];
        c_history[2] = r_torque[2];

        // dashpot
        
        const double r_coef = coeffRollVisc[itype][jtype] * 2 * sqrt(r_inertia*kr);

        // add damping torque
        r_torque[0] += r_coef*wr_t[0];
        r_torque[1] += r_coef*wr_t[1];
        r_torque[2] += r_coef*wr_t[2];
      }
    }
  };
}
}
#endif // ROLLING_MODEL_EPSD_H_
#endif
