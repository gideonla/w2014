function [f,g,H]=fenton_wrap(x,nrderivs);
% USAGE:    [f,g,H]=fenton_wrap(x,nrderivs)
% PURPOSE:  a wrapper around fenton's function
% INPUT:    x:          (vector) input value 
%           nrderivs:   (integer) the number of derivs needed. 
%                       0: return only function 
%                       1: return function  and gradient.
%                       2+: return function, gradient, and Hessian H. 
% OUTPUT:   f:          (scalar) Function value
%           g:          (vector) Gradient
%           H:          (matrix) Hessian

% consistency tests
if length(x)~=2
    % throw an error, function not defined
    error('Fentons function has exactly two arguments');
end
    
% choose actual derivative info needed
switch nrderivs
    case 0,
        f=fenton(x);
    case 1,
        gradientBundle=fenton(gradientinit(x));
        f=gradientBundle.x;
        g=gradientBundle.dx';
    case 2, 
        hessianBundle=fenton(hessianinit(x)); 
        f=hessianBundle.x;
        g=hessianBundle.dx';
        H=hessianBundle.hx;
    otherwise,
        % throw an error, case not defined
        error('This derivative option is not defined');
end

        
