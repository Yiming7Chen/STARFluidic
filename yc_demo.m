
fclose all
instrreset
clear all
close all%% initiate serial communication with the arduino board on fluidic syste5m

%% initialization
STARFluidic=STARmapFluidic();
STARFluidic=STARFluidic.setSerial('COM5'); %connect to serialport
pause(5)
STARFluidic=STARFluidic.resetFluidic(); %initiate fluidic
% [STARFluidic,v_temperature,vt_temperature]=STARFluidic.wait_chiller(4); %cool the chiller to 4C


% instrfind %%list all serial ports 

%% initiate micro-manager
%{
addpath(genpath('c:\program files\Micro-Manager-2.0beta'));
import mmcorej.*; % make sure micro-manager folder is in the path
mmc=CMMCore;
cfgpath=which('MMConfig_demo.cfg')
mmc.loadSystemConfiguration (cdfpath);
%}

