if ~exist('captureId','var')
    captureId = "20260322001";
end
if ~exist('variableName','var')
    variableName = "syncRx";
end
if ~exist('frameIdx','var')
    frameIdx = 51;
end
if ~exist('rxSampleRate','var')
    rxSampleRate = 1024*30e3;
end
if ~exist('preShiftList','var')
    preShiftList = (150:2:198) * 30e3;
end
if ~exist('searchBWList','var')
    searchBWList = [30 60 90 120 150 180 210 240 270 300];
end

set(0,'DefaultFigureVisible','off');

refBurst.BlockPattern = 'Case C';
refBurst.L_max = 8;
nrbSSB = 20;
scsSSB = hSSBurstSubcarrierSpacing(refBurst.BlockPattern);

filename = sprintf("%s_%s_%03d_%05d.mat",captureId,variableName,0,frameIdx);
S = load(filename);
rxWaveformRaw = S.(variableName);

results = table('Size',[0 7], ...
    'VariableTypes',{'double','double','double','double','double','double','string'}, ...
    'VariableNames',{'frameIdx','preShiftHz','searchBWkHz','cellId','bchCrc','freqOffset','status'});

for preShiftHz = preShiftList
    for searchBW = searchBWList
        try
            rxWaveform = rxWaveformRaw;
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

            pbchHest = nrExtractResources(pbchIndices,hest);
            [pbchEq,csi] = nrEqualizeMMSE(pbchRx,pbchHest,nest);
            Qm = pbchIndicesInfo.G / pbchIndicesInfo.Gd;
            csi = repmat(csi.',Qm,1);
            csi = reshape(csi,[],1);

            pbchBits = nrPBCHDecode(pbchEq,ncellid,v,nest);
            pbchBits = pbchBits .* csi;
            [~,crcBCH] = nrBCHDecode(pbchBits,8,refBurst.L_max,ncellid);

            results = [results; {frameIdx,preShiftHz,searchBW,ncellid,crcBCH,freqOffset,"ok"}]; %#ok<AGROW>
        catch ME
            results = [results; {frameIdx,preShiftHz,searchBW,NaN,NaN,NaN,string(ME.identifier)}]; %#ok<AGROW>
        end
    end
end

disp(results);

