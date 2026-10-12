// Modelli a livello di circuito ricavati dagli schemi di servizio (P550-P559).
// Valori dei componenti come da schema; dove un valore non è leggibile o dipende da un trimmer
// di taratura, la scelta è dichiarata nel commento.
#pragma once
#include <vector>
#include <array>
#include <cmath>

namespace perf::circuit
{
constexpr double kVT = 0.02585; // tensione termica a 25 °C

// ---------------------------------------------------------------------------------------
// Circuito lineare risolto con analisi nodale modificata (MNA).
// Condensatori discretizzati con la regola del trapezio (= trasformata bilineare),
// operazionali ideali (nullore). La matrice cambia solo quando cambia un potenziometro.
class LinearCircuit
{
public:
    int node() { return numNodes++; }
    void resistor (int a, int b, double R) { res.push_back ({ a, b, 1.0 / R }); }
    void capacitor (int a, int b, double C) { caps.push_back ({ a, b, C, 0.0, 0.0 }); }
    // operazionale ideale: l'equazione del nodo d'uscita diventa V+ - V- = 0
    void opamp (int out, int plus, int minus) { amps.push_back ({ out, plus, minus }); }
    // ingresso: sorgente di tensione esterna (indice src) collegata al nodo tramite R
    void input (int n, double R, int src) { ins.push_back ({ n, 1.0 / R, src }); }

    void setResistor (size_t index, double R) { res[index].g = 1.0 / R; dirty = true; }
    size_t numResistors() const { return res.size(); }

    void prepare (double sampleRate);
    void reset();
    void tick (const double* sources);
    double v (int n) const { return x[(size_t) n]; }

private:
    struct Res { int a, b; double g; };
    struct Cap { int a, b; double C, G, J; };
    struct Amp { int out, plus, minus; };
    struct In  { int n; double g; int src; };

    void build();
    void stamp (int a, int b, double g);

    int numNodes = 0;
    bool dirty = true;
    double T = 1.0 / 48000.0;
    std::vector<Res> res;
    std::vector<Cap> caps;
    std::vector<Amp> amps;
    std::vector<In> ins;
    std::vector<double> M, rhs, x;     // matrice LU (righe)
    std::vector<int> perm;
    std::vector<char> isAmpRow;
};

// ---------------------------------------------------------------------------------------
// Catena archi: miscelatore 8'/16' + preenfasi IC2 (P559), equalizzatore a giratori
// LOW/MIDDLE/HIGH (P559), passa-basso R22/C13 e filtri d'ingresso Sallen-Key delle tre
// linee BBD (P551, tre schede in parallelo sul nodo E).
class StringsChain
{
public:
    StringsChain();
    void prepare (double sr) { c.prepare (sr); }
    void reset() { c.reset(); }
    void setEq (double low, double mid, double high); // posizioni slider 0..1
    // v8, v16: uscite dei sommatori IC13 (volt); restituisce l'ingresso alle BBD (volt)
    double process (double v8, double v16)
    {
        const double s[2] = { v8, v16 };
        c.tick (s);
        return c.v (nOut);
    }
    double eqOut() const { return c.v (nEq); }

private:
    LinearCircuit c;
    int nOut = 0, nEq = 0;
    size_t rIdx[6] {}; // Ra/Rb delle tre bande
    double eq[3] { -1, -1, -1 };
};

// ---------------------------------------------------------------------------------------
// Inviluppo BRASS (P557): TR16, C19 2,2µ, TR15, C18 2,2µ (derivatore del gate comune),
// D1/D2, DECAY P1 470K log + R26 22K, ATTACK P5 470K log + R27 100K, C2 4,7µ,
// nodo di somma R28 10K / R32 3K3 / R31 220K (RANGE) / R33 100K (modulazione).
// Restituisce la tensione del nodo di somma (base di TR14), in volt.
class BrassEnvelope
{
public:
    void prepare (double sr) { dt = 1.0 / (sr * kSub); reset(); }
    void reset();
    double process (bool gate, double decaySlider, double attackSlider, double rangeSlider, double modVolts);
    double attackDelayMs() const { return 0.0; }
private:
    static constexpr int kSub = 4;
    double dt = 1.0 / 192000.0;
    double vC19 = 5.56, vC18 = 0.0, vC2 = 0.0, w = 0.0;
    bool sat = true, d1 = false, d2 = false;
};

// ---------------------------------------------------------------------------------------
// VCF a scala di transistor (TR1-TR10, 4 x 47n, P557), modello non lineare di Huovilainen,
// sovracampionato 2x. Ingresso in volt sulla base di TR10.
class Ladder
{
public:
    void prepare (double sr) { fsOs = sr * 2.0; reset(); }
    void reset() { y[0] = y[1] = y[2] = y[3] = 0.0; t[0] = t[1] = t[2] = t[3] = 0.0; last = 0.0; }
    double process (double vin, double cutoffHz, double resonance);
private:
    double tick (double u, double g, double k);
    double y[4] {}, t[4] {}, last = 0.0, fsOs = 96000.0;
};

// ---------------------------------------------------------------------------------------
struct OnePole // passa-basso del primo ordine, trasformata bilineare con prewarp
{
    double b = 0, a = 0, z = 0;
    void set (double fc, double sr)
    {
        const double k = std::tan (3.14159265358979 * fc / sr);
        b = k / (1.0 + k); a = (1.0 - k) / (1.0 + k);
    }
    double process (double x) { const double yv = b * x + z; z = b * x + a * yv; return yv; }
    void reset() { z = 0; }
};

struct Biquad // passa-basso del secondo ordine (Sallen-Key a guadagno unitario)
{
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void setLowpass (double f0, double Q, double sr);
    double process (double x)
    {
        const double yv = b0 * x + z1;
        z1 = b1 * x - a1 * yv + z2;
        z2 = b2 * x - a2 * yv;
        return yv;
    }
    void reset() { z1 = z2 = 0; }
};

// Potenziometri: curve tipiche delle piste logaritmiche (10% a metà corsa)
inline double logTaper (double s) { return (std::pow (10.0, 2.0 * s) - 1.0) / 99.0; }
} // namespace perf::circuit
