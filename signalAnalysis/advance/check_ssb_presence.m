function results = check_ssb_presence()
% Lightweight coarse screening for frames that may contain an SSB block.
% Default values are an example dataset. Before using a new capture, check:
% captureId, variableName, superFrameIdx, frameRange, nrb, scs, fftNum, thresholdDb.
% You may set these variables in the workspace before running this function.

    if ~exist('captureId','var')
        captureId = "20260305001";
    end
    if ~exist('variableName','var')
        variableName = "syncRx";
    end
    if ~exist('superFrameIdx','var')
        superFrameIdx = 0;
    end
    if ~exist('frameRange','var')
        frameRange = 0:69;
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
    if ~exist('thresholdDb','var')
        thresholdDb = 7;
    end

    rxSampleRate = fftNum*scs*1e3;
    [~,~,~,cpLenAll,symbolLenAll] = nr_OFDMInfo(nrb,scs,rxSampleRate);

    winSc = 240;
    winSym = 4;
    kernel = ones(winSc,winSym)/(winSc*winSym);

    frameRange = frameRange(:).';
    frameCount = numel(frameRange);
    fileName = strings(frameCount,1);
    coarseMetricDb = nan(frameCount,1);
    timeContrastDb = nan(frameCount,1);
    freqContrastDb = nan(frameCount,1);
    peakSubcarrier = nan(frameCount,1);
    peakSymbol = nan(frameCount,1);
    hasSSB = false(frameCount,1);
    status = strings(frameCount,1);

    for idx = 1:frameCount
        frameIdx = frameRange(idx);
        fileName(idx) = sprintf("%s_%s_%03d_%05d.mat",captureId,variableName,superFrameIdx,frameIdx);

        if ~isfile(fileName(idx))
            status(idx) = "missing";
            continue;
        end

        dataStruct = load(fileName(idx));
        if ~isfield(dataStruct,variableName)
            status(idx) = "missing_variable";
            continue;
        end

        rxWaveform = dataStruct.(variableName);
        rxGrid = nr_OFDMDemodulate(rxWaveform,nrb,fftNum,cpLenAll,symbolLenAll);
        powerGrid = abs(rxGrid).^2;

        localMean = conv2(powerGrid,kernel,'valid');
        bgMean = median(localMean(:));
        if bgMean <= 0
            bgMean = eps;
        end

        [peakMean,peakIdx] = max(localMean(:));
        [scStart,symStart] = ind2sub(size(localMean),peakIdx);

        symbolPower = mean(powerGrid,1);
        bandSymbolPower = mean(symbolPower(symStart:symStart+winSym-1));
        symbolBg = median(symbolPower);
        if symbolBg <= 0
            symbolBg = eps;
        end

        subcarrierPower = mean(powerGrid,2);
        bandSubcarrierPower = mean(subcarrierPower(scStart:scStart+winSc-1));
        subcarrierBg = median(subcarrierPower);
        if subcarrierBg <= 0
            subcarrierBg = eps;
        end

        coarseMetricDb(idx) = 10*log10(peakMean/bgMean);
        timeContrastDb(idx) = 10*log10(bandSymbolPower/symbolBg);
        freqContrastDb(idx) = 10*log10(bandSubcarrierPower/subcarrierBg);
        peakSubcarrier(idx) = scStart;
        peakSymbol(idx) = symStart;
        hasSSB(idx) = coarseMetricDb(idx) >= thresholdDb && timeContrastDb(idx) >= 1.5;
        status(idx) = "ok";
    end

    results = table(frameRange(:),fileName,coarseMetricDb,timeContrastDb,freqContrastDb, ...
        peakSubcarrier,peakSymbol,hasSSB,status, ...
        'VariableNames',{'frameIdx','fileName','coarseMetricDb','timeContrastDb', ...
        'freqContrastDb','peakSubcarrier','peakSymbol','hasSSB','status'});

    disp(results);
end
