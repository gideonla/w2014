function [pk, iters] = pcg_steihaug(f, g, B, delta, m_solv)
    % m_solv is a function that computes M\x
    epsilon = min(0.005, sqrt(norm(g))) * norm(g);
    z = zeros(length(g), 1);
    r = g;
    d = -m_solv(g);
    y = -d;
    if norm(r) < epsilon
        pk = zeros(length(g));
        return;
    end
    iters = 0;
    while (1)
        iters = iters + 1;
        dbd = d.' * B * d;
        if dbd <= 0
            [tau1, tau2] = findtaus(z, d, delta);
            pk1 = z + tau1 * d;
            pk2 = z + tau2 * d;
            if eval_model(f, g, B, pk1) >= eval_model(f, g, B, pk2)
                pk = pk2;
                return;
            else
                pk = pk1;
                return;
            end
        end
        
        alpha = r.' * y / dbd;
        
        old_z = z;
        z = z + alpha * d;
        
        if norm(z) >= delta
            [tau1, tau2] = findtaus(old_z, d, delta);
            if tau1 >= 0
                pk = old_z + tau1 * d;
                return;
            else
                pk = old_z + tau2 * d;
                return;
            end
        end
        
        old_r = r;
        r = r + alpha * B * d;
        if norm(r) < epsilon
            pk = z;
            return;
        end
        old_y = y;
        y = m_solv(r);
        
        beta = (r.' *  y) / (old_r.' * old_y);
        d = -y + beta * d;
    end
end

% Finds tau1, tau2 such that norm(z + tau{1,2}*d) == delta
function [tau1, tau2] = findtaus(z, d, delta)
    c = norm(z)^2 - delta^2;
    b = 2 * z.' * d;
    a = norm(d)^2;
    tau1 = (-b + sqrt(b^2 - 4*a*c)) / (2*a);
    tau2 = (-b - sqrt(b^2 - 4*a*c)) / (2*a);
end

function m = eval_model(f, g, B, x)
    m = f + g.' * x + 0.5 * x.' * B * x;
end
