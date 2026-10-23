// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/ConstantQ.hpp"
#include <cassert>
#include <vector>
#include <random>
#include <chrono>
#include <iostream>
int main(){
    std::mt19937 random(82);std::uniform_real_distribution<double> noise(-1,1);
    for(double sr:{44100.,48000.,96000.})for(double gamma:{0.,20.}){
        const int bands=48,bpo=12;const double minimum=110.;
        const int size=mir2::cqFrameSize(sr,minimum,bpo,gamma);assert(size>0&&size<=32768);
        std::vector<mir2::Complex> work(static_cast<std::size_t>(size)),spectrum(work.size());
        std::vector<double> input(work.size()),result(bands);int offsets[bands+1]{};
        const int count=mir2::buildCQKernels(work.data(),size,sr,minimum,bpo,bands,gamma,1.e-4,offsets,nullptr);assert(count>0);
        std::vector<mir2::CQEntry> entries(static_cast<std::size_t>(count));
        assert(mir2::buildCQKernels(work.data(),size,sr,minimum,bpo,bands,gamma,1.e-4,offsets,entries.data(),count)==count);
        for(int type=0;type<3;++type){
            for(int i=0;i<size;++i){input[i]=type==0?0:(type==1?noise(random):.3*std::sin(mir2::kTwoPi*440*i/sr));spectrum[i]={input[i],0};}
            mir2::fft(spectrum.data(),size,false);
            mir2::constantQ(spectrum.data(),entries.data(),offsets,bands,result.data());
            for(int b=0;b<bands;++b){
                const double freq=minimum*std::exp2(static_cast<double>(b)/bpo);const int length=mir2::cqKernelLength(sr,freq,bpo,gamma);
                const int offset=(size-length)/2;double real=0,imag=0;
                for(int i=0;i<length;++i){const double w=(1-std::cos(mir2::kTwoPi*i/length))/length;
                    const double p=mir2::kTwoPi*freq*(i-(length-1)*.5)/sr;
                    real+=input[offset+i]*w*std::cos(p);imag-=input[offset+i]*w*std::sin(p);}
                const double oracle=2*std::hypot(real,imag);
                assert(std::abs(result[b]-oracle)<.0005);
                if(type==0)assert(result[b]==0);
            }
            if(type==2){assert(std::abs(result[24]-.3)<.001);assert(std::max_element(result.begin(),result.end())-result.begin()==24);}
        }
        std::vector<double> uniform(128),coefficients(13);
        std::fill(result.begin(),result.end(),1.0);
        mir2::cqCepstra(result.data(),bands,minimum,bpo,uniform.data(),128,coefficients.data(),13,true,1.e-10);
        for(auto c:coefficients)assert(std::abs(c)<1.e-10);
        const auto start=std::chrono::steady_clock::now();
        for(int repeat=0;repeat<100;++repeat){for(int i=0;i<size;++i)spectrum[i]={input[i],0};mir2::fft(spectrum.data(),size,false);mir2::constantQ(spectrum.data(),entries.data(),offsets,bands,result.data());}
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/100;
        std::cout<<"sr="<<sr<<" gamma="<<gamma<<" size="<<size<<" entries="<<count<<" ms/frame="<<ms<<'\n';
    }
    assert(mir2::cqFrameSize(96000,20,48,0)==0);
    assert(mir2::cqKernelLength(48000,110,12,20)<mir2::cqKernelLength(48000,110,12,0));
}
