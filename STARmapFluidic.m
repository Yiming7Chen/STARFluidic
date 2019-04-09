classdef STARmapFluidic
    %UNTITLED2 Summary of this class goes here
    %   Detailed explanation goes here
    
    properties
        %fluidic
        serialport;
        
        %microscope
        mm_path; %micro-manager path
        return_msg; % last return msg
        mmc; %micro-manager core
        stepsperul=15.36; %take this amount of step motor steps to push/pull 1ul
        
        %{
        Here is a list of common commands to send to the fluidic system (reference for development)
        
        Core Macro
        'M0' - reset the fluidic
            SERIAL RETURN 'ok'
        'M5' - empty syringe to waste
            SERIAL RETURN 'ok'
        'M6' - push/pull liquid through certain pathway
                pulL/pusL, byPass/Flow cell, Wait/No wait, volumne, speed, port
            SERIAL RETURN 'ok'
        
        Chiller
        'Cs' - chiller status
            SERIAL RETURN 'on'/'off'
        'Ci' - chiller idol
            SERIAL RETURN 'ok'
        'Cp' - chiller
            SERIAL RETURN 'n' (temperature)
        'Cnxx.x' - chiller set temperature
            SERIAL RETURN 'ok'
        
        %}
        
        
        
    end
    
    methods
        %% common
        function obj = setSerial(obj,sp,br)
            % initiate the class parameters
            % sp [string] - serial port
            % br - baudrate
            if nargin<3
                br=9600;
                fprintf('Set baudrate to defult, which is 9600\n')
            end
            s = serial(sp);
            fopen(s);
            set(s,'BaudRate',br);
            obj.serialport=s;
            fprintf('Communicate fluidic system through %s. BaudRate = %d\n ',sp, br);
        end
        %% fluidic functions
        function obj = resetFluidic(obj)
            % set fluidic system 'Macro0'
            obj.return_msg=send_command(obj.serialport,'M0');
            fprintf(strcat('Reset Fluidic. RETURN -\t', obj.return_msg));
        end
        
        function obj = send_command(obj,cmd)
            % format and send command
            obj.return_msg=send_command(obj.serialport,cmd);
            fprintf(strcat('Sent command. RETURN -\t', obj.return_msg));
        end
        
        
        function obj = load_through_bypass(obj, valve, amount_ul, speed)
            % load liquid from valve into syringe and then into flowcell
            if nargin<4
                speed=50;
            end
            load_amt = round(amount_ul * obj.stepsperul);
            empty_syringe(obj.serialport)
            % load into syringe
            cmd = sprintf('M6,L,P,W,%d,%d,%s',load_amt, speed, valve);
            obj.return_msg=send_command(obj.serialport, cmd);
            wait_pressure(obj.serialport,50); %wait pressure to decay
            % push through flowcell
            cmd = sprintf('M6,H,F,W,%d,%d,%s',load_amt, speed, '01');
            obj.return_msg=send_command(obj.serialport, cmd);
            wait_pressure(obj.serialport,50); %wait pressure to decay
            empty_syringe(obj.serialport);
        end
        
        
        function obj = load_through_flowcell(obj, valve, amount_ul, speed, threshold)
            % load liquid directly into flow cell
            if nargin<4
                speed=50;
            end
            
            if nargi<5
                threshold=25;
            end
            
            empty_syringe(obj.serialport); % empty the syringe first
            load_amt = amount_ul * obj.stepsperul;
            %             % L [pull] through P [bypass] with W [wait]
            cmd = sprintf('M6,L,P,W,%d,%d,%s', load_amt, speed, valve);
            obj.return_msg=send_command(obj.serialport, cmd);
            wait_pressure(obj.serialport, threshold); %wait till pressure goes below threshold
        end
        
        function obj = empty_syringe(obj)
            obj.return_msg=empty_syringe(obj.serialport);
        end
        
        
        function [obj, vtemperature,vtime] = wait_chiller(obj,t_celsius,threshold_celsius)
            %start chiller and wait till it is set to temperature
            %targetTemperature [char] or [num] - in n.x format. Temperature to set to
            %wait till the temperature goes around the target temperature
            %or below threshold_celsius
            if nargin<3
                threshold_celsius=t_celsius;
            end
            tic
            vtemperature=[]; %record temperature
            vtime=[]; %relative time of tempearture record
            
            %format input and send command
            if isnumeric(t_celsius)
                cmd=sprintf('Cn%.1f',t_celsius);
            else
                cmd=['Cn' t_celsius];
                t_celsius=str2num(t_celsius);
            end
            
            obj.return_msg=send_command(obj.serialport,cmd);
            fprintf(strcat('Chiller RETURN -\t', obj.return_msg));
            %record temperature and also wait temperature to go lower than
            %threshold
            figure
            c_celsius=t_celsius+1;
            while c_celsius>threshold_celsius+0.4 % allow 0.4C error
                c_celsius=str2num(send_command(obj.serialport,'Cp'));
                vtemperature=[vtemperature c_celsius];
                vtime=[vtime toc];
                if mod(length(vtime),100)==0
                    plot(vtime,vtemperature);
                    drawnow
                end
            end
            
            fprintf('The Chiller is Chill @ %d Celsius', c_celsius)
            
        end
        
        %% micro-manager functions
        function obj = setMicroManager(obj,mmpath,cfgname)
            addpath(genpath(mmpath));
            import mmcorej.*;
            obj.mm_path=mmpath;
            obj.mmc=CMMCore;
            cfgpath=which(cfgname);
            pause(5); %not sure why but this avoids failToOpenFile error
            obj.mmc.loadSystemConfiguration (cfgpath);
            
        end
        
        function img = snapImage(obj)
%             keyboard();
            obj.mmc.snapImage();
            img = obj.mmc.getImage();  % returned as a 1D array of signed integers in row-major order
            width = obj.mmc.getImageWidth();
            height = obj.mmc.getImageHeight();
            if obj.mmc.getBytesPerPixel == 2
                pixelType = 'uint16';
            else
                pixelType = 'uint8';
            end
            img = typecast(img, pixelType);      % to proper data type
            img = reshape(img, [width, height]); % vector to 2D img
            img = img';       
            
        end
        
    end
    
end
%% private functions

function return_msg=send_command(sp,cmdstr)
%sp - serialport
%cmdstr - command string
cmdstr=sprintf('%s\r\n',cmdstr);
fprintf(sp,cmdstr);
return_msg=fgetl(sp);

end


function return_msg=empty_syringe(sp,outputroute)
%sp - serialport
%outputroute - 'waste'/'bleach'
if nargin<2
    outputroute='waste';
end
if strcmp(outputroute,'bleach')
    send_command(sp,'M5,B');
else if strcmp(outputroute,'waste')
        send_command(sp,'M5,W');
    end
end
return_msg=fgetl(sp);



end

function [vpressure,vtime]=wait_pressure(sp,threshold)
%sp - serial port
%threshold - threshold to stop waiting
%consider adding timeoutcheck variable- check if the mBar still changing after certain timethreshold
pressure=threshold+1;
tic
while abs(pressure)>threshold
    pressure=get_rel_pressure(sp);
    vpressure=[vpressure pressure];
    vtime=[vtime toc];
    %     if nargin==3
    %         if toc >timeoutcheck
    %             temp=find(vtime<vtime(end)-60);
    %             temp=temp(end);
    %             if abs(vpressure(temp)-vpressure(end))<10 % if pressure didnt change more than 10mBar in the last minute
    %                 break
    %             end
    %         end
    %     end
end

end

function hexstring=num2hex(input)

if isnumeric(input) & 1<=input<=24
    hexstring=num2str(input);
    hexstring=dec2hex(hexstring);
else
    hexstring=input;
    fprintf('expecting numeric input ranging from 1 to 24')
end
end

function pressure=get_rel_pressure(sp)
pressure=send_command(sp,'Tr');
pressure=str2num(pressure);
end

function obj = pullfromvalve(obj)
end

