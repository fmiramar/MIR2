// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/Wavelet.hpp"
#include <cassert>
#include <vector>
#include <random>
#include <iostream>
#include <chrono>
int main() {
    std::mt19937 random(19); std::uniform_real_distribution<double> noise(-1,1);
    for(int order=1;order<=8;++order) for(int size:{32,64,256,1024,8192})
        for(int levels=1;levels<=mir2::waveletMaxLevels(size);++levels) {
            std::vector<double> input(static_cast<std::size_t>(size)),work(input.size()),scratch(input.size());
            for(auto& v:input)v=noise(random);
            work=input; const auto filter=mir2::daubechiesFilter(order);
            mir2::waveletForward(work.data(),scratch.data(),size,levels,filter);
            double energy=0,transformed=0;for(int i=0;i<size;++i){energy+=input[static_cast<std::size_t>(i)]*input[static_cast<std::size_t>(i)];transformed+=work[static_cast<std::size_t>(i)]*work[static_cast<std::size_t>(i)];}
            assert(std::abs(energy-transformed)<1.e-8*energy);
            double bands[14]{},previous[14]{};
            mir2::waveletEnergies(work.data(),size,levels,bands);
            double total=0;for(int i=0;i<=levels;++i)total+=bands[i];
            assert(std::abs(total-energy/size)<1.e-10);
            auto m=mir2::waveletMeasurements(bands,previous,levels,48000);
            assert(m.entropy>=0 && m.entropy<=1 && m.flux>0 && m.centroid>=0 && m.spread>=0);
            std::copy(bands,bands+14,previous);
            assert(mir2::waveletMeasurements(bands,previous,levels,48000).flux==0);
            mir2::waveletInverse(work.data(),scratch.data(),size,levels,filter);
            for(int i=0;i<size;++i)assert(std::abs(work[static_cast<std::size_t>(i)]-input[static_cast<std::size_t>(i)])<1.e-10);
        }
    double haar[]{1,1,-1,-1},scratch[4]{};
    mir2::waveletForward(haar,scratch,4,2,mir2::daubechiesFilter(1));
    assert(std::abs(haar[0])<1.e-14 && std::abs(haar[1]-2)<1.e-14 && haar[2]==0 && haar[3]==0);
    // Exercise the production streaming adapter through different host block boundaries.
    for (int frameSize : {32, 128, 1024}) for (int blockSize : {17, 64, 127, 256})
        for (bool symmetric : {false, true}) for (int order : {1, 4, 8}) {
        const auto n = static_cast<std::size_t>(frameSize);
        std::vector<double> input(n * 7), frame(n * 2), output(n), scratchBuffer(n * 2);
        for (auto& value : input) value = noise(random);
        int position = 0;
        for (int block = 0; block < frameSize * 7; block += blockSize) {
            const int limit = std::min(frameSize * 7, block + blockSize);
            for (int sample = block; sample < limit; ++sample) {
                const double actual = mir2::waveletReconstructSample(input[static_cast<std::size_t>(sample)],
                    frame.data(), output.data(), scratchBuffer.data(), position, frameSize,
                    mir2::waveletMaxLevels(frameSize), mir2::daubechiesFilter(order), 0, symmetric);
                const double expected = sample < frameSize ? 0.0 : input[static_cast<std::size_t>(sample - frameSize)];
                assert(std::abs(actual - expected) < 1e-10);
            }
        }
    }
    // Independent filter invariants, including all even-shift orthogonality products.
    for (int order = 1; order <= 8; ++order) {
        const auto filter = mir2::daubechiesFilter(order);
        double sum = 0.0;
        for (int i = 0; i < filter.taps; ++i) sum += filter.low[i];
        assert(std::abs(sum - std::sqrt(2.0)) < 1e-13);
        for (int moment = 0; moment < order; ++moment) {
            double residual = 0.0;
            for (int k = 0; k < filter.taps; ++k)
                residual += (k % 2 ? -1.0 : 1.0) * filter.low[filter.taps - 1 - k]
                    * std::pow(static_cast<double>(k) / filter.taps, moment);
            assert(std::abs(residual) < 1e-13);
        }
        for (int shift = 0; shift < filter.taps; shift += 2) {
            double product = 0.0;
            for (int i = 0; i + shift < filter.taps; ++i) product += filter.low[i] * filter.low[i + shift];
            assert(std::abs(product - (shift == 0 ? 1.0 : 0.0)) < 1e-13);
        }
    }
    // The new bases must add observable behavior, not merely new labels:
    // an interior degree-four polynomial has nonzero db4 detail but zero db8 detail.
    double db4Residual = 0.0, db8Residual = 0.0;
    for (int order : {4, 8}) {
        const auto filter = mir2::daubechiesFilter(order);
        double result = 0.0;
        for (int k = 0; k < filter.taps; ++k)
            result += (k % 2 ? -1.0 : 1.0) * filter.low[filter.taps - 1 - k]
                * std::pow((k + 5.0) / 32.0, 4);
        (order == 4 ? db4Residual : db8Residual) = std::abs(result);
    }
    assert(db4Residual > 1e-6 && db8Residual < 1e-13);
    std::cout << "Interior degree-four detail db4=" << db4Residual << " db8=" << db8Residual << "\n";

    // Build a dense finite analysis operator independently: each new low/high row
    // is a weighted sum of the preceding approximation rows. No production
    // transform routine is called to construct this reference matrix.
    for (int order : {1, 4, 8}) for (bool symmetric : {false, true}) {
        constexpr int size = 32, levels = 5;
        const int count = symmetric ? size * 2 : size;
        const auto filter = mir2::daubechiesFilter(order);
        std::vector<std::vector<double>> matrix(static_cast<std::size_t>(count),
            std::vector<double>(static_cast<std::size_t>(count)));
        for (int i = 0; i < count; ++i) matrix[i][i] = 1;
        for (int length = count, level = 0; level < levels; ++level, length /= 2) {
            const auto previous = matrix;
            for (int row = 0; row < length; ++row) {
                std::fill(matrix[row].begin(), matrix[row].end(), 0);
                const bool high = row >= length / 2;
                for (int k = 0; k < filter.taps; ++k) {
                    const double weight = high ? (k % 2 ? -1 : 1) * filter.low[filter.taps - k - 1] : filter.low[k];
                    const int source = (2 * (row % (length / 2)) + k) % length;
                    for (int column = 0; column < count; ++column)
                        matrix[row][column] += weight * previous[source][column];
                }
            }
        }
        for (int fixture = 0; fixture < 4; ++fixture) {
            std::vector<double> original(size), embedded(count), work(count), scratchBuffer(count), expected(count), projected(size);
            for (int i = 0; i < size; ++i)
                original[i] = fixture == 0 ? noise(random) : fixture == 1 ? i / 31.0
                    : fixture == 2 ? (i == 0 ? 1.0 : 0.0) : (i == size - 1 ? 1.0 : 0.0);
            mir2::waveletEmbed(original.data(), embedded.data(), size, symmetric);
            for (int i = 0; i < count; ++i)
                assert(embedded[i] == original[i < size ? i : count - 1 - i]);
            for (int row = 0; row < count; ++row)
                for (int col = 0; col < count; ++col) expected[row] += matrix[row][col] * embedded[col];
            work = embedded;
            mir2::waveletForward(work.data(), scratchBuffer.data(), count, levels, filter);
            for (int i = 0; i < count; ++i) assert(std::abs(work[i] - expected[i]) < 1e-11);
            double bands[14]{}, inputEnergy = 0, outputEnergy = 0;
            for (double value : original) inputEnergy += value * value;
            mir2::waveletEnergies(work.data(), count, levels, bands);
            for (int i = 0; i <= levels; ++i) outputEnergy += bands[i];
            assert(std::abs(inputEnergy / size - outputEnergy) < 1e-11);
            for (int remove : {0, 3, count / 2, count}) {
                work = expected;
                std::fill(work.begin(), work.begin() + remove, 0);
                std::vector<double> denseInverse(count);
                for (int col = 0; col < count; ++col)
                    for (int row = 0; row < count; ++row) denseInverse[col] += matrix[row][col] * work[row];
                mir2::waveletInverse(work.data(), scratchBuffer.data(), count, levels, filter);
                mir2::waveletProject(work.data(), projected.data(), size, symmetric);
                outputEnergy = 0;
                for (int i = 0; i < size; ++i) {
                    const double reference = symmetric ? (denseInverse[i] + denseInverse[count - 1 - i]) * .5 : denseInverse[i];
                    assert(std::abs(projected[i] - reference) < 1e-11);
                    if (remove == 0) assert(std::abs(projected[i] - original[i]) < 1e-11);
                    if (remove == count) assert(projected[i] == 0);
                    outputEnergy += projected[i] * projected[i];
                }
                assert(outputEnergy <= inputEnergy + 1e-10);
            }
        }
    }
    // Half-sample reflection removes the artificial end-to-start jump of a ramp.
    double rampDetail[2]{};
    for (int boundary = 0; boundary < 2; ++boundary) {
        const int size = 256, count = size * (boundary ? 2 : 1);
        std::vector<double> input(size), work(count), scratchBuffer(count);
        for (int i = 0; i < size; ++i) input[i] = i / 255.0;
        mir2::waveletEmbed(input.data(), work.data(), size, boundary != 0);
        mir2::waveletForward(work.data(), scratchBuffer.data(), count, 1, mir2::daubechiesFilter(4));
        double bands[2]{};
        mir2::waveletEnergies(work.data(), count, 1, bands);
        rampDetail[boundary] = bands[1];
    }
    assert(rampDetail[1] < rampDetail[0] * .01);
    std::cout << "Ramp finest detail periodic=" << rampDetail[0] << " symmetric=" << rampDetail[1] << "\n";
    double silent[4]{}, oneScale[4]{1,0,0,0};
    assert(mir2::waveletMeasurements(oneScale,silent,3,48000).flux == 1.0);
    std::vector<double> benchmark(1024), work(2048), temporary(2048), output(1024);
    for (auto& value : benchmark) value = noise(random);
    volatile double sink = 0.0;
    std::cout << "Wavelet db1..db8 reconstruction, reflected finite-operator oracle, streaming block invariance, energy and Haar oracle passed\n";
    for (int order : {4, 8}) for (bool symmetric : {false, true}) {
        const auto start = std::chrono::steady_clock::now();
        const int count = symmetric ? 2048 : 1024;
        for (int iteration = 0; iteration < 3000; ++iteration) {
            mir2::waveletEmbed(benchmark.data(), work.data(), 1024, symmetric);
            mir2::waveletForward(work.data(),temporary.data(),count,6,mir2::daubechiesFilter(order));
            mir2::waveletInverse(work.data(),temporary.data(),count,6,mir2::daubechiesFilter(order));
            mir2::waveletProject(work.data(), output.data(), 1024, symmetric);
            sink = sink + output[0];
        }
        const double micros = std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/3000;
        std::cout << "db" << order << (symmetric ? " symmetric" : " periodic")
            << " forward+inverse 1024/6: " << micros << " us/frame (" << sink << ")\n";
    }
}
