%% NR Cell Search and MIB and SIB1 Recovery
clear all
close all
clc

% readDat("20260213001_syncRx_xnr-sxr-01.map",1,20*1024*15*2,0*4,"int16=>double",10,0,1)

load('20260305001_syncRx_000_00000.mat')
rxSampleRate = 4096*30e3;


rxWaveform = syncRx;
searchBW = 30*10;
FrequencyOffset = 186*30e3;
t = (0:size(rxWaveform,1)-1).' / rxSampleRate;
rxWaveform = rxWaveform .* exp(1i*2*pi*FrequencyOffset*t);


refBurst.BlockPattern = 'Case C';
refBurst.L_max = 8;
minChannelBW = 10;
fPhaseComp = 0;

% rxWaveform(15360*7:end) = 0;
% Get OFDM information from configured burst and receiver parameters
nrbSSB = 20;
scsSSB = hSSBurstSubcarrierSpacing(refBurst.BlockPattern);
rxOfdmInfo = nrOFDMInfo(nrbSSB,scsSSB,'SampleRate',rxSampleRate);

% 17793.120 - 17799.960

% Display spectrogram of received waveform
% figure;
% plot(abs(enb));
figure;
nfft = rxOfdmInfo.Nfft;
spectrogram(rxWaveform(:,1),ones(nfft,1),0,nfft,'centered',rxSampleRate,'yaxis','MinThreshold',-130);
title('Spectrogram of the Received Waveform')

%% PSS Search and Frequency Offset Correction
% The receiver performs PSS search and coarse frequency offset estimation
% following these steps:
%
% * Frequency shift the received waveform with a candidate frequency
% offset. Candidate offsets are spaced half subcarrier apart. Use
% |searchBW| to control the frequency offset search bandwidth.
% * Correlate the frequency-shifted received waveform with each of the
% three possible PSS sequences (NID2) and extract the strongest correlation
% peak. The reference PSS sequences are centered in frequency. Therefore,
% the strongest correlation peak provides a measure of coarse frequency
% offset with respect to the center frequency of the carrier. The peak also
% indicates which of the three PSS (NID2) has been detected in the received
% waveform and the time instant of the best channel conditions.
% * Estimate frequency offsets below half subcarrier by correlating the
% cyclic prefix of each OFDM symbol in the SSB with the corresponding
% useful parts of the OFDM symbols. The phase of this correlation is
% proportional to the frequency offset in the waveform.

disp(' -- Frequency correction and timing estimation --')

% Specify the frequency offset search bandwidth in kHz
% searchBW = 80*scsSSB;
[rxWaveform,freqOffset,NID2] = hSSBurstFrequencyCorrect(rxWaveform,refBurst.BlockPattern,rxSampleRate,searchBW);
disp([' Frequency offset: ' num2str(freqOffset,'%.0f') ' Hz'])
% NID2 = 2;
%% Time Synchronization and OFDM Demodulation
% The receiver estimates the timing offset to the strongest SS block by
% using the reference PSS sequence detected in the frequency search
% process. After frequency offset correction, the receiver can assume that
% the center frequencies of the reference PSS and received waveform are
% aligned. Finally, the receiver OFDM demodulates the synchronized waveform
% and extracts the SS block.

% Create a reference grid for timing estimation using detected PSS. The PSS
% is placed in the second OFDM symbol of the reference grid to avoid the
% special CP length of the first OFDM symbol.
refGrid = zeros([nrbSSB*12 2]);
refGrid(nrPSSIndices,2) = nrPSS(NID2); % Second OFDM symbol for correct CP length

% Timing estimation. This is the timing offset to the OFDM symbol prior to
% the detected SSB due to the content of the reference grid
nSlot = 0;
timingOffset = nrTimingEstimate(rxWaveform,nrbSSB,scsSSB,nSlot,refGrid,'SampleRate',rxSampleRate);
% timingOffset = timingOffset - 23040 + 6*(108+1536);
% timingOffset = timingOffset - 30720*3;
% Synchronization, OFDM demodulation, and extraction of strongest SS block
rxGrid = nrOFDMDemodulate(rxWaveform(1+timingOffset:end,:),nrbSSB,scsSSB,nSlot,'SampleRate',rxSampleRate);
rxGrid = rxGrid(:,2:5,:);

% Display the timing offset in samples. As the symbol lengths are measured
% in FFT samples, scale the symbol lengths to account for the receiver
% sample rate.
srRatio = rxSampleRate/(scsSSB*1e3*rxOfdmInfo.Nfft);
firstSymbolLength = rxOfdmInfo.SymbolLengths(1)*srRatio;
str = sprintf(' Time offset to synchronization block: %%.0f samples (%%.%.0ff ms) \n',floor(log10(rxSampleRate))-3);
fprintf(str,timingOffset+firstSymbolLength,(timingOffset+firstSymbolLength)/rxSampleRate*1e3);

%% SSS Search
% The receiver extracts the resource elements associated to the SSS from
% the received grid and correlates them with each possible SSS sequence
% generated locally. The indices of the strongest PSS and SSS sequences
% combined give the physical layer cell identity, which is required for
% PBCH DM-RS and PBCH processing.

% Extract the received SSS symbols from the SS/PBCH block
sssIndices = nrSSSIndices;
sssRx = nrExtractResources(sssIndices,rxGrid);

% Correlate received SSS symbols with each possible SSS sequence
sssEst = zeros(1,336);
for NID1 = 0:335

    ncellid = (3*NID1) + NID2;
    sssRef = nrSSS(ncellid);
    sssEst(NID1+1) = sum(abs(mean(sssRx .* conj(sssRef),1)).^2);

end

% Plot SSS correlations
figure;
stem(0:335,sssEst,'o');
title('SSS Correlations (Frequency Domain)');
xlabel('$N_{ID}^{(1)}$','Interpreter','latex');
ylabel('Magnitude');
axis([-1 336 0 max(sssEst)*1.1]);

% Determine NID1 by finding the strongest correlation
NID1 = find(sssEst==max(sssEst)) - 1;

% Plot selected NID1
hold on;
plot(NID1,max(sssEst),'kx','LineWidth',2,'MarkerSize',8);
legend(["correlations" "$N_{ID}^{(1)}$ = " + num2str(NID1)],'Interpreter','latex');

% Form overall cell identity from estimated NID1 and NID2
ncellid = (3*NID1) + NID2;

disp([' Cell identity: ' num2str(ncellid)])

%% PBCH DM-RS search
% In a process similar to SSS search, the receiver constructs each possible
% PBCH DM-RS sequence and performs channel and noise estimation. The index
% of the PBCH DM-RS with the best SNR determines the LSBs of the SS/PBCH
% block index required for PBCH scrambling initialization.

% Calculate PBCH DM-RS indices
dmrsIndices = nrPBCHDMRSIndices(ncellid);

% Perform channel estimation using DM-RS symbols for each possible DM-RS
% sequence and estimate the SNR
dmrsEst = zeros(1,8);
for ibar_SSB = 0:7
    
    refGrid = zeros([240 4]);
    refGrid(dmrsIndices) = nrPBCHDMRS(ncellid,ibar_SSB);
    [hest,nest] = nrChannelEstimate(rxGrid,refGrid,'AveragingWindow',[0 1]);
    dmrsEst(ibar_SSB+1) = 10*log10(mean(abs(hest(:).^2)) / nest);
    
end

% Plot PBCH DM-RS SNRs
figure;
stem(0:7,dmrsEst,'o');
title('PBCH DM-RS SNR Estimates');
xlabel('$\overline{i}_{SSB}$','Interpreter','latex');
xticks(0:7);
ylabel('Estimated SNR (dB)');
axis([-1 8 min(dmrsEst)-1 max(dmrsEst)+1]);

% Record ibar_SSB for the highest SNR
ibar_SSB = find(dmrsEst==max(dmrsEst)) - 1;

% Plot selected ibar_SSB
hold on;
plot(ibar_SSB,max(dmrsEst),'kx','LineWidth',2,'MarkerSize',8);
legend(["SNRs" "$\overline{i}_{SSB}$ = " + num2str(ibar_SSB)],'Interpreter','latex');

%% Channel Estimation using PBCH DM-RS and SSS
% The receiver estimates the channel for the entire SS/PBCH block using the
% SSS and PBCH DM-RS detected in previous steps. An estimate of the
% additive noise on the PBCH DM-RS / SSS is also performed.

refGrid = zeros([nrbSSB*12 4]);
refGrid(dmrsIndices) = nrPBCHDMRS(ncellid,ibar_SSB);
refGrid(sssIndices) = nrSSS(ncellid);
[hest,nest,hestInfo] = nrChannelEstimate(rxGrid,refGrid,'AveragingWindow',[0 1]);

%% PBCH Demodulation
% The receiver uses the cell identity to determine and extract the resource
% elements associated with the PBCH from the received grid. In addition,
% the receiver uses the channel and noise estimates to perform MMSE
% equalization. The equalized PBCH symbols are then demodulated and
% descrambled to give bit estimates for the coded BCH block.

disp(' -- PBCH demodulation and BCH decoding -- ')

% Extract the received PBCH symbols from the SS/PBCH block
[pbchIndices,pbchIndicesInfo] = nrPBCHIndices(ncellid);
pbchRx = nrExtractResources(pbchIndices,rxGrid);

% Configure 'v' for PBCH scrambling according to TS 38.211 Section 7.3.3.1
% 'v' is also the 2 LSBs of the SS/PBCH block index for L_max=4, or the 3
% LSBs for L_max=8 or 64.
if refBurst.L_max == 4
    v = mod(ibar_SSB,4);
else
    v = ibar_SSB;
end
ssbIndex = v;

% PBCH equalization and CSI calculation
pbchHest = nrExtractResources(pbchIndices,hest);
[pbchEq,csi] = nrEqualizeMMSE(pbchRx,pbchHest,nest);
Qm = pbchIndicesInfo.G / pbchIndicesInfo.Gd;
csi = repmat(csi.',Qm,1);
csi = reshape(csi,[],1);

% Plot received PBCH constellation after equalization
figure;
plot(pbchEq,'o');
xlabel('In-Phase'); ylabel('Quadrature')
title('Equalized PBCH Constellation');
m = max(abs([real(pbchEq(:)); imag(pbchEq(:))])) * 1.1;
axis([-m m -m m]);

% PBCH demodulation
pbchBits = nrPBCHDecode(pbchEq,ncellid,v,nest);

% Calculate RMS PBCH EVM
pbchRef = nrPBCH(pbchBits<0,ncellid,v);
evm = comm.EVM;
pbchEVMrms = evm(pbchRef,pbchEq);

% Display calculated EVM
disp([' PBCH RMS EVM: ' num2str(pbchEVMrms,'%0.3f') '%']);

%% BCH Decoding
% The receiver weights BCH bit estimates with channel state information
% (CSI) from the MMSE equalizer and decodes the BCH. BCH decoding consists
% of rate recovery, polar decoding, CRC decoding, descrambling, and
% separating the 24 BCH transport block bits from the 8 additional
% timing-related payload bits.

% Apply CSI
pbchBits = pbchBits .* csi;

% Perform BCH decoding including rate recovery, polar decoding, and CRC
% decoding. PBCH descrambling and separation of the BCH transport block
% bits 'trblk' from 8 additional payload bits A...A+7 is also performed:
%   A ... A+3: 4 LSBs of System Frame Number
%         A+4: half frame number
% A+5 ... A+7: for L_max=64, 3 MSBs of the SS/PBCH block index
%              for L_max=4 or 8, A+5 is the MSB of subcarrier offset k_SSB
polarListLength = 8;
[~,crcBCH,trblk,sfn4lsb,nHalfFrame,msbidxoffset] = ...
    nrBCHDecode(pbchBits,polarListLength,refBurst.L_max,ncellid);

% Display the BCH CRC
disp([' BCH CRC: ' num2str(crcBCH)]);

% Stop processing MIB and SIB1 if BCH was received with errors
if crcBCH
    disp(' BCH CRC is not zero.');
    return
end

% Use 'msbidxoffset' value to set bits of 'k_SSB' or 'ssbIndex', depending
% on the number of SS/PBCH blocks in the burst
if (refBurst.L_max==64)
    ssbIndex = ssbIndex + (bi2de(msbidxoffset.','left-msb') * 8);
    k_SSB = 0;
else
    k_SSB = msbidxoffset * 16;
end

% Displaying the SSB index
disp([' SSB index: ' num2str(ssbIndex)]);

%% MIB Parsing
% The example parses the 24 decoded BCH transport block bits into a
% structure which represents the MIB message fields. This process includes
% reconstituting the 10-bit system frame number (SFN) |NFrame| from the 6
% MSBs in the MIB and the 4 LSBs in the PBCH payload bits. It also includes
% incorporating the MSB of the subcarrier offset |k_SSB| from the PBCH
% payload bits in the case of L_max=4 or 8 SS/PBCH blocks per burst.

% Create set of subcarrier spacings signaled by the 7th bit of the decoded
% MIB, the set is different for FR1 (L_max=4 or 8) and FR2 (L_max=64)
if (refBurst.L_max==64)
    commonSCSs = [60 120];
else
    commonSCSs = [15 30];
end

% Create a structure of MIB fields from the decoded MIB bits. The BCH
% transport block 'trblk' is the RRC message BCCH-BCH-Message, consisting
% of a leading 0 bit then 23 bits corresponding to the MIB
mib.NFrame = bi2de([trblk(2:7); sfn4lsb] .','left-msb');
mib.SubcarrierSpacingCommon = commonSCSs(trblk(8) + 1);
mib.k_SSB = k_SSB + bi2de(trblk(9:12).','left-msb');
mib.DMRSTypeAPosition = 2 + trblk(13);
mib.PDCCHConfigSIB1 = bi2de(trblk(14:21).','left-msb');
mib.CellBarred = trblk(22);
mib.IntraFreqReselection = trblk(23);

% Display the MIB structure
disp(' BCH/MIB Content:')
disp(mib);

% Check if a CORESET for Type0-PDCCH common search space (CSS) is present,
% according to TS 38.213 Section 4.1
if ~isCORESET0Present(refBurst.BlockPattern,mib.k_SSB)
    fprintf('CORESET0 is not present (k_SSB > k_SSB_max).\n');
    return
end

%% OFDM Demodulation on Full Bandwidth
% Once the MIB is recovered, the receiver uses common subcarrier spacing
% and a bandwidth supporting CORESET0 to OFDM demodulate the frame
% containing the detected SS block. The receiver determines the CORESET0
% frequency resources in common numerology through an offset from the
% location of the SSB detected and a bandwidth specified in TS 38.213
% Section 13 Tables 13-1 through 13-10 [ <#19 5> ]. The frequency
% correction process aligned the center of the OFDM resource grid with the
% center frequency of the SS burst. However, these centers are not
% necessarily aligned with the center frequency of CORESET0. This figure
% shows the relationship between the SSB, CORESET0 frequency resources and
% associated PDCCH monitoring occasions.
%
% <<../CellSearchExampleCORESET0.png>>
%
% Unlike the SS burst, control and data channels must be aligned in
% frequency with their common resource block (CRB) raster. The value of
% KSSB in the MIB signals the frequency offset of the SSB from that CRB
% raster. As the frequency correction process centered the SSB in
% frequency, apply a frequency shift determined by |k_SSB| to align data
% and control channels with their CRB before OFDM demodulation

if (refBurst.L_max==64)
    scsKSSB = mib.SubcarrierSpacingCommon;
else
    scsKSSB = 15;
end
k_SSB = mib.k_SSB;
kFreqShift = k_SSB*scsKSSB*1e3;
rxWaveform = rxWaveform.*exp(1i*2*pi*kFreqShift*(0:length(rxWaveform)-1)'/rxSampleRate);

% Adjust timing offset to the frame origin
frameOffset = hTimingOffsetToFrame(refBurst,timingOffset,ssbIndex,rxSampleRate);

% If the frame offset is negative, the frame of interest is incomplete. Add
% leading zeros to the waveform to align the wavefom to the frame
display(frameOffset)

%% Appendix
% This example uses these helper functions:
% 
% * <matlab:edit('hCORESET0Resources.m') hCORESET0Resources.m>
% * <matlab:edit('hMCS.m') hMCS.m>
% * <matlab:edit('hPDCCH0Configuration.m') hPDCCH0Configuration.m>
% * <matlab:edit('hPDCCH0MonitoringOccasions.m') hPDCCH0MonitoringOccasions.m>
% * <matlab:edit('hSIB1PDSCHConfiguration.m') hSIB1PDSCHConfiguration.m>
% * <matlab:edit('hPDSCHTimeAllocationTables.m') hPDSCHTimeAllocationTables.m>
% * <matlab:edit('hSIB1WaveformConfiguration.m') hSIB1WaveformConfiguration.m>
% * <matlab:edit('hSIB1Boost.m') hSIB1Boost.m>
% * <matlab:edit('hSSBurstFrequencyCorrect.m') hSSBurstFrequencyCorrect.m>
% * <matlab:edit('hSSBurstStartSymbols.m') hSSBurstStartSymbols.m>
% * <matlab:edit('hSSBurstSubcarrierSpacing.m') hSSBurstSubcarrierSpacing.m>
% * <matlab:edit('hSystemInformationDCIFieldsSize.m') hSystemInformationDCIFieldsSize.m>

%% References
% # 3GPP TS 38.101-1. "NR; User Equipment (UE) radio transmission and
% reception; Part 1: Range 1 Standalone" _3rd Generation Partnership
% Project; Technical Specification Group Radio Access Network_.
% # 3GPP TS 38.104. "NR; Base Station (BS) radio transmission and
% reception." _3rd Generation Partnership Project; Technical
% Specification Group Radio Access Network_.
% # 3GPP TS 38.211. "NR; Physical channels and modulation."
% _3rd Generation Partnership Project; Technical Specification Group Radio
% Access Network_.
% # 3GPP TS 38.212. "NR; Multiplexing and channel coding."
% _3rd Generation Partnership Project; Technical Specification Group Radio
% Access Network_.
% # 3GPP TS 38.213. "NR; Physical layer procedures for control." _3rd
% Generation Partnership Project; Technical Specification Group Radio
% Access Network_.
% # 3GPP TS 38.214. "NR; Physical layer procedures for data." _3rd
% Generation Partnership Project; Technical Specification Group Radio
% Access Network_.
% # 3GPP TS 38.321. "NR; Medium Access Control (MAC) protocol
% specification." _3rd Generation Partnership Project; Technical
% Specification Group Radio Access Network_.

%% Local functions

function present = isCORESET0Present(ssbBlockPattern,kSSB)

    switch ssbBlockPattern
        case {'Case A','Case B','Case C'} % FR1
            kssb_max = 23;
        case {'Case D','Case E'} % FR2
            kssb_max = 11;
    end
    if (kSSB <= kssb_max)
        present = true;
    else
        present = false;
    end
    
end

function dci = hDCI(dcispec,dcibits)

    % Parse DCI message into a structure of DCI message fields
    fieldsizes = structfun(@(x)x,dcispec);
    fieldbits2dec = @(x,y)bin2dec(char(x(y(1):y(2)) + '0'));
    fieldbitranges = [[0; cumsum(fieldsizes(1:end-1))]+1 cumsum(fieldsizes)];
    fieldbitranges = num2cell(fieldbitranges,2);
    values = cellfun(@(x)fieldbits2dec(dcibits.',x),fieldbitranges,'UniformOutput',false);
    dci = cell2struct(values,fieldnames(dcispec));
    
end

function timingOffset = hTimingOffsetToFrame(burst,offset,ssbIdx,rxSampleRate)
    
    % As the symbol lengths are measured in FFT samples, scale the symbol
    % lengths to account for the receiver sample rate. Non-integer delays
    % are approximated at the end of the process.
    scs = hSSBurstSubcarrierSpacing(burst.BlockPattern);
    ofdmInfo = nrOFDMInfo(1,scs,'SampleRate',rxSampleRate); % smallest FFT size for SCS-SR
    srRatio = rxSampleRate/(scs*1e3*ofdmInfo.Nfft);
    symbolLengths = ofdmInfo.SymbolLengths*srRatio;
    
    % Adjust timing offset to the start of the SS block. This step removes
    % the extra offset introduced in the reference grid during PSS search,
    % which contained the PSS in the second OFDM symbol.
    offset = offset + symbolLengths(1);
    
    % Timing offset is adjusted so that the received grid starts at the
    % frame head i.e. adjust the timing offset for the difference between
    % the first symbol of the strongest SSB, and the start of the frame
    burstStartSymbols = hSSBurstStartSymbols(burst.BlockPattern,burst.L_max); % Start symbols in SSB numerology
    ssbFirstSym = burstStartSymbols(ssbIdx+1); % 0-based
    
    % Adjust for whole subframes
    symbolsPerSubframe = length(symbolLengths);
    subframeOffset = floor(ssbFirstSym/symbolsPerSubframe);
    samplesPerSubframe = sum(symbolLengths);
    timingOffset = offset - (subframeOffset*samplesPerSubframe);
    
    % Adjust for remaining OFDM symbols and round offset if not integer
    symbolOffset = mod(ssbFirstSym,symbolsPerSubframe);
    timingOffset = round(timingOffset - sum(symbolLengths(1:symbolOffset)));
    
end

function highlightSSBlock(refBurst,ssbIndex,commonNRB,scs,kFreqShift)
    
    scsSSB = scs(1);
    scsCommon = scs(2);
    
    % Determine frequency origin of the SSB in common numerology
    bounding_box = @(y,x,h,w)rectangle('Position',[x+0.5 y-0.5 w h],'EdgeColor','r');
    scsRatio = scsSSB/scsCommon;
    ssbFreqOrig = 12*(commonNRB-20*scsRatio)/2+1+kFreqShift/(scsCommon*1e3);
    
    % Determine time origin of the SSB in common numerology
    ssbStartSymbols = hSSBurstStartSymbols(refBurst.BlockPattern,refBurst.L_max);
    ssbHeadSymbol = ssbStartSymbols(ssbIndex+1)/scsRatio;
    ssbTailSymbol = floor((ssbStartSymbols(ssbIndex+1)+4)/scsRatio)-1;

    bounding_box(ssbFreqOrig,ssbHeadSymbol,240*scsRatio,ssbTailSymbol-ssbHeadSymbol+1);
    
    str = sprintf('Strongest \n SSB: %d',ssbIndex);
    text(ssbHeadSymbol,ssbFreqOrig-20,0, str,'FontSize',10,'Color','w')

end
