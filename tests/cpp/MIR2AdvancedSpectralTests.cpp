// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/SpectralAdvanced.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>
namespace {
void check(bool condition, const char* message) { if (!condition) { std::cerr << message << '\n'; std::exit(1); } }
void near(double value, double expected, const char* message, double tolerance = 1.0e-10) {
    if (!std::isfinite(value) || std::abs(value-expected)>tolerance) {
        std::cerr << message << " actual " << value << " expected " << expected << '\n'; std::exit(1);
    }
}
}
int main() {
    using D=mir2::SpectralDescriptor;
    const double uniform[]={1,1,1,1,1};
    const double tone[]={0,0,2,0,0};
    const double ascending[]={1,2,3,4,5};
    auto evaluate=[&](const double* x,D d,int spectrum=0,double p=2,int mode=0) {
        return mir2::spectralDescriptor(x,nullptr,5,100,0,0,spectrum,d,p,mode);
    };
    near(evaluate(uniform,D::crest),1,"uniform crest");
    near(evaluate(tone,D::crest),5,"single-bin crest");
    near(evaluate(uniform,D::spread),std::sqrt(20000.),"known spectral spread");
    near(evaluate(tone,D::spread),0,"single-bin spread");
    near(evaluate(ascending,D::slope),1.0/1500.0,"normalized least-squares slope");
    near(evaluate(tone,D::power,1),0.8,"mean squared bins");
    near(evaluate(tone,D::energy,1),4,"sum squared bins");
    near(evaluate(tone,D::rms,1),std::sqrt(.8),"spectral RMS");
    near(evaluate(uniform,D::entropy),1,"normalized entropy");
    near(evaluate(uniform,D::entropy,0,2,1),std::log(5.),"natural entropy");
    near(evaluate(uniform,D::entropy,0,2,2),std::log2(5.),"entropy bits");
    near(evaluate(tone,D::entropy),0,"one-bin entropy");
    near(evaluate(ascending,D::decrease),4.0/14.0,"spectral decrease formula");
    near(evaluate(tone,D::complexity,0,.1),1,"peak count");
    near(evaluate(tone,D::strongPeak),1,"peak concentration");
    near(evaluate(tone,D::hfc,1),800,"linear Hz HFC");
    near(evaluate(tone,D::hfc,1,2,1),160000,"squared Hz HFC");
    near(evaluate(uniform,D::bandwidth,0,1),120,"p=1 bandwidth");
    near(evaluate(uniform,D::irregularity),0,"uniform Jensen irregularity");
    near(evaluate(ascending,D::irregularity),4.0/55.0,"Jensen adjacent differences");
    near(evaluate(ascending,D::irregularity,0,2,1),0,"linear ramp Krimphoff deviation");
    near(evaluate(tone,D::irregularity,0,2,1),8.0/3.0,"Krimphoff raw L1 deviation");
    near(evaluate(tone,D::sparsity),1,"one-bin Hoyer");
    near(evaluate(uniform,D::sparsity),0,"uniform Hoyer");
    near(mir2::spectralDescriptor(uniform,tone,5,100,0,0,0,D::variation),1-1/std::sqrt(5.),"cosine variation");
    near(mir2::spectralDescriptor(tone,tone,5,100,0,0,0,D::variation),0,"same-frame variation");
    near(mir2::spectralDescriptor(ascending,nullptr,5,100,100,200,1,D::bandRatio),13./55.,"band/full energy ratio");
    near(mir2::spectralDescriptor(uniform,nullptr,5,100,1000,0,0,D::energy),0,"empty range above Nyquist");
    double scratch[5],contrast,valley;
    mir2::spectralContrast(ascending,0,4,0,.2,1e-20,scratch,contrast,valley);
    near(contrast,20*std::log10(5.),"contrast top/bottom quantile"); near(valley,0,"valley dB");
    mir2::spectralContrast(ascending,0,4,1,.4,1e-20,scratch,contrast,valley);
    near(contrast,10*std::log10(20.5/2.5),"power contrast quantile means");
    mir2::SpectralPeak peaks[2];
    const double threePeaks[]={0,1,0,3,0,2,0};
    check(mir2::extractSpectralPeaks(threePeaks,7,100,0,0,0,peaks,2)==2,"capacity count");
    near(peaks[0].frequency,300,"strongest selection sorted");near(peaks[1].frequency,500,"second strongest selection");
    double gaussian[9];
    for(int i=0;i<9;++i) gaussian[i]=std::exp(-.5*(i-3.25)*(i-3.25));
    check(mir2::extractSpectralPeaks(gaussian,9,100,0,0,.1,peaks,2)==1,"log-parabolic peak count");
    near(peaks[0].frequency,325,"log-parabolic frequency");near(peaks[0].magnitude,1,"log-parabolic height");
    const double silent[]={0,0,0,0,0};
    for(int d=0;d<=static_cast<int>(D::sparsity);++d) near(evaluate(silent,static_cast<D>(d)),0,"silence descriptor");
    const double dirty[]={std::numeric_limits<double>::quiet_NaN(),-1,1,std::numeric_limits<double>::infinity(),0};
    for(int d=0;d<=static_cast<int>(D::sparsity);++d) check(std::isfinite(evaluate(dirty,static_cast<D>(d))),"finite on invalid bins");
    std::vector<double> bins(1025), work(1025);for(int i=0;i<1025;++i) bins[static_cast<std::size_t>(i)]=1+std::sin(i*.13);
    volatile double sink=0;
    const auto start=std::chrono::steady_clock::now();
    for(int frame=0;frame<2000;++frame) for(int d=0;d<=static_cast<int>(D::sparsity);++d)
        sink+=mir2::spectralDescriptor(bins.data(),bins.data(),1025,48000./2048.,0,0,1,static_cast<D>(d),2,0);
    const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"Advanced spectral tests passed; 16 descriptors, 2048 FFT: "<<seconds*1e6/2000<<" us/frame-group; guard "<<sink<<'\n';
}
