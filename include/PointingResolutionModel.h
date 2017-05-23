#ifndef UCORRELATOR_RESOLUTION_MODEL_H
#define UCORRELATOR_RESOLUTION_MODEL_H

#include "AnitaEventSummary.h"

namespace UCorrelator
{

  class PointingResolution 
  {
    public: 

      PointingResolution(double phi, double theta, double dphi, double dtheta, double rho = 0); 

      double * computeProbability(int N, const double *phi, 
                                         const double *theta,
                                         double * p = 0); 

      double computeProbability(double phi, double theta); 

    private:
      double phi; 
      double theta; 
      double dphi; 
      double dtheta; 
      double inv_dphi2; 
      double inv_dtheta2; 
      double inv_dphidtheta; 
      double rho; 
      double expterm; 
      double norm; 

  }; 

  class PointingResolutionModel 
  {
    public: 
      virtual PointingResolution *  computePointingResolution(const AnitaEventSummary * sum, AnitaPol::AnitaPol_t pol, int peak, PointingResolution * p = 0) const = 0; 
      virtual ~PointingResolutionModel() { ; } 
  }; 



  class ConstantPointingResolutionModel
  {
    public: 
      ConstantPointingResolutionModel(double dphi, double dtheta, double rho = 0) 
        : dphi(dphi), dtheta(dtheta), rho(rho) { }


      virtual PointingResolution * computePointingResolution(const AnitaEventSummary * sum, AnitaPol::AnitaPol_t pol, int peak, PointingResolution *p) const
      {
        new (p) PointingResolution(sum->peak[pol][peak].phi, sum->peak[pol][peak].theta, dphi,dtheta,rho); 
        return p; 
      }

      private: 
        double dphi, dtheta, rho; 

  }; 

}




#endif 