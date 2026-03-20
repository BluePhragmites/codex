function rmsValues = movingRMS(x, windowSize)
%MOVINGRMS Compute moving RMS values with a fixed window length.
%   rmsValues = movingRMS(x, windowSize) returns the RMS value at each
%   sample using a trailing window of length windowSize.

    arguments
        x (:, 1) double
        windowSize (1, 1) double {mustBeInteger, mustBePositive}
    end

    squaredSignal = x .^ 2;
    kernel = ones(windowSize, 1) / windowSize;
    meanSquare = filter(kernel, 1, squaredSignal);
    rmsValues = sqrt(meanSquare);
end
