function [cpLen,symbolLen,frameOffset,cpLenAll,symbolLenALL] = nr_OFDMInfo(nrb,scs,rxSampleRate,slotIdx,cpType)
% nrb: number resouce block
%
% scs : subcarrier space
%    15,30,60,120,240
    cpLenAll = [];
    symbolLenALL = [];
    fftNum = 1;
    while 1
        if fftNum<nrb*12
            fftNum = fftNum *2;
        else
            break;
        end
    end

    mu = ceil(log2(scs/15));
    if 2^mu*15~=scs
        error("scs = %d,error!\n",scs)
    end


    if nargin == 2
        rxSampleRate = scs*1e3*fftNum;
        slotIdx = 0;
        cpType = 'normal';
    elseif nargin == 3
        fftNum = rxSampleRate/scs/1e3;
        slotIdx = 0;
        cpType = 'normal';
    elseif nargin == 4
        cpType = 'normal';
    end

    slotNum = 10*2^mu;
    if slotIdx>= slotNum
        error("scs = %d, slotNum =%d error!\n",scs,slotNum)
    end
    switch cpType
        case 'normal'
            longCp = 144*2^-mu + 16;
            shortCp = 144*2^-mu;
        case 'extended'
            longCp = 512;
            shortCp = 512;
        otherwise
            error("cpType :%d error!\n",cpType);
    end

    rate = ceil(rxSampleRate/(scs*fftNum*1e3));
    fac = rxSampleRate/(2048*15e3);
    longCp = fac*longCp;
    shortCp = fac*shortCp;

    if rate*scs*fftNum*1e3~=rxSampleRate
        error("rxSampleRate :%d error!\n",rxSampleRate);
    end

    cpLenAll = zeros(14*slotNum,1);
    symbolLenALL = zeros(14*slotNum,1);
    for l = 0:1:14*slotNum-1
        if mod(l,7*2^mu)==0
            cpLenAll(l+1) = longCp;
            symbolLenALL(l+1) = longCp+fftNum;
        else
            cpLenAll(l+1) = shortCp;
            symbolLenALL(l+1) = shortCp+fftNum;
        end
    end
    cpLenAll = cpLenAll * rate;
    symbolLenALL = symbolLenALL * rate;

    %%
    cpLen = cpLenAll(slotIdx*14+[1:14]).';
    symbolLen = symbolLenALL(slotIdx*14+[1:14]).';
    frameOffset = sum(symbolLenALL(1:slotIdx*14));
    cpLenAll = cpLenAll.';
    symbolLenALL = symbolLenALL.';
end
