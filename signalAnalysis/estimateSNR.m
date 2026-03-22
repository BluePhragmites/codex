function snrDb = estimateSNR(signalSegment, noiseSegment)
%ESTIMATESNR Estimate signal-to-noise ratio in decibels.
%   snrDb = estimateSNR(signalSegment, noiseSegment) computes SNR using the
%   average power of signalSegment and noiseSegment.

    arguments
        signalSegment (:, 1) double
        noiseSegment (:, 1) double
    end

    signalPower = mean(signalSegment .^ 2);
    noisePower = mean(noiseSegment .^ 2);

    if noisePower == 0
        error('Noise power cannot be zero when estimating SNR.');
    end

    snrDb = 10 * log10(signalPower / noisePower);
end
