% filename = '20211020002_sync_huawei_30720000.dat';
% precision:
% 'float=>double'
% 'int16=>double'
% writeDat('20250508001_syncRx_.dat',syncRx,"int16")
% 
function writeDat(filename,data,precision)
    fp = fopen(filename,'wb');
    if isreal(data)
        complexData = data;
    else
        complexData = zeros(length(data)*2,1);
        complexData(1:2:length(data)*2) = real(data);
        complexData(2:2:length(data)*2) = imag(data);
    end
    % complexData = complexData/max(abs(complexData))/1.2;
    % complexData = complexData/max(abs(complexData));
    fwrite(fp,complexData,precision);
    fclose(fp);
end