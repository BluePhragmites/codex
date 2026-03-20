%DEMOSIGNALANALYSIS Demonstration of the basic MATLAB signal analysis tools.

fs = 1000;
t = (0:1/fs:1-1/fs)';
cleanSignal = sin(2 * pi * 50 * t) + 0.5 * sin(2 * pi * 120 * t);
noise = 0.2 * randn(size(t));
x = cleanSignal + noise;

[f, amplitude, dominantFreq] = fftSpectrumAnalysis(x, fs);
rmsValues = movingRMS(x, 50);
snrDb = estimateSNR(cleanSignal, noise);

fprintf('Dominant frequency: %.2f Hz\n', dominantFreq);
fprintf('Estimated SNR: %.2f dB\n', snrDb);

figure('Name', 'Signal Analysis Demo');
subplot(3, 1, 1);
plot(t, x);
title('Time-Domain Signal');
xlabel('Time (s)');
ylabel('Amplitude');
grid on;

subplot(3, 1, 2);
plot(f, amplitude);
title('Single-Sided Amplitude Spectrum');
xlabel('Frequency (Hz)');
ylabel('Amplitude');
grid on;

subplot(3, 1, 3);
plot(t, rmsValues);
title('Moving RMS');
xlabel('Time (s)');
ylabel('RMS');
grid on;
