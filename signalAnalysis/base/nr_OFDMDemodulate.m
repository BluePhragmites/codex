function nrGrid = nr_OFDMDemodulate(syncRx,nrb,fftNum,cpLen,symbolLen,slotNum)
    if nargin == 4
        symbolLen = cpLen+fftNum;
        slotLen = sum(symbolLen);
        slotNum = floor(length(syncRx)/slotLen);
    elseif nargin == 5
        slotLen = sum(symbolLen);
        slotNum = floor(length(syncRx)/slotLen);
        
        if slotNum==0
            symbolSize = floor(length(syncRx)/sum(symbolLen(1:14)));
            symbolLen = symbolLen(1:symbolSize*14);
            cpLen = cpLen(1:symbolSize*14);
            slotLen = sum(symbolLen);
            slotNum = floor(length(syncRx)/slotLen);
        end
    end


    symolLen = floor(length(cpLen)/14);
    if symolLen*14~=length(cpLen)
        error("cpLen %d\n",length(cpLen));
    end

    nrGrid = zeros(nrb*12,14*symolLen);
    dataIdx = 0;
    for iTemp = 0:1:slotNum-1
        for jTemp = 1:1:symolLen*14
            dataIdx = dataIdx + cpLen(jTemp);
            timeData = syncRx(dataIdx+[1:fftNum]);
            freData = fftshift(fft(timeData));
            freqStartIdx = (fftNum-nrb*12)/2;
            freDataRb = freData(freqStartIdx+[1:nrb*12]);
            nrGrid(:,jTemp+iTemp*symolLen*14) = freDataRb;
            dataIdx = dataIdx + fftNum;
        end
    end
end
