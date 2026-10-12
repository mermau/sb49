#include "Circuits.h"
#include <algorithm>

namespace perf::circuit
{
// ======================================================================================
void LinearCircuit::prepare (double sampleRate)
{
    T = 1.0 / sampleRate;
    const size_t n = (size_t) numNodes;
    M.assign (n * n, 0.0);
    rhs.assign (n, 0.0);
    x.assign (n, 0.0);
    perm.assign (n, 0);
    isAmpRow.assign (n, 0);
    for (auto& a : amps) isAmpRow[(size_t) a.out] = 1;
    for (auto& c : caps) c.G = 2.0 * c.C / T;
    dirty = true;
    reset();
}

void LinearCircuit::reset()
{
    for (auto& c : caps) c.J = 0.0;
    std::fill (x.begin(), x.end(), 0.0);
}

void LinearCircuit::stamp (int a, int b, double g)
{
    const size_t n = (size_t) numNodes;
    if (a >= 0) M[(size_t) a * n + (size_t) a] += g;
    if (b >= 0) M[(size_t) b * n + (size_t) b] += g;
    if (a >= 0 && b >= 0)
    {
        M[(size_t) a * n + (size_t) b] -= g;
        M[(size_t) b * n + (size_t) a] -= g;
    }
}

void LinearCircuit::build()
{
    const size_t n = (size_t) numNodes;
    std::fill (M.begin(), M.end(), 0.0);
    for (auto& r : res) stamp (r.a, r.b, r.g);
    for (auto& c : caps) stamp (c.a, c.b, c.G);
    for (auto& i : ins) M[(size_t) i.n * n + (size_t) i.n] += i.g;
    for (auto& a : amps)
    {
        double* row = &M[(size_t) a.out * n];
        std::fill (row, row + n, 0.0);
        row[(size_t) a.plus] += 1.0;
        row[(size_t) a.minus] -= 1.0;
    }
    // fattorizzazione LU con pivoting parziale
    for (size_t i = 0; i < n; ++i) perm[i] = (int) i;
    for (size_t k = 0; k < n; ++k)
    {
        size_t p = k;
        double best = std::abs (M[k * n + k]);
        for (size_t i = k + 1; i < n; ++i)
            if (std::abs (M[i * n + k]) > best) { best = std::abs (M[i * n + k]); p = i; }
        if (p != k)
        {
            for (size_t j = 0; j < n; ++j) std::swap (M[k * n + j], M[p * n + j]);
            std::swap (perm[k], perm[p]);
        }
        const double piv = M[k * n + k];
        for (size_t i = k + 1; i < n; ++i)
        {
            const double f = M[i * n + k] / piv;
            M[i * n + k] = f;
            for (size_t j = k + 1; j < n; ++j) M[i * n + j] -= f * M[k * n + j];
        }
    }
    dirty = false;
}

void LinearCircuit::tick (const double* sources)
{
    if (dirty) build();
    const size_t n = (size_t) numNodes;
    std::fill (rhs.begin(), rhs.end(), 0.0);
    for (auto& i : ins) rhs[(size_t) i.n] += i.g * sources[i.src];
    for (auto& c : caps)
    {
        if (c.a >= 0) rhs[(size_t) c.a] += c.J;
        if (c.b >= 0) rhs[(size_t) c.b] -= c.J;
    }
    for (size_t i = 0; i < n; ++i) if (isAmpRow[i]) rhs[i] = 0.0;

    // sostituzione in avanti e all'indietro
    for (size_t i = 0; i < n; ++i)
    {
        double s = rhs[(size_t) perm[i]];
        for (size_t j = 0; j < i; ++j) s -= M[i * n + j] * x[j];
        x[i] = s;
    }
    for (size_t i = n; i-- > 0;)
    {
        double s = x[i];
        for (size_t j = i + 1; j < n; ++j) s -= M[i * n + j] * x[j];
        x[i] = s / M[i * n + i];
    }
    for (auto& c : caps)
    {
        const double va = c.a >= 0 ? x[(size_t) c.a] : 0.0;
        const double vb = c.b >= 0 ? x[(size_t) c.b] : 0.0;
        c.J = 2.0 * c.G * (va - vb) - c.J;
    }
}

// ======================================================================================
StringsChain::StringsChain()
{
    auto& k = c;
    const int G = -1;
    // --- miscelatore 16'/8' e IC2 (preenfasi)
    const int N = k.node(), Mn = k.node(), P = k.node(), X = k.node(), Y = k.node(), O2 = k.node();
    k.input (N, 100e3, 0);           // R16 dal bus 8'
    k.input (N, 100e3, 1);           // R17 dal bus 16'
    k.capacitor (N, G, 470e-12);     // C11
    k.capacitor (N, Mn, 4.7e-9);     // C9
    k.resistor (Mn, P, 220e3);       // R55
    k.resistor (P, G, 1e6);          // R13
    k.opamp (O2, P, X);              // IC2
    k.resistor (O2, X, 4.7e3);       // R12
    k.resistor (X, Y, 470.0);        // R14
    k.capacitor (Y, G, 10e-9);       // C7
    // --- equalizzatore a giratori, IC5 non invertente
    const int A = k.node(), B = k.node(), OUT = k.node();
    k.resistor (O2, A, 2.2e3);       // R11
    struct Band { double Cs, Rg, Cg, Rq, Rser; };
    const Band bands[3] = {
        { 470e-9, 470.0, 12e-9, 82e3, 820.0 },  // LOW:    C1, R1, C2, R7, R6   (f0 ≈ 341 Hz)
        { 150e-9, 470.0, 6.8e-9, 68e3, 100.0 }, // MIDDLE: C3, R2, C4, R8, R10  (f0 ≈ 881 Hz)
        { 33e-9, 470.0, 3.3e-9, 47e3, 100.0 },  // HIGH:   C6, R3, C5, R9, R15  (f0 ≈ 3,2 kHz)
    };
    for (int i = 0; i < 3; ++i)
    {
        const int W = k.node(), Pg = k.node(), Q = k.node(), Og = k.node();
        rIdx[2 * i] = k.numResistors();     k.resistor (A, W, 5e3);
        rIdx[2 * i + 1] = k.numResistors(); k.resistor (W, B, 5e3 + bands[i].Rser);
        k.capacitor (W, Pg, bands[i].Cs);
        k.resistor (Pg, Og, bands[i].Rg);
        k.capacitor (Pg, Q, bands[i].Cg);
        k.resistor (Q, G, bands[i].Rq);
        k.opamp (Og, Q, Og);                 // inseguitore del giratore
    }
    k.opamp (OUT, A, B);                     // IC5
    k.resistor (OUT, B, 3.3e3);              // R18
    k.capacitor (OUT, B, 47e-12);            // C12 (letto "47n": 47 pF, vedi README)
    // --- R22/C13 e tre filtri d'ingresso BBD (impedenze divise per 3)
    const int E = k.node(), S1 = k.node(), S2 = k.node(), F = k.node();
    k.resistor (OUT, E, 4.7e3);              // R22
    k.capacitor (E, G, 10e-9);               // C13
    k.resistor (E, S1, 22e3 / 3.0);          // R19
    k.capacitor (S1, F, 3.0 * 8.2e-9);       // C11 (P551)
    k.resistor (S1, S2, 22e3 / 3.0);         // R21
    k.capacitor (S2, G, 3.0 * 120e-12);      // C14
    k.opamp (F, S2, F);                      // TR4 inseguitore di emettitore
    nOut = F;
    nEq = OUT;
    setEq (0.5, 0.5, 0.5);
}

void StringsChain::setEq (double low, double mid, double high)
{
    const double s[3] = { low, mid, high };
    const double ser[3] = { 820.0, 100.0, 100.0 };
    for (int i = 0; i < 3; ++i)
    {
        if (std::abs (s[i] - eq[i]) < 1.0e-4) continue;
        eq[i] = s[i];
        // potenziometro lineare 10K: cursore verso A = taglio, verso B = esaltazione
        c.setResistor (rIdx[2 * i], std::max (1.0, s[i] * 10e3));
        c.setResistor (rIdx[2 * i + 1], std::max (1.0, (1.0 - s[i]) * 10e3) + ser[i]);
    }
}

// ======================================================================================
void BrassEnvelope::reset()
{
    vC19 = 0.65 + (12.0 - 0.65) * 15e3 / (19.7e3 + 15e3); // C19 a riposo (TR16 interdetto)
    vC18 = 0.1; vC2 = 0.0; w = 0.0;
    sat = true; d1 = false; d2 = false;
}

double BrassEnvelope::process (bool gate, double decaySlider, double attackSlider, double rangeSlider, double modVolts)
{
    constexpr double R25 = 1.5e3, R22 = 15e3, R21 = 4.7e3, R23 = 15e3, C19 = 2.2e-6, C18 = 2.2e-6, C2 = 4.7e-6;
    constexpr double beta = 200.0, vSat = 0.1, vD = 0.6, rD2 = 10.0;
    const double Rd = 22e3 + 470e3 * logTaper (decaySlider);  // R26 + P1
    const double Ra = 100e3 + 470e3 * logTaper (attackSlider); // R27 + P5
    constexpr double Gs = 1.0 / 10e3 + 1.0 / 3.3e3 + 1.0 / 220e3 + 1.0 / 100e3;
    constexpr double kz = 1.0 / (10e3 * Gs);

    // RANGE: P2 22K lin tra R30 18K (da +12 V) e massa, filtrato da C13 10µ
    const double rs = std::clamp (rangeSlider, 0.0, 1.0);
    const double wTarget = 12.0 * rs * 22e3 / 40e3;
    const double rTop = 18e3 + (1.0 - rs) * 22e3, rBot = std::max (1.0, rs * 22e3);
    const double tauW = std::max (1.0e-4, 10e-6 * (rTop * rBot / (rTop + rBot)));

    double S = 0.0;
    for (int sub = 0; sub < kSub; ++sub)
    {
        w += (wTarget - w) * (1.0 - std::exp (-dt / tauW));
        const double Sext = (w / 220e3 + modVolts / 100e3) / Gs;

        // TR16 / C19 / TR15
        const double iin = gate ? (vSat - vC19) / R22 : (12.0 - vC19) / (R21 + R22);
        const double Ib = std::max (0.0, (vC19 - 0.65) / R23);
        vC19 += dt * (iin - Ib) / C19;
        const double Ic = beta * std::max (0.0, (vC19 - 0.65) / R23);

        // nodi Y (collettore TR15), X (C18/D1/D2/R26), Z (C2): Eulero implicito, stati a tratti
        const double G18 = C18 / dt, G2 = C2 / dt;
        double Vy = vSat, X = 0.0, Z = vC2;
        for (int it = 0; it < 6; ++it)
        {
            double a[3][4] {};
            if (sat) { a[0][0] = 1.0; a[0][3] = vSat; }
            else
            {
                a[0][0] = 1.0 / R25 + G18; a[0][1] = -G18;
                a[0][3] = 12.0 / R25 - Ic + G18 * vC18;
            }
            const double g1 = d1 ? 1.0 / Ra : 0.0, g2 = d2 ? 1.0 / rD2 : 0.0;
            a[1][0] = -G18; a[1][1] = G18 + 1.0 / Rd + g1 + g2; a[1][2] = -g1;
            a[1][3] = -G18 * vC18 + g1 * vD - g2 * vD;
            a[2][1] = -g1; a[2][2] = G2 + g1 + (1.0 - kz) / 10e3;
            a[2][3] = G2 * vC2 - g1 * vD + Sext / 10e3;
            // eliminazione di Gauss 3x3
            for (int k = 0; k < 3; ++k)
            {
                int p = k;
                for (int i = k + 1; i < 3; ++i) if (std::abs (a[i][k]) > std::abs (a[p][k])) p = i;
                if (p != k) for (int j = 0; j < 4; ++j) std::swap (a[k][j], a[p][j]);
                for (int i = k + 1; i < 3; ++i)
                {
                    const double f = a[i][k] / a[k][k];
                    for (int j = k; j < 4; ++j) a[i][j] -= f * a[k][j];
                }
            }
            Z = a[2][3] / a[2][2];
            X = (a[1][3] - a[1][2] * Z) / a[1][1];
            Vy = (a[0][3] - a[0][1] * X - a[0][2] * Z) / a[0][0];

            bool nsat = sat;
            if (sat)
            {
                const double need = (12.0 - Vy) / R25 - G18 * (Vy - X - vC18);
                if (need > Ic) nsat = false;
            }
            else if (Vy < vSat) nsat = true;
            const bool nd1 = (X - Z) > vD;
            const bool nd2 = X < -vD;
            if (nsat == sat && nd1 == d1 && nd2 == d2) break;
            sat = nsat; d1 = nd1; d2 = nd2;
        }
        vC18 = Vy - X;
        vC2 = Z;
        S = kz * Z + Sext;
    }
    return S;
}

// ======================================================================================
double Ladder::tick (double u, double g, double k)
{
    const double in = std::tanh (u - k * y[3]);
    y[0] += g * (in - t[0]);   t[0] = std::tanh (y[0]);
    y[1] += g * (t[0] - t[1]); t[1] = std::tanh (y[1]);
    y[2] += g * (t[1] - t[2]); t[2] = std::tanh (y[2]);
    y[3] += g * (t[2] - t[3]); t[3] = std::tanh (y[3]);
    return y[3];
}

double Ladder::process (double vin, double cutoffHz, double resonance)
{
    cutoffHz = std::clamp (cutoffHz, 0.5, std::min (20000.0, fsOs * 0.22));
    const double g = 1.0 - std::exp (-2.0 * 3.14159265358979 * cutoffHz / fsOs);
    const double k = 4.0 * std::clamp (resonance, 0.0, 1.0);
    const double u = vin / (2.0 * kVT); // coppia differenziale d'ingresso: tanh(V / 2VT)
    const double mid = 0.5 * (last + u);
    last = u;
    tick (mid, g, k);
    return tick (u, g, k);
}

// ======================================================================================
void Biquad::setLowpass (double f0, double Q, double sr)
{
    const double K = std::tan (3.14159265358979 * std::min (f0, sr * 0.49) / sr);
    const double norm = 1.0 / (1.0 + K / Q + K * K);
    b0 = K * K * norm; b1 = 2.0 * b0; b2 = b0;
    a1 = 2.0 * (K * K - 1.0) * norm;
    a2 = (1.0 - K / Q + K * K) * norm;
}
} // namespace perf::circuit
