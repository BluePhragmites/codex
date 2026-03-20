close all
clc
clearvars -except captureId variableName superFrameIdx fileGroup frameRange nrb scs fftNum frameOffset

% Default values in this script are only an example dataset.
% Before analyzing a new capture, first check or modify:
% captureId, variableName, superFrameIdx, frameRange, nrb, scs, fftNum, frameOffset.
% You may either set these variables in the workspace before running this script
% or directly edit the defaults below for the current dataset.

%%
if ~exist('captureId','var')
    captureId = "20260305001";
end
if ~exist('variableName','var')
    variableName = "syncRx";
end
if ~exist('superFrameIdx','var')
    if exist('fileGroup','var')
        superFrameIdx = fileGroup;
    else
        superFrameIdx = 0;
    end
end
if ~exist('frameRange','var')
    frameRange = 0;
end
if ~exist('nrb','var')
    nrb = 51;
end
if ~exist('scs','var')
    scs = 30;
end
if ~exist('fftNum','var')
    fftNum = 4096;
end
if ~exist('frameOffset','var')
    frameOffset = 0;
end

rxSampleRate = fftNum*scs*1e3;%122880000;%fftNum*scs*1e3;

%%
[cpLen,symbolLen,~,cpLenAll,symbolLenAll] = nr_OFDMInfo(nrb,scs,rxSampleRate);

%%
for frameIdx = frameRange
    % The middle 3-digit field is the NR 1024-frame cycle index.
    filename = sprintf("%s_%s_%03d_%05d.mat",captureId,variableName,superFrameIdx,frameIdx);
    dataStruct = load(filename);
    rxWaveform = dataStruct.(variableName);

    if frameOffset <= 0
      rxWaveform = [zeros(frameOffset,1);rxWaveform];
    else
      rxWaveform = [rxWaveform(frameOffset:end);zeros(frameOffset,1)];
    end

    %% 下行信号显示
    nrGrid = nr_OFDMDemodulate(rxWaveform,nrb,fftNum,cpLenAll,symbolLenAll);
    figure(1);subplot(211);plot(abs(rxWaveform));
    xlim([0 4096*15*20]);
    title("time domain data");
    figure(1);subplot(212);imagesc(abs(nrGrid));
    title("time-freq domain data");
    set(gcf,'Position',[105 262 560 420])
    %% 暂停0.2秒
    % pause(0.2);
end
