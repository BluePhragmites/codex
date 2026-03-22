if ~exist('captureId','var')
    captureId = "20260322001";
end
if ~exist('variableName','var')
    variableName = "syncRx";
end
if ~exist('candidateFrames','var')
    candidateFrames = [0 3 5 31 51 63];
end
if ~exist('rxSampleRate','var')
    rxSampleRate = 1024*30e3;
end
if ~exist('searchBW','var')
    searchBW = 30*10;
end
if ~exist('preShiftHz','var')
    preShiftHz = 186*30e3;
end

set(0,'DefaultFigureVisible','off');

refBurst.BlockPattern = 'Case C';
refBurst.L_max = 8;
nrbSSB = 20;
scsSSB = hSSBurstSubcarrierSpacing(refBurst.BlockPattern);

results = table('Size',[0 6], ...
    'VariableTypes',{'double','double','double','double','double','string'}, ...
    'VariableNames',{'frameIdx','cellId','bchCrc','frameOffset','freqOffset','status'});

for frameIdx = candidateFrames
    try
        filename = sprintf("%s_%s_%03d_%05d.mat",captureId,variableName,0,frameIdx);
        S = load(filename);
        rxWaveform = S.(variableName);

        t = (0:size(rxWaveform,1)-1).' / rxSampleRate;
        rxWaveform = rxWaveform .* exp(1i*2*pi*preShiftHz*t);

        [rxWaveform,freqOffset,NID2] = hSSBurstFrequencyCorrect(rxWaveform,refBurst.BlockPattern,rxSampleRate,searchBW);

        refGrid = zeros([nrbSSB*12 2]);
        refGrid(nrPSSIndices,2) = nrPSS(NID2);
        timingOffset = nrTimingEstimate(rxWaveform,nrbSSB,scsSSB,0,refGrid,'SampleRate',rxSampleRate);

        rxGrid = nrOFDMDemodulate(rxWaveform(1+timingOffset:end,:),nrbSSB,scsSSB,0,'SampleRate',rxSampleRate);
        rxGrid = rxGrid(:,2:5,:);

        sssIndices = nrSSSIndices;
        sssRx = nrExtractResources(sssIndices,rxGrid);
        sssEst = zeros(1,336);
        for NID1 = 0:335
            ncellid = (3*NID1) + NID2;
            sssRef = nrSSS(ncellid);
            sssEst(NID1+1) = sum(abs(mean(sssRx .* conj(sssRef),1)).^2);
        end

        NID1 = find(sssEst == max(sssEst),1) - 1;
        ncellid = (3*NID1) + NID2;

        dmrsIndices = nrPBCHDMRSIndices(ncellid);
        dmrsEst = zeros(1,8);
        for ibarSSB = 0:7
            refGridDmrs = zeros([240 4]);
            refGridDmrs(dmrsIndices) = nrPBCHDMRS(ncellid,ibarSSB);
            [hest,nest] = nrChannelEstimate(rxGrid,refGridDmrs,'AveragingWindow',[0 1]);
            dmrsEst(ibarSSB+1) = 10*log10(mean(abs(hest(:).^2)) / nest);
        end

        ibarSSB = find(dmrsEst == max(dmrsEst),1) - 1;
        refGridPbch = zeros([nrbSSB*12 4]);
        refGridPbch(dmrsIndices) = nrPBCHDMRS(ncellid,ibarSSB);
        refGridPbch(sssIndices) = nrSSS(ncellid);
        [hest,nest] = nrChannelEstimate(rxGrid,refGridPbch,'AveragingWindow',[0 1]);

        [pbchIndices,pbchIndicesInfo] = nrPBCHIndices(ncellid);
        pbchRx = nrExtractResources(pbchIndices,rxGrid);

        if refBurst.L_max == 4
            v = mod(ibarSSB,4);
        else
            v = ibarSSB;
        end
        ssbIndex = v;

        pbchHest = nrExtractResources(pbchIndices,hest);
        [pbchEq,csi] = nrEqualizeMMSE(pbchRx,pbchHest,nest);
        Qm = pbchIndicesInfo.G / pbchIndicesInfo.Gd;
        csi = repmat(csi.',Qm,1);
        csi = reshape(csi,[],1);

        pbchBits = nrPBCHDecode(pbchEq,ncellid,v,nest);
        pbchBits = pbchBits .* csi;
        [~,crcBCH,~,~,~,msbidxoffset] = nrBCHDecode(pbchBits,8,refBurst.L_max,ncellid);

        if crcBCH
            results = [results; {frameIdx,ncellid,crcBCH,NaN,freqOffset,"bch_crc_fail"}]; %#ok<AGROW>
            continue;
        end

        if refBurst.L_max == 64
            ssbIndex = ssbIndex + (bi2de(msbidxoffset.','left-msb') * 8);
        end

        ofdmInfo = nrOFDMInfo(1,scsSSB,'SampleRate',rxSampleRate);
        srRatio = rxSampleRate/(scsSSB*1e3*ofdmInfo.Nfft);
        symbolLengths = ofdmInfo.SymbolLengths * srRatio;
        offset = timingOffset + symbolLengths(1);
        burstStartSymbols = hSSBurstStartSymbols(refBurst.BlockPattern,refBurst.L_max);
        ssbFirstSym = burstStartSymbols(ssbIndex+1);
        symbolsPerSubframe = length(symbolLengths);
        subframeOffset = floor(ssbFirstSym/symbolsPerSubframe);
        samplesPerSubframe = sum(symbolLengths);
        symbolOffset = mod(ssbFirstSym,symbolsPerSubframe);
        frameOffset = round(offset - (subframeOffset*samplesPerSubframe) - sum(symbolLengths(1:symbolOffset)));

        results = [results; {frameIdx,ncellid,crcBCH,frameOffset,freqOffset,"ok"}]; %#ok<AGROW>
    catch ME
        results = [results; {frameIdx,NaN,NaN,NaN,NaN,string(ME.identifier)}]; %#ok<AGROW>
    end
end

disp(results);

