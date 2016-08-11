#include "WaveformCombiner.h" 
#include "AntennaPositions.h" 
#include <vector> 
#include "FilteredAnitaEvent.h"
#include "FFTtools.h"
#include "SystemResponse.h"
#include "DeltaT.h"


UCorrelator::WaveformCombiner::WaveformCombiner(int nantennas, int npad, bool useUnfiltered, bool deconvolve, const char * responseDir)
  : coherent(260), deconvolved(260) 
{
  setNAntennas(nantennas); 
  setNPad(npad); 
  setDeconvolve(deconvolve); 
  setUseUnfiltered(useUnfiltered); 
  setGroupDelayFlag(true); 
  
  memset(responses,0, sizeof(responses)); 

  if (responseDir) 
  {
    loadResponsesFromDir(responseDir);
  }
}


void UCorrelator::WaveformCombiner::loadResponsesFromDir(const char * dir)
{

    fprintf(stderr,"WARNING! loadResponsesFromDir has not been implemented!\n"); 
}
 

UCorrelator::WaveformCombiner::~WaveformCombiner()
{

}

const AnalysisWaveform * UCorrelator::WaveformCombiner::getDeconvolved() const 
{

  if (!do_deconvolution)
  {
//    fprintf(stderr,"WARNING! Deconvolution has not been enabled!\n"); 
    return 0; 
  }

  return &deconvolved; 
}


void UCorrelator::WaveformCombiner::setResponse(const AbstractResponse * response, AnitaPol::AnitaPol_t pol, int antenna)
{
  if (antenna >= 0)
  {
    if (pol == AnitaPol::kNotAPol)
    {
      fprintf(stderr,"WARNING! Seems strange to set the same response for both polarizations of a particular antenna. n"); 
      responses[0][antenna] = response; 
      responses[1][antenna] = response; 
    }
    else
    {
      responses[pol][antenna] = response; 
    }
  }
  else
  {
    for (int i = 0; i < NUM_SEAVEYS; i++)
    {
      if (pol == AnitaPol::kNotAPol)
      {
        responses[0][i] = response; 
        responses[1][i] = response; 
      }
      else
      {
        responses[pol][i] = response; 
      }
    }
  }
}


static const UCorrelator::AntennaPositions * antpos = UCorrelator::AntennaPositions::instance(); 


static void scaleGraph(TGraph * g, double C)
{

  int max = g->GetN(); 

  for (int i = 0; i < max; i++) 
  {
    g->GetY()[i] *= C; 
  }
}

static void addToGraph(TGraph * g, const TGraph * h)
{
  int max = TMath::Min(g->GetN(), h->GetN()); 
  for (int i = 0; i < max; i++)
  {
    g->GetY()[i] += h->GetY()[i]; 
  }
}

void UCorrelator::WaveformCombiner::combine(double phi, double theta, const FilteredAnitaEvent * event, AnitaPol::AnitaPol_t pol, uint64_t disallowed)
{

  int antennas[nant]; 
  std::vector<AnalysisWaveform> padded(nant);
  const int numDeconv = do_deconvolution ? nant : 0;
  std::vector<AnalysisWaveform> deconv(numDeconv);
  // AnalysisWaveform deconv[do_deconvolution ? nant : 0];


  antpos->getClosestAntennas(phi, nant, antennas, disallowed); 
  double delays[nant]; 


  for (int i = 0; i < nant; i++) 
  {
    //ensure transform already calculated so we don't have to repeat when deconvolving
    (void) event->getRawGraph(antennas[i],pol)->freq(); 

    padded[i].~AnalysisWaveform(); 
    new (&padded[i]) AnalysisWaveform(*event->getRawGraph(antennas[i],pol));

    if (i == 0)
    {
      coherent_avg_spectrum = *(event->getRawGraph(antennas[0],pol)->power());
    }
    else
    {
      addToGraph(&coherent_avg_spectrum, event->getRawGraph(antennas[0],pol)->power()); 
    }

    padded[i].padFreq(npad);

    if (do_deconvolution)
    {
     deconv[i].~AnalysisWaveform(); 
      new (&deconv[i]) AnalysisWaveform(*event->getRawGraph(antennas[i],pol));
      responses[pol][antennas[i]]->deconvolveInPlace(&deconv[i]); //TODO add angle 
      if (i == 0)
      {
        deconvolved_avg_spectrum= *(deconv[i].power()); 
      }
      else
      {
        addToGraph(&deconvolved_avg_spectrum, deconv[i].power()); 
      }
      deconv[i].padFreq(npad); 
    }

    delays[i] = i == 0 ? 0 : getDeltaT(antennas[i], antennas[0], phi, theta, pol, enable_group_delay); 

//    printf("%d\n",antennas[i]); 
//    printf("%f\n",delays[i]); 
//    printf("%d\n",padded[i].even()->GetN()); 
  }


  combineWaveforms(nant, &padded[0], delays,0, &coherent); 
  
  scaleGraph(&coherent_avg_spectrum, 1./nant); 

  if (do_deconvolution)
  {
    combineWaveforms(nant, &deconv[0], delays,0, &deconvolved); 
    scaleGraph(&deconvolved_avg_spectrum, 1./nant); 
  }
}


AnalysisWaveform * UCorrelator::WaveformCombiner::combineWaveforms(int nwf, const AnalysisWaveform * wf, const double * delays, const double * scales, AnalysisWaveform * out)
{
  // we want to make the waveform as big as the entire span of all the waveforms to combine 

  double dt = wf[0].deltaT(); 


  double min = wf[0].even()->GetX()[0] + delays[0]; 
  double max = wf[0].even()->GetX()[wf[0].Neven()-1] + delays[0]; 

  for (int i = 1; i < nwf; i++) 
  {
    min = TMath::Min(min, wf[i].even()->GetX()[0] + delays[i]); 
    max = TMath::Max(max, wf[i].even()->GetX()[wf[i].Neven()-1] + delays[i]); 
  }

  int N = ceil((max-min)/dt); 

  if (!out) out = new AnalysisWaveform(N); 
  TGraph * gupdate = out->updateEven(); 
  gupdate->Set(N); 

  for (int i = 0; i < N; i++) 
  {
    gupdate->GetX()[i] = min + i * dt; 

    double val = 0; 
    for (int j = 0; j < nwf; j++) 
    {
      double scale = scales ? scales[j] : 1; 
      val += wf[j].evalEven(gupdate->GetX()[i] + delays[j])/nwf*scale; 
    }

//    printf("%f %f\n", gupdate->GetX()[i], val); 
    gupdate->GetY()[i] = val; 
  }

  return out; 
}

