#include "Flags.h" 
#include "UsefulAnitaEvent.h"
#include "AnitaGeomTool.h" 


int UCorrelator::flags::checkSaturation(const UsefulAnitaEvent *ev, uint64_t * save_hsat, uint64_t * save_vsat, double thresh)
{
  
  uint64_t hsat = 0; 
  uint64_t vsat = 0; 

  int totalsat = 0; 
  for (int i = 0; i < NUM_SEAVEYS; i++) 
  {
      int hindex = AnitaGeomTool::getChanIndexFromAntPol(i,AnitaPol::kHorizontal); 
      int vindex = AnitaGeomTool::getChanIndexFromAntPol(i,AnitaPol::kVertical); 
      const double *yh = ev->fVolts[hindex]; 
      const double *yv = ev->fVolts[vindex]; 

      for (int j = 0; j < ev->fNumPoints[hindex]; j++)
      {
        if (fabs(yh[j]) > thresh)
        {
          hsat &= 1 << i; 
          totalsat ++; 
          break; 
        }
      }

      for (int j = 0; j < ev->fNumPoints[vindex]; j++)
      {
        if (fabs(yv[j]) > thresh)
        {
          vsat &= 1 << i; 
          totalsat ++; 
          break; 
        }
      }
  }


  if (save_hsat) *save_hsat = hsat; 
  if (save_vsat) *save_hsat = vsat; 

  return totalsat; 
}

