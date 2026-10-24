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
        const int bands=84,bpo=12;const double minimum=55.;
        const int size=mir2::cqFrameSize(sr,minimum,bpo,gamma);assert(size>0&&size<=32768);
        std::vector<mir2::Complex> work(static_cast<std::size_t>(size)),spectrum(work.size());
        std::vector<double> input(work.size()),result(bands);int offsets[bands+1]{};
        const int count=mir2::buildCQKernels(work.data(),size,sr,minimum,bpo,bands,gamma,1.e-4,offsets,nullptr);assert(count>0);
        std::vector<mir2::CQEntry> entries(static_cast<std::size_t>(count));
        assert(mir2::buildCQKernels(work.data(),size,sr,minimum,bpo,bands,gamma,1.e-4,offsets,entries.data(),count)==count);
        for(int type=0;type<3;++type){
            for(int i=0;i<size;++i){input[static_cast<std::size_t>(i)]=type==0?0:(type==1?noise(random):.3*std::sin(mir2::kTwoPi*440*i/sr));spectrum[static_cast<std::size_t>(i)]={input[static_cast<std::size_t>(i)],0};}
            mir2::fft(spectrum.data(),size,false);
            mir2::constantQ(spectrum.data(),entries.data(),offsets,bands,result.data());
            for(int b=0;b<bands;++b){
                const double freq=minimum*std::exp2(static_cast<double>(b)/bpo);const int length=mir2::cqKernelLength(sr,freq,bpo,gamma);
                const int offset=(size-length)/2;double real=0,imag=0;
                for(int i=0;i<length;++i){const double w=(1-std::cos(mir2::kTwoPi*i/length))/length;
                    const double p=mir2::kTwoPi*freq*(i-(length-1)*.5)/sr;
                    real+=input[static_cast<std::size_t>(offset+i)]*w*std::cos(p);imag-=input[static_cast<std::size_t>(offset+i)]*w*std::sin(p);}
                const double oracle=2*std::hypot(real,imag);
                assert(std::abs(result[static_cast<std::size_t>(b)]-oracle)<.0005);
                if(type==0)assert(result[static_cast<std::size_t>(b)]==0);
            }
            if(type==2){assert(std::abs(result[36]-.3)<.001);assert(std::max_element(result.begin(),result.end())-result.begin()==36);}
        }
        std::vector<double> uniform(128),coefficients(13),without0(12);
        // Log power affine in Hz must remain affine after linear-Hz resampling.
        for(int b=0;b<bands;++b){
            const double frequency=minimum*std::exp2(static_cast<double>(b)/bpo);
            result[static_cast<std::size_t>(b)]=std::exp(0.5*(-8.0+0.001*frequency));
        }
        mir2::cqCepstra(result.data(),bands,minimum,bpo,uniform.data(),128,coefficients.data(),13,true,1.e-10);
        mir2::cqCepstra(result.data(),bands,minimum,bpo,uniform.data(),128,without0.data(),12,false,1.e-10);
        const double maximum=minimum*std::exp2(static_cast<double>(bands-1)/bpo);
        for(int i=0;i<128;++i){
            const double expected=-8.0+0.001*(minimum+(maximum-minimum)*i/127.0);
            assert(std::abs(uniform[static_cast<std::size_t>(i)]-expected)<1.e-12);
        }
        for(int i=0;i<12;++i)assert(std::abs(coefficients[static_cast<std::size_t>(i+1)]-without0[static_cast<std::size_t>(i)])<1.e-12);
        std::fill(result.begin(),result.end(),1.0);
        mir2::cqCepstra(result.data(),bands,minimum,bpo,uniform.data(),128,coefficients.data(),13,true,1.e-10);
        for(auto c:coefficients)assert(std::abs(c)<1.e-10);
        const auto start=std::chrono::steady_clock::now();
        for(int repeat=0;repeat<100;++repeat){for(int i=0;i<size;++i)spectrum[static_cast<std::size_t>(i)]={input[static_cast<std::size_t>(i)],0};mir2::fft(spectrum.data(),size,false);mir2::constantQ(spectrum.data(),entries.data(),offsets,bands,result.data());}
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/100;
        std::cout<<"sr="<<sr<<" gamma="<<gamma<<" size="<<size<<" entries="<<count<<" ms/frame="<<ms<<'\n';
    }
    assert(mir2::cqFrameSize(96000,20,48,0)==0);
    assert(mir2::cqFrameSize(-48000,110,12,0)==0);
    assert(mir2::cqKernelLength(48000,0,12,0)==0);
    std::vector<mir2::Complex> work(1024); int offsets[13]{};
    assert(mir2::buildCQKernels(work.data(),1024,48000,20000,12,12,0,1e-4,offsets,nullptr)==-1);
    assert(mir2::buildCQKernels(work.data(),1024,48000,1000,12,12,0,1e-4,offsets,nullptr,1)==-1);
    std::cout<<"CQT/VQT direct-correlation oracle and CQCC linear-Hz resampling passed\n";
    assert(mir2::cqKernelLength(48000,110,12,20)<mir2::cqKernelLength(48000,110,12,0));
}
