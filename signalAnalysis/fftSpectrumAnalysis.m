function [f, amplitude, dominantFreq] = fftSpectrumAnalysis(x, fs)
%FFTSPECTRUMANALYSIS Compute the single-sided amplitude spectrum of a signal.
%   [f, amplitude, dominantFreq] = fftSpectrumAnalysis(x, fs) returns the
%   frequency axis f, the single-sided amplitude spectrum, and the dominant
%   frequency dominantFreq for the input signal x sampled at fs Hz.

    arguments
        x (:, 1) double
        fs (1, 1) double {mustBePositive}
    end

    n = length(x);
    spectrum = fft(x);
    twoSided = abs(spectrum / n);
    amplitude = twoSided(1:floor(n / 2) + 1);

    if length(amplitude) > 2
        amplitude(2:end-1) = 2 * amplitude(2:end-1);
    end

    f = fs * (0:floor(n / 2)) / n;
    [~, idx] = max(amplitude);
    dominantFreq = f(idx);
end
