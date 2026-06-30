// =============================================================================
// network/network_ReliableUdp.cpp — Reliable UDP Implementation (AP-33)
// =============================================================================
#include "network_ReliableUdp.h"
#include "../core/Log.h"

// =============================================================================
// RTT-Schätzung nach Jacobson/Karels Algorithmus
// =============================================================================
void RttEstimator::Update(float measuredRtt) {
    // Clamp auf gültigen Bereich
    measuredRtt = std::clamp(measuredRtt, minRtt, maxRtt);

    if (smoothedRtt < 0.001f) {
        // Erste Messung
        smoothedRtt = measuredRtt;
        rttVariance = measuredRtt * 0.5f;
    } else {
        // Jacobson/Karels: alpha=0.125, beta=0.25
        const float alpha = 0.125f;
        const float beta  = 0.25f;

        float diff = measuredRtt - smoothedRtt;
        smoothedRtt += alpha * diff;
        rttVariance += beta * (std::abs(diff) - rttVariance);
    }

    AddLog("[Net] RTT aktualisiert: measured={:.3f}ms, smoothed={:.3f}ms, RTO={:.3f}ms",
           measuredRtt * 1000.0f, smoothedRtt * 1000.0f, GetRto() * 1000.0f);
}

float RttEstimator::GetRto() const {
    // RTO = SRTT + 4 * RTTVAR (RFC 6298)
    float rto = smoothedRtt + 4.0f * rttVariance;
    return std::clamp(rto, minRtt, maxRtt);
}
