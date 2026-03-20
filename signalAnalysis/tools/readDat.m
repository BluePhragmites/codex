% filename = '20211020002_sync_huawei_30720000.dat';
% flag 
%   0 -> real
%   1 -> complex
% length
%   flag real :*1
%   flag complex :*2
% precision:
% 'float=>double'
% 'int16=>double'
% cnt:
%   recommended to use a small value first for large raw files
%   if not explicitly required, start with about 10 to 20 frames
%   cnt = 0 means read until the end of file
% dataFormat:
%   0 -> output name keeps the 1024-frame cycle index and frame index
%        <captureId>_<varname>_<superFrameIdx>_<frameIdx>.mat
%   1 -> output name uses continuous frame numbering across 1024-frame cycles
%        <captureId>_<varname>_000_<continuousFrameIdx>.mat
% readDat("202507180003_syncRx_.dat",0,20*4096/64*15*2,0*8,"uint8=>double",0,0)
function readDat(filename,flag,length,offsetlen,precision,cnt,frameIdx,dataFormat)
    if nargin==5
        cnt = 0;
        frameIdx = 0;
        dataFormat = 0;
    elseif nargin==6
        frameIdx = 0;
        dataFormat = 0;
    elseif nargin==7
        dataFormat = 0;
    end
    fp = fopen(filename,'rb');
    %length = 20*1024*15*2;
    iTemp = 1;
    %offsetlen = 1024;
    fseek(fp,offsetlen,-1);
    while 1
        data = fread(fp,length,precision);
        if isempty(data)
            break;
        end
        if flag == 1
            complexData = complex(data(1:2:end),data(2:2:end));
        else
            complexData = data;
        end
        strcell = strsplit(filename,'_');
        varname = strcell{2};
        dataname = strcell{1};
        expEq = sprintf('%s = complexData;',varname);
        eval(expEq);
        % savename = sprintf('%s_%s_%05d.mat',dataname,varname,iTemp+frameIdx-1);
        if dataFormat == 0
            % Keep the original NR 1024-frame cycle structure in the filename.
            savename = sprintf("%s_%s_%03d_%05d.mat",dataname,varname,floor((iTemp+frameIdx-1)/1024),mod(iTemp+frameIdx-1,1024));
        else
            % Use continuous frame numbering so adjacent frames stay consecutive.
            savename = sprintf("%s_%s_%03d_%05d.mat",dataname,varname,0,iTemp+frameIdx-1);
        end

        save(savename,varname);
        iTemp = iTemp+1;
        if cnt~=0 && (iTemp-1)>=cnt
            break;
        end
    end
    fclose(fp);
end
