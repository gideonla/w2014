% Parameters: 
% start is some arbitrary starting point, specifying x, y, and lambda
% epsilon is the stopping tolerance
% M and q are the data matrix and label vector
 
function x = p3_int_point(M, q, start, C, epsilon)
    % First compute the necessary matrices
    [n, m] = size(M);
    G = diag([ones(m,1); zeros(n+1,1)]);
    c = [zeros(m+1, 1); C * ones(n, 1)];
    A = [bsxfun(@times, q, M) q eye(n); zeros(n, m) zeros(n, 1) eye(n)];
    b = [ones(n, 1); zeros(n, 1)];
    
    x = start(1:m+n+1);
    y = start(m+n+2:m+3*n+1);
    lambda = start(m+3*n+2:m+5*n+1);
    
    % Do the recommended correction to the starting point
    [dxaff, dyaff, dlaff] = compute_affine_scaling(G, A, b, c, x, y, lambda);

    y = max(1, abs(y + dyaff));
    lambda = max(1, abs(lambda+dlaff));
    
    % Start looping
    while 1
        grad_l = G*x - A.' * lambda + c;
        norm(grad_l)
        if norm(grad_l) < epsilon && norm(A*x - y - b) < epsilon && norm(y .* lambda) < epsilon
            break;
        end
        [dxaff, dyaff, dlaff] = compute_affine_scaling(G, A, b, c, x, y, lambda);
        mu = y.' * lambda / n;
        aaff = get_maxmult([y; lambda], [dyaff; dlaff]);
        muaff = (y + aaff*dyaff).' * (lambda + aaff*dlaff) / n;
        sigma = (muaff / mu)^3;
        [dx, dy, dl] = compute_step(G, A, b, c, x, y, lambda, dyaff, dlaff, sigma, mu);
   
        tau = 0.8;
        alpha = get_maxmult([tau * y; tau * lambda], [dy; dl]);
        x = x + alpha * dx;
        y = y + alpha * dy;
        lambda = lambda + alpha * dl;
    end
end

% Finds greatest alpha such that a-alpha*da>=0
function alpha = get_maxmult(a, da)
    rs = -a ./ da;
    rs(rs <= 0) = 1;
    alpha = min(rs);
end

function [dxaff, dyaff, dlaff] = compute_affine_scaling(G, A, b, c, x, y, lambda)
    M2 = bsxfun(@times, lambda./y, A);
    M2 = A.' * M2;
    X = G + M2;
    
    rd = G*x - A.'*lambda + c;
    rp = A*x - y - b;
    
    v1 = lambda .* (-rp - y) ./ y;
    v1 = -rd + A.' * v1;
    
    dxaff = X \ v1;
    dlaff = -lambda .* (y + (A * dxaff) + rp) ./ y;
    dyaff = A * dxaff + rp;
end

function [dx, dy, dl] = compute_step(G, A, b, c, x, y, lambda, dyaff, dlaff, sigma, mu)
    M2 = bsxfun(@times, lambda./y, A);
    M2 = A.' * M2;
    X = G + M2;

    rd = G*x - A.'*lambda + c;
    rp = A*x - y - b;
    rl = -lambda .* y - dyaff .* dlaff + sigma*mu*ones(length(y), 1);
    
    v = -rd + A.' * (rl ./ y) - A.' * (lambda .* rp ./ y);
    
    dx = X \ v;
    dl = (rl  - lambda.*(A * dx) - lambda .* rp) ./ y;
    dy = A * dx + rp;
end
